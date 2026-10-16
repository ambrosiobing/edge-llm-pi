// main.cpp: project e12, the STWIN.box vibration band reporter. C++17, one process: the
// 6 kHz frame path from the probe's CDC port, the window and the FFT (bands.cpp), the
// control rule (rules_e12.c, through rule_find), the engine through common/cpp/edgellm.hpp
// and the per-event log through elog.h. The loop order is the shared main's: event, rule,
// act, user text, engine, log row, history push; the ChatML wrapping of the system prompt
// and the user text is reproduced here because this project owns its main().
//
//   edgellm-e12 --project e12 (--replay <csv> | --live serial <dev>:<baud> | --frames <txt>)
//               --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>] [--state <path>]
//               [--n-predict N] [--threads N] [--n-ctx N] [--baseline-windows N]
//               [--ask-every N] [--max-events N] [--template chatml|none] [--stub]
//
// Three sources, one event shape. --replay reads band-energy rows (t_s, tag, e0..e3, fs_hz),
// one per window, written by tools/make_replay.py; --frames reads a captured text file of
// frames through the parser and the FFT; --live reads the same frames from the CDC port
// through the shared serial backend. Whatever the source, the event the rule and the model
// see is:
//
//   v0..v3  the four band energies     v4 total     v5 share of B1 in the total
//   v6      the baseline share, 0.0 while the first --baseline-windows are being averaged
//   v7      the sample rate measured from the stream's timestamps (the replay's column)
//   v8      frames dropped inside the window (always 0: a window with a gap is discarded)
//
// The rule acts: its decision is the status line on the DSI panel (stdout) and the rule
// column of the log. The model's answer goes to the log and to the line below the status,
// labelled as the model's, and nowhere else.
#include "bands.hpp"
#include "stwin_frames.hpp"

#include "edgellm.hpp"
#include "elog.h"
#include "io.h"
#include "rules.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Options {
    std::string project = "e12", replay, live_spec, frames, grammar, prompt, log, model, state;
    std::string tmpl = "chatml";
    int  n_predict = 96, threads = 4, n_ctx = 512;
    int  baseline_windows = 50, ask_every = 1, max_events = 0;
    bool stub = false;
};

std::atomic<bool> stop_requested{ false };

void on_signal(int) { stop_requested = true; }

int usage(const char *argv0)
{
    std::fprintf(stderr,
        "usage: %s --project e12 (--replay <csv> | --live serial <dev>:<baud> | --frames <txt>)\n"
        "          --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>] [--state <path>]\n"
        "          [--n-predict N] [--threads N] [--n-ctx N] [--baseline-windows N]\n"
        "          [--ask-every N] [--max-events N] [--template chatml|none] [--stub]\n", argv0);
    return 2;
}

