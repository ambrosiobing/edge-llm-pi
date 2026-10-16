// main.cpp: project e07, the IKS4A1 climate and motion narrator. C++17 over the C API
// through common/cpp/edgellm.hpp (catalogue variant B); nothing here includes llama.h.
//
//   edgellm-e07 --project e07 (--replay <csv> | --live iio <sysfs iio root>)
//               --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>]
//               [--every N] [--interval S] [--n-predict N] [--threads N] [--n-ctx N]
//               [--temperature T] [--state <path>] [--max-events N] [--stub]
//
// The loop keeps the shared main's order per event: the rule on the event, the log
// row, the history push. The model is asked every --every events (60 by default, one
// summary an hour at a 60 s interval) and its answer reaches only the log. The sensors
// are reached through io.h: one "iio" source per IIO device, bound by its name
// attribute and never by its iio:deviceN number, and one "sysfs" source per hwmon
// attribute of the sht4x, all found with std::filesystem. --live takes the backend
// name "iio" and the sysfs root of the IIO bus, so the command line keeps the shared
// main's shape; --stub is accepted only by a binary configured with
// -DEDGELLM_STUB_MODEL=ON, as in edgellm_main.c.
//
// Event values, fixed here and reproduced by tools/make_replay.py:
//   v0 pressure hPa (LPS22DF)          v1 pressure 30 minutes earlier, hPa
//   v2 temperature C (sht4x)           v3 relative humidity percent (sht4x)
//   v4 acceleration magnitude g        v5 rotation rate magnitude deg/s (LSM6DSV16X)
//   v6 magnetic field magnitude uT     v7, v8 the same two magnitudes from the
//                                        LSM6DSO16IS, logged as a witness, not prompted
#include "edgellm.hpp"
#include "elog.h"
#include "io.h"
#include "rules.h"

#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

volatile std::sig_atomic_t stop_requested = 0;
void on_signal(int) { stop_requested = 1; }

constexpr double G_MS2 = 9.80665;             // IIO accel is m/s^2; the prompt says g
constexpr double RAD_TO_DEG = 57.29577951308232;
constexpr double GAUSS_TO_UT = 100.0;         // IIO magn is Gauss; the prompt says uT
constexpr int RING = 31;                      // 30 minutes of history plus now, at 60 s

struct Options {
    std::string project, replay, backend, spec, grammar, prompt, log, model, state;
    int every = 60, max_events = 0, n_predict = 64, threads = 4, n_ctx = 512;
    double interval = 0.0, temperature = 0.3;
    bool stub = false;
};

int usage(const char *argv0)
{
    std::fprintf(stderr,
                 "usage: %s --project e07 (--replay <csv> | --live iio <iio sysfs root>)\n"
                 "          --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>]\n"
                 "          [--every N] [--interval S] [--n-predict N] [--threads N]\n"
                 "          [--n-ctx N] [--temperature T] [--state <path>] [--max-events N]\n"
                 "          [--stub]\n", argv0);
    return 2;
}

bool parse(int argc, char **argv, Options &o)
{
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--stub") { o.stub = true; continue; }
        if (i + 1 >= argc) return false;
        std::string v = argv[++i];
        if (a == "--project") o.project = v;
        else if (a == "--replay") o.replay = v;
        else if (a == "--live") {
            if (i + 1 >= argc) return false;
            o.backend = v;
            o.spec = argv[++i];
        }
        else if (a == "--grammar") o.grammar = v;
        else if (a == "--prompt") o.prompt = v;
        else if (a == "--log") o.log = v;
        else if (a == "--model") o.model = v;
        else if (a == "--state") o.state = v;
        else if (a == "--every") o.every = std::atoi(v.c_str());
        else if (a == "--interval") o.interval = std::atof(v.c_str());
        else if (a == "--n-predict") o.n_predict = std::atoi(v.c_str());
        else if (a == "--threads") o.threads = std::atoi(v.c_str());
        else if (a == "--n-ctx") o.n_ctx = std::atoi(v.c_str());
        else if (a == "--temperature") o.temperature = std::atof(v.c_str());
        else if (a == "--max-events") o.max_events = std::atoi(v.c_str());
        else return false;
    }
    bool one_source = o.replay.empty() != o.backend.empty();
    return !o.project.empty() && !o.grammar.empty() && !o.prompt.empty() &&
           !o.log.empty() && one_source && o.every > 0;
}

std::string slurp(const std::string &path)
{
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot read " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

// The ChatML markers. The shared main wraps every C project's prompt this way; a
// project with its own main does it itself. Qwen2.5 and SmolLM2 are ChatML models, and
// prompt.txt stays plain text with no marker in it.
std::string wrap_system(const std::string &p)
{
    return "<|im_start|>system\n" + p + "<|im_end|>\n";
}
std::string wrap_user(const std::string &u)
{
    return "<|im_start|>user\n" + u + "<|im_end|>\n<|im_start|>assistant\n";
}

// The user text per event. The thresholds live in the system prompt, so they cost no
// tokens here; the change is given as a number so the question is whether a 0.5B model
// can compare it with 1, not whether it can subtract.
std::string format_prompt(const io_event &ev)
{
    char buf[320];
    std::snprintf(buf, sizeof buf,
                  "pressure %.1f hPa, 30 minutes ago %.1f hPa, change %+.1f hPa, "
                  "temperature %.1f C, humidity %.0f percent, acceleration %.2f g, "
                  "rotation %.1f deg per second, field %.0f uT. Decide and summarise.",
                  ev.v[0], ev.v[1], ev.v[0] - ev.v[1], ev.v[2], ev.v[3], ev.v[4], ev.v[5],
                  ev.v[6]);
    return buf;
}

double magnitude(const io_event &ev)
{
    if (ev.n < 3) return 0.0;
    return std::sqrt(ev.v[0] * ev.v[0] + ev.v[1] * ev.v[1] + ev.v[2] * ev.v[2]);
}

// The six IIO devices and the one hwmon device of the shield, each behind io.h. The
// device directory is found by reading every name attribute under the root, because
// iio:deviceN follows probe order and changes between boots; a missing name is an
// error at open, never a silent zero at read.
class SensorSet {
public:
    bool open(const std::string &iio_root, double interval_s, std::string &err)
    {
        const char *names[] = { "lps22df", "lsm6dsv16x_accel", "lsm6dsv16x_gyro", "lis2mdl",
                                "lsm6dso16is_accel", "lsm6dso16is_gyro" };
        for (const char *n : names) {
            std::string dir = find_by_name(iio_root, "iio:device", n);
            if (dir.empty()) { err = std::string("no IIO device named ") + n; return false; }
            std::string spec = dir + ":" + std::to_string(interval_s);
            char e[256] = { 0 };
            io_source *s = io_open_live("iio", spec.c_str(), e, sizeof e);
            if (s == nullptr) { err = e; return false; }
            iio_.push_back(s);
            dirs_.push_back(dir);
        }
        std::string hw = find_by_name("/sys/class/hwmon", "hwmon", "sht4x");
        if (hw.empty()) { err = "no hwmon device named sht4x"; return false; }
        for (const char *attr : { "temp1_input", "humidity1_input" }) {
            std::string spec = hw + "/" + attr + ":" + std::to_string(interval_s);
            char e[256] = { 0 };
            io_source *s = io_open_live("sysfs", spec.c_str(), e, sizeof e);
            if (s == nullptr) { err = e; return false; }
            hwmon_.push_back(s);
        }
        dirs_.push_back(hw);
        return true;
    }

    // One merged event. Every source is read once, in the order it was opened; the
    // first source carries the 60 s wait and the others are due by then.
    bool read(io_event &ev)
    {
        io_event part[8];
        for (size_t i = 0; i < iio_.size(); ++i)
            if (io_next(iio_[i], &part[i]) != 1) return false;
        for (size_t i = 0; i < hwmon_.size(); ++i)
            if (io_next(hwmon_[i], &part[6 + i]) != 1) return false;
        std::memset(&ev, 0, sizeof ev);
        ev.t_s = part[0].t_s;
        std::snprintf(ev.tag, sizeof ev.tag, "%s", "iks4a1");
        double p_hpa = part[0].v[0] * 10.0;               // IIO pressure is kPa
        ring_.push_back(p_hpa);
        if ((int)ring_.size() > RING) ring_.pop_front();
        ev.v[0] = p_hpa;
        ev.v[1] = ring_.front();                          // 30 minutes ago, or the oldest
        ev.v[2] = part[6].v[0] / 1000.0;                  // hwmon temp1_input, milli C
        ev.v[3] = part[7].v[0] / 1000.0;                  // hwmon humidity1_input, milli %
        ev.v[4] = magnitude(part[1]) / G_MS2;
        ev.v[5] = magnitude(part[2]) * RAD_TO_DEG;
        ev.v[6] = magnitude(part[3]) * GAUSS_TO_UT;
        ev.v[7] = magnitude(part[4]) / G_MS2;
        ev.v[8] = magnitude(part[5]) * RAD_TO_DEG;
        ev.n = 9;
        return true;
    }

    void close()
    {
        for (io_source *s : iio_) io_close(s);
        for (io_source *s : hwmon_) io_close(s);
        iio_.clear();
        hwmon_.clear();
    }

    const std::vector<std::string> &dirs() const { return dirs_; }

private:
    static std::string find_by_name(const std::string &root, const std::string &prefix,
                                    const std::string &want)
    {
        std::error_code ec;
        for (const auto &entry : fs::directory_iterator(root, ec)) {
            std::string base = entry.path().filename().string();
            if (base.compare(0, prefix.size(), prefix) != 0) continue;
            std::ifstream f(entry.path() / "name");
            std::string name;
            if (f && std::getline(f, name) && name == want) return entry.path().string();
        }
        return "";
    }

    std::vector<io_source *> iio_, hwmon_;
    std::vector<std::string> dirs_;
    std::deque<double> ring_;
};

void sleep_s(double s)
{
    if (s <= 0.0) return;
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, nullptr);
}

} // namespace