std::string read_text(const std::string &path)
{
    std::ifstream f(path);
    if (!f) return std::string();
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

// The ChatML form the shared main applies: SmolLM2 and Qwen2.5 are ChatML models, so the
// system prompt and the user turn carry the im_start and im_end markers and the answer
// begins after the assistant marker. prompt.txt stays plain text. "none" passes through.
std::string wrap_system(const std::string &tmpl, const std::string &prompt)
{
    if (tmpl == "none") return prompt;
    return "<|im_start|>system\n" + prompt + "<|im_end|>\n";
}

std::string wrap_user(const std::string &tmpl, const std::string &user)
{
    if (tmpl == "none") return user;
    return "<|im_start|>user\n" + user + "<|im_end|>\n<|im_start|>assistant\n";
}

// The user text per window: the four energies as shares, the watched share, its baseline
// and the ratio between them. The thresholds live in prompt.txt and cost no tokens here.
std::string format_prompt(const io_event &ev)
{
    char buf[320];
    const double ratio = ev.v[6] > 0.0 ? ev.v[5] / ev.v[6] : 0.0;
    std::snprintf(buf, sizeof buf,
                  "Band shares B0 %.3f, B1 %.3f, B2 %.3f, B3 %.3f. B1 share %.3f, baseline %.3f, "
                  "ratio %.2f. Sample rate %.0f Hz. Report.",
                  ev.v[4] > 0.0 ? ev.v[0] / ev.v[4] : 0.0, ev.v[4] > 0.0 ? ev.v[1] / ev.v[4] : 0.0,
                  ev.v[4] > 0.0 ? ev.v[2] / ev.v[4] : 0.0, ev.v[4] > 0.0 ? ev.v[3] / ev.v[4] : 0.0,
                  ev.v[5], ev.v[6], ratio, ev.v[7]);
    return buf;
}

io_event make_event(double t_s, const e12::BandEnergies &be, double baseline, double fs_hz)
{
    io_event ev;
    std::memset(&ev, 0, sizeof ev);
    ev.t_s = t_s;
    std::snprintf(ev.tag, sizeof ev.tag, "stwin-bands");
    ev.n = 9;
    for (int b = 0; b < e12::kBands; ++b) ev.v[b] = be.e[(size_t)b];
    ev.v[4] = be.total;
    ev.v[5] = be.share(e12::kWatchedBand);
    ev.v[6] = baseline;
    ev.v[7] = fs_hz;
    ev.v[8] = 0.0;
    return ev;
}

// What the loop carries across events.
struct Loop {
    const Options &o;
    const rule_desc *rule = nullptr;
    std::unique_ptr<edgellm::Engine> engine;
    elog *log = nullptr;
    rule_ctx ctx{};
    long n_events = 0, n_model = 0, n_model_errors = 0, n_rate_warnings = 0;
    double share_sum = 0.0, baseline = 0.0, sum_prompt_ms = 0.0, sum_generate_ms = 0.0;

    explicit Loop(const Options &opts) : o(opts) {}

    // The baseline over the first N windows: 0.0 while it is being taken, then fixed.
    double baseline_for(double share)
    {
        if (n_events < o.baseline_windows) {
            share_sum += share;
            if (n_events + 1 == o.baseline_windows) baseline = share_sum / o.baseline_windows;
            return 0.0;
        }
        return baseline;
    }

    // One window, in the shared main's order. Returns 0, or -1 on a log failure.
    int handle(io_event &ev, bool ask)
    {
        const int idx = rule->fn(&ctx, &ev);
        const int n_names = rule_name_count(rule);
        if (idx < 0 || idx >= n_names) {
            std::fprintf(stderr, "e12: rule returned %d outside its %d names\n", idx, n_names);
            return -1;
        }
        if (std::fabs(ev.v[7] - e12::kSampleRateHz) > 0.01 * e12::kSampleRateHz) ++n_rate_warnings;
        // The rule acts: the status line on the panel.
        std::printf("window %4ld  t %9.3f s  B1 share %.3f  baseline %.3f  fs %7.1f Hz  rule %s\n",
                    n_events + 1, ev.t_s, ev.v[5], ev.v[6], ev.v[7], rule->names[idx]);
        edgellm::Result res;
        edgellm_result raw;
        const edgellm_result *logged = nullptr;
        if (ask) {
            try {
                res = engine->decide(wrap_user(o.tmpl, format_prompt(ev)));
                ++n_model;
                sum_prompt_ms += res.prompt_ms;
                sum_generate_ms += res.generate_ms;
                std::printf("model: %s\n", res.text.c_str());
            } catch (const std::exception &e) {
                ++n_model_errors;
                std::fprintf(stderr, "e12: %s\n", e.what());
            }
            std::memset(&raw, 0, sizeof raw);
            std::snprintf(raw.text, sizeof raw.text, "%s", res.text.c_str());
            raw.n_prompt_tokens = res.n_prompt_tokens;
            raw.n_generated = res.n_generated;
            raw.prompt_ms = res.prompt_ms;
            raw.generate_ms = res.generate_ms;
            raw.stub = res.stub ? 1 : 0;
            logged = &raw;
        }
        if (elog_row(log, &ev, rule->names[idx], logged) != 0) {
            std::fprintf(stderr, "e12: log write failed\n");
            return -1;
        }
        rule_ctx_push(&ctx, &ev);
        ++n_events;
        std::fflush(stdout);
        return 0;
    }
};

// Completed windows cross from the reader thread to the loop through this queue, so the
// CDC port is drained at 6 kHz while the engine takes its seconds. A queue longer than
// kQueueCap drops its oldest window and counts it.
struct WindowQueue {
    static constexpr size_t kQueueCap = 64;
    std::mutex m;
    std::condition_variable cv;
    std::deque<e12::Window> q;
    bool done = false;
    unsigned long overflow = 0;

    void push(e12::Window &&w)
    {
        std::lock_guard<std::mutex> lk(m);
        if (q.size() >= kQueueCap) { q.pop_front(); ++overflow; }
        q.push_back(std::move(w));
        cv.notify_one();
    }
    void finish()
    {
        std::lock_guard<std::mutex> lk(m);
        done = true;
        cv.notify_one();
    }
    // Returns 1 with a window, 0 when the reader is done and the queue empty. backlog is
    // how many windows are still waiting after this one.
    int pop(e12::Window &w, size_t &backlog)
    {
        std::unique_lock<std::mutex> lk(m);
        while (q.empty() && !done && !stop_requested)
            cv.wait_for(lk, std::chrono::milliseconds(200));
        if (q.empty()) return 0;
        w = std::move(q.front());
        q.pop_front();
        backlog = q.size();
        return 1;
    }
};

int run_replay(Loop &L)
{
    char err[256];
    io_source *src = io_open_replay(L.o.replay.c_str(), err, sizeof err);
    if (src == nullptr) {
        std::fprintf(stderr, "e12: %s\n", err);
        return 1;
    }
    io_event row;
    int r, rc = 0;
    while (!stop_requested && (r = io_next(src, &row)) == 1) {
        if (row.n < 5) {
            std::fprintf(stderr, "e12: replay row needs e0..e3 and fs_hz\n");
            rc = 1;
            break;
        }
        e12::BandEnergies be;
        for (int b = 0; b < e12::kBands; ++b) { be.e[(size_t)b] = row.v[b]; be.total += row.v[b]; }
        const double baseline = L.baseline_for(be.share(e12::kWatchedBand));
        io_event ev = make_event(row.t_s, be, baseline, row.v[4]);
        if (L.handle(ev, L.n_events % L.o.ask_every == 0) != 0) { rc = 1; break; }
        if (L.o.max_events > 0 && L.n_events >= L.o.max_events) break;
    }
    io_close(src);
    return rc;
}

int run_windows(Loop &L)
{
    WindowQueue queue;
    e12::WindowBuilder builder;
    unsigned long bad_lines = 0, windows_with_gaps = 0;
    char err[256];
    io_source *src = nullptr;
    if (!L.o.live_spec.empty()) {
        src = io_open_live("serial", L.o.live_spec.c_str(), err, sizeof err);
        if (src == nullptr) {
            std::fprintf(stderr, "e12: %s\n", err);
            return 1;
        }
    }
    std::thread reader([&]() {
        e12::Frame f;
        e12::Window w;
        if (src != nullptr) {
            io_event ev;
            while (!stop_requested && io_next(src, &ev) == 1) {
                if (!e12::frame_from_event(ev, f)) { ++bad_lines; continue; }
                if (builder.push(f, w)) queue.push(std::move(w));
            }
        } else {
            std::ifstream in(L.o.frames);
            std::string line;
            while (!stop_requested && std::getline(in, line)) {
                if (!e12::parse_frame_line(line.c_str(), f)) { ++bad_lines; continue; }
                if (builder.push(f, w)) queue.push(std::move(w));
            }
        }
        queue.finish();
    });
    int rc = 0;
    e12::Window w;
    size_t backlog = 0;
    while (!stop_requested && queue.pop(w, backlog) == 1) {
        if (w.dropped > 0) {
            ++windows_with_gaps;
            std::printf("window discarded: %d frame(s) missing between seq %lu and %lu\n",
                        w.dropped, w.first_seq, w.last_seq);
            continue;
        }
        const double fs = w.fs_hz();
        e12::BandEnergies be = e12::window_energies(w.ax, w.ay, w.az, fs);
        const double baseline = L.baseline_for(be.share(e12::kWatchedBand));
        io_event ev = make_event(w.t0_us * 1e-6, be, baseline, fs);
        // The model is asked only when no newer window is waiting, so the panel stays
        // current; the windows that arrived while it answered get rule-only rows.
        const bool ask = backlog == 0 && L.n_events % L.o.ask_every == 0;
        if (L.handle(ev, ask) != 0) { rc = 1; break; }
        if (L.o.max_events > 0 && L.n_events >= L.o.max_events) break;
    }
    std::fprintf(stderr, "e12: %lu frames, %lu dropped, %lu windows with gaps discarded, "
                         "%lu unparsed lines, %lu windows lost to backlog\n",
                 builder.frames(), builder.dropped_total(), windows_with_gaps, bad_lines,
                 queue.overflow);
    if (src == nullptr) {
        reader.join();
    } else {
        // The reader may be blocked in read() on the port; the log is flushed per row, so
        // the process can end without it. stop_requested lets it leave if it wakes.
        stop_requested = true;
        reader.detach();
    }
    return rc;
}

} // namespace