int main(int argc, char **argv)
{
    Options o;
    if (!parse(argc, argv, o)) return usage(argv[0]);
#ifndef EDGELLM_STUB_MODEL
    if (o.stub) {
        std::fprintf(stderr, "%s: built with the real engine; configure with "
                             "-DEDGELLM_STUB_MODEL=ON for --stub\n", argv[0]);
        return 2;
    }
    if (o.model.empty()) {
        std::fprintf(stderr, "%s: --model is required with the real engine\n", argv[0]);
        return 2;
    }
#endif
    const rule_desc *rule = rule_find(o.project.c_str());
    if (rule == nullptr) {
        std::fprintf(stderr, "%s: no rule registered for project %s\n", argv[0],
                     o.project.c_str());
        return 1;
    }
    if (!o.backend.empty() && o.backend != "iio") {
        std::fprintf(stderr, "%s: the live backend of e07 is iio, got %s\n", argv[0],
                     o.backend.c_str());
        return 2;
    }

    edgellm::Config cfg;
    io_source *replay = nullptr;
    SensorSet sensors;
    elog *log = nullptr;
    int rc = 0;
    try {
        cfg.model = o.model;
        cfg.grammar = o.grammar;
        cfg.system_prompt = wrap_system(slurp(o.prompt));
        cfg.state = o.state;
        cfg.n_ctx = o.n_ctx;
        cfg.n_threads = o.threads;
        cfg.n_predict = o.n_predict;
        cfg.temperature = (float)o.temperature;
        edgellm::Engine engine(cfg);

        std::string err;
        if (!o.replay.empty()) {
            char e[256] = { 0 };
            replay = io_open_replay(o.replay.c_str(), e, sizeof e);
            if (replay == nullptr) throw std::runtime_error(e);
        } else if (!sensors.open(o.spec, o.interval > 0.0 ? o.interval : 60.0, err)) {
            throw std::runtime_error(err);
        }
        for (const std::string &d : sensors.dirs())
            std::fprintf(stderr, "e07: bound %s\n", d.c_str());

        log = elog_open(o.log.c_str(), edgellm::Engine::version());
        if (log == nullptr) throw std::runtime_error("cannot write " + o.log);

        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);
        rule_ctx ctx;
        std::memset(&ctx, 0, sizeof ctx);
        long n_events = 0, n_model = 0, n_model_errors = 0, n_free = 0;
        double sum_prompt_ms = 0.0, sum_generate_ms = 0.0;
        const int n_names = rule_name_count(rule);

        // The loop: rule, log, push. The model every o.every events, starting with the
        // first, so a 1440-row day gives 24 answers; its answer goes to the log only.
        while (!stop_requested) {
            io_event ev;
            if (replay != nullptr) {
                int r = io_next(replay, &ev);
                if (r == 0) break;
                if (r < 0) { rc = 1; break; }
            } else if (!sensors.read(ev)) {
                rc = 1;
                break;
            }
            int idx = rule->fn(&ctx, &ev);
            if (idx < 0 || idx >= n_names) {
                std::fprintf(stderr, "%s: rule returned %d outside its %d names\n", argv[0],
                             idx, n_names);
                rc = 1;
                break;
            }
            edgellm_result res;
            const edgellm_result *logged = nullptr;
            if (n_events % o.every == 0) {
                std::memset(&res, 0, sizeof res);
                try {
                    edgellm::Result r = engine.decide(wrap_user(format_prompt(ev)));
                    std::snprintf(res.text, sizeof res.text, "%s", r.text.c_str());
                    res.n_prompt_tokens = r.n_prompt_tokens;
                    res.n_generated = r.n_generated;
                    res.prompt_ms = r.prompt_ms;
                    res.generate_ms = r.generate_ms;
                    res.stub = r.stub ? 1 : 0;
                    n_model++;
                    sum_prompt_ms += r.prompt_ms;
                    sum_generate_ms += r.generate_ms;
                    if (r.text.find("\"trend\"") == std::string::npos) n_free++;
                } catch (const std::exception &e) {
                    std::fprintf(stderr, "e07: decision failed: %s\n", e.what());
                    n_model_errors++;
                }
                logged = &res;
            }
            if (elog_row(log, &ev, rule->names[idx], logged) != 0) {
                std::fprintf(stderr, "%s: log write failed\n", argv[0]);
                rc = 1;
                break;
            }
            rule_ctx_push(&ctx, &ev);
            n_events++;
            if (o.max_events > 0 && n_events >= o.max_events) break;
            if (replay != nullptr && o.interval > 0.0) sleep_s(o.interval);
        }
        std::fprintf(stderr, "e07: %ld events, %ld model answers, %ld model errors, %ld "
                             "answers without a trend key, mean prompt %.1f ms, mean generate "
                             "%.1f ms, rss %ld kB, engine %s, source %s\n",
                     n_events, n_model, n_model_errors, n_free,
                     n_model > 0 ? sum_prompt_ms / (double)n_model : 0.0,
                     n_model > 0 ? sum_generate_ms / (double)n_model : 0.0,
                     edgellm::Engine::rss_kb(), edgellm::Engine::version(),
                     replay != nullptr ? "replay" : "iio");
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s: %s\n", argv[0], e.what());
        rc = 1;
    }
    elog_close(log);
    io_close(replay);
    sensors.close();
    return rc;
}