int main(int argc, char **argv)
{
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--stub") { o.stub = true; continue; }
        if (i + 1 >= argc) return usage(argv[0]);
        const std::string v = argv[i + 1];
        if (a == "--project") o.project = v;
        else if (a == "--replay") o.replay = v;
        else if (a == "--frames") o.frames = v;
        else if (a == "--live") {
            if (i + 2 >= argc || v != "serial") return usage(argv[0]);
            o.live_spec = argv[i + 2];
            ++i;
        }
        else if (a == "--grammar") o.grammar = v;
        else if (a == "--prompt") o.prompt = v;
        else if (a == "--log") o.log = v;
        else if (a == "--model") o.model = v;
        else if (a == "--state") o.state = v;
        else if (a == "--n-predict") o.n_predict = std::atoi(v.c_str());
        else if (a == "--threads") o.threads = std::atoi(v.c_str());
        else if (a == "--n-ctx") o.n_ctx = std::atoi(v.c_str());
        else if (a == "--baseline-windows") o.baseline_windows = std::atoi(v.c_str());
        else if (a == "--ask-every") o.ask_every = std::atoi(v.c_str());
        else if (a == "--max-events") o.max_events = std::atoi(v.c_str());
        else if (a == "--template") o.tmpl = v;
        else return usage(argv[0]);
        ++i;
    }
    const int sources = (o.replay.empty() ? 0 : 1) + (o.live_spec.empty() ? 0 : 1) +
                        (o.frames.empty() ? 0 : 1);
    if (sources != 1 || o.grammar.empty() || o.prompt.empty() || o.log.empty() ||
        (o.tmpl != "chatml" && o.tmpl != "none") || o.baseline_windows < 1 || o.ask_every < 1)
        return usage(argv[0]);
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
    Loop L(o);
    L.rule = rule_find(o.project.c_str());
    if (L.rule == nullptr) {
        std::fprintf(stderr, "%s: no rule registered for project %s\n", argv[0],
                     o.project.c_str());
        return 1;
    }
    const std::string plain = read_text(o.prompt);
    if (plain.empty()) {
        std::fprintf(stderr, "%s: cannot read %s\n", argv[0], o.prompt.c_str());
        return 1;
    }
    edgellm::Config cfg;
    cfg.model = o.model;
    cfg.grammar = o.grammar;
    cfg.system_prompt = wrap_system(o.tmpl, plain);
    cfg.state = o.state;
    cfg.n_ctx = o.n_ctx;
    cfg.n_threads = o.threads;
    cfg.n_predict = o.n_predict;
    cfg.temperature = 0.0f;                       // greedy: the enum and the report together
    try {
        L.engine = std::make_unique<edgellm::Engine>(cfg);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return 1;
    }
    L.log = elog_open(o.log.c_str(), edgellm::Engine::version());
    if (L.log == nullptr) {
        std::fprintf(stderr, "%s: cannot write %s\n", argv[0], o.log.c_str());
        return 1;
    }
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::printf("e12: fft %s, window %d samples, expected %.0f Hz, baseline over %d windows, "
                "engine %s\n", e12::fft_backend(), e12::kWindow, e12::kSampleRateHz,
                o.baseline_windows, edgellm::Engine::version());

    const int rc = o.replay.empty() ? run_windows(L) : run_replay(L);

    std::fprintf(stderr, "%s: %ld events, %ld model answers, %ld model errors, %ld windows off "
                         "the expected rate by more than 1 percent, mean prompt %.1f ms, mean "
                         "generate %.1f ms, rss %ld kB, engine %s\n",
                 o.project.c_str(), L.n_events, L.n_model, L.n_model_errors, L.n_rate_warnings,
                 L.n_model > 0 ? L.sum_prompt_ms / (double)L.n_model : 0.0,
                 L.n_model > 0 ? L.sum_generate_ms / (double)L.n_model : 0.0,
                 edgellm::Engine::rss_kb(), edgellm::Engine::version());
    elog_close(L.log);
    return rc;
}
