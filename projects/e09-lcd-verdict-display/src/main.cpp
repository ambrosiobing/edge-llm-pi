// main.cpp: project e09, the LCD prompt and verdict display. One process on the
// Raspberry Pi 3 Model B Rev 1.2: the ADXL345 through the IIO backend of io.h, e05's
// rule in rules_e09.c, SmolLM2-360M through common/cpp/edgellm.hpp, three LEDs through
// a second io.h source on the gpio backend, and the 3.5 inch panel through panel.cpp.
//
// The loop is the shared main's loop (common/c/src/edgellm_main.c), reproduced here
// because the shared main has no hook between the rule and the model and this project
// draws a frame at exactly that point: open the source, find the rule, then for every
// event run the rule on the history (the event is not yet in it), act through
// io_actuate with the rule's index and never with the model's answer, draw the prompt
// and the verdict, ask the model only when the verdict differs from the previous
// event's (--ask change, the default) or on every event (--ask always), draw the
// answer, log the row in the shape elog.h fixes, push the event. The ChatML wrapping
// of the system prompt and the user text is the shared main's, line for line.
//
//   edgellm-e09 --project e09 (--replay <csv> | --live iio <dir[:interval]>)
//               --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>]
//               [--leds <gpio spec>] [--card /dev/dri/cardN] [--touch /dev/input/eventN]
//               [--panel-log <csv>] [--g-per-unit F] [--ask change|always]
//               [--n-predict N] [--threads N] [--n-ctx N] [--temperature T]
//               [--interval S] [--state <path>] [--max-events N] [--stub] [--headless]
//               [--template chatml|zephyr|none]
//
// --headless skips the panel and the touch controller, which is how the oracle test
// runs in CI. --stub is accepted only by a binary configured with
// -DEDGELLM_STUB_MODEL=ON, as in the shared main, so a log never claims an engine it
// did not run. --g-per-unit converts the backend's unit to g before the rule and the
// prompt see a value: the ADXL345 IIO driver reports m/s^2, so the bench run passes
// 0.1019716 (one over 9.80665); a replay file is already in g and keeps the default 1.
//
// The panel log (--panel-log) is the acceptance record: one row per frame that
// followed a decision, with the milliseconds from the decision's return to the page
// flip's event, and one row per touch that toggled the enlarged pane.
#include "edgellm.hpp"
#include "elog.h"
#include "io.h"
#include "panel.hpp"
#include "rules.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

#include <signal.h>
#include <time.h>

namespace {

volatile std::sig_atomic_t stop_requested = 0;

void on_signal(int) { stop_requested = 1; }

double ms_now()
{
    using clk = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clk::now().time_since_epoch()).count();
}

struct Options {
    std::string project, replay, backend, spec, grammar, prompt, log, model, state;
    std::string leds, card, touch, panel_log, tmpl = "chatml", ask = "change";
    double g_per_unit = 1.0, interval = 0.0;
    int max_events = 0, n_predict = 8, threads = 4, n_ctx = 512;
    float temperature = 0.0f;
    bool stub = false, headless = false;
};

int usage(const char *argv0)
{
    std::fprintf(stderr,
                 "usage: %s --project e09 (--replay <csv> | --live <backend> <spec>)\n"
                 "          --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>]\n"
                 "          [--leds <gpio spec>] [--card <dri node>] [--touch <event node>]\n"
                 "          [--panel-log <csv>] [--g-per-unit F] [--ask change|always]\n"
                 "          [--n-predict N] [--threads N] [--n-ctx N] [--temperature T]\n"
                 "          [--interval S] [--state <path>] [--max-events N] [--stub]\n"
                 "          [--headless] [--template chatml|zephyr|none]\n", argv0);
    return 2;
}

bool parse(int argc, char **argv, Options &o)
{
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--stub") { o.stub = true; continue; }
        if (a == "--headless") { o.headless = true; continue; }
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
        else if (a == "--leds") o.leds = v;
        else if (a == "--card") o.card = v;
        else if (a == "--touch") o.touch = v;
        else if (a == "--panel-log") o.panel_log = v;
        else if (a == "--template") o.tmpl = v;
        else if (a == "--ask") o.ask = v;
        else if (a == "--g-per-unit") o.g_per_unit = std::atof(v.c_str());
        else if (a == "--interval") o.interval = std::atof(v.c_str());
        else if (a == "--max-events") o.max_events = std::atoi(v.c_str());
        else if (a == "--n-predict") o.n_predict = std::atoi(v.c_str());
        else if (a == "--threads") o.threads = std::atoi(v.c_str());
        else if (a == "--n-ctx") o.n_ctx = std::atoi(v.c_str());
        else if (a == "--temperature") o.temperature = (float)std::atof(v.c_str());
        else return false;
    }
    if (o.tmpl != "chatml" && o.tmpl != "zephyr" && o.tmpl != "none") return false;
    if (o.ask != "change" && o.ask != "always") return false;
    if (o.project.empty() || o.grammar.empty() || o.prompt.empty() || o.log.empty())
        return false;
    return (o.replay.empty()) != (o.backend.empty());
}

std::string read_text(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read " + path);
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

// The chat template of the shared main: SmolLM2 and Qwen2.5 are ChatML models, so the
// system prompt and the user turn carry the im_start and im_end markers and the answer
// begins after the assistant marker. prompt.txt stays plain text.
std::string wrap_system(const std::string &plain, const std::string &tmpl)
{
    if (tmpl == "zephyr") return "<|system|>\n" + plain + "</s>\n";
    if (tmpl == "none") return plain;
    return "<|im_start|>system\n" + plain + "<|im_end|>\n";
}

std::string wrap_user(const std::string &user, const std::string &tmpl)
{
    if (tmpl == "zephyr") return "<|user|>\n" + user + "</s>\n<|assistant|>\n";
    if (tmpl == "none") return user;
    return "<|im_start|>user\n" + user + "<|im_end|>\n<|im_start|>assistant\n";
}

double magnitude(const io_event &ev)
{
    return std::sqrt(ev.v[0] * ev.v[0] + ev.v[1] * ev.v[1] + ev.v[2] * ev.v[2]);
}

// The 16-number summary the model sees: x, y, z and the magnitude now (4), the peak
// and the minimum magnitude over the window and the current reading (2), the tilt in
// degrees against the oldest reading (1), and the three previous readings (9). The
// thresholds are in the system prompt, so they cost no tokens per event. The current
// event is not yet in the history when this is called.
std::string format_prompt(const io_event &ev, const rule_ctx &ctx)
{
    double now = magnitude(ev), peak = now, low = now, tilt = 0.0;
    for (int i = 0; i < ctx.n; ++i) {
        double m = magnitude(ctx.hist[i]);
        peak = std::max(peak, m);
        low = std::min(low, m);
    }
    if (ctx.n > 0) {
        const io_event &ref = ctx.hist[0];
        double r = magnitude(ref);
        if (now > 1e-6 && r > 1e-6) {
            double c = (ev.v[0] * ref.v[0] + ev.v[1] * ref.v[1] + ev.v[2] * ref.v[2]) / (now * r);
            c = std::max(-1.0, std::min(1.0, c));
            tilt = std::acos(c) * 57.29577951308232;
        }
    }
    char buf[512];
    int k = std::snprintf(buf, sizeof buf,
                          "x %.2f y %.2f z %.2f g, magnitude %.2f, peak %.2f, minimum %.2f "
                          "over 1.6 s, tilt %.0f degrees, previous x y z:",
                          ev.v[0], ev.v[1], ev.v[2], now, peak, low, tilt);
    for (int i = (ctx.n > 3 ? ctx.n - 3 : 0); i < ctx.n && k < (int)sizeof buf - 40; ++i)
        k += std::snprintf(buf + k, sizeof buf - (size_t)k, "%s %.2f %.2f %.2f",
                           i > (ctx.n > 3 ? ctx.n - 3 : 0) ? ";" : "",
                           ctx.hist[i].v[0], ctx.hist[i].v[1], ctx.hist[i].v[2]);
    if (ctx.n == 0 && k < (int)sizeof buf - 8)
        k += std::snprintf(buf + k, sizeof buf - (size_t)k, " none");
    std::snprintf(buf + k, sizeof buf - (size_t)k, ". Decide.");
    return buf;
}

// The rule's index drives the three LEDs: green for NONE, yellow for TILT, red for TAP
// or FREEFALL. The model's answer never reaches this function.
int actuate(io_source *out, int idx)
{
    int rc = 0;
    rc |= io_actuate(out, "led:0", idx == 0);
    rc |= io_actuate(out, "led:1", idx == 2);
    rc |= io_actuate(out, "led:2", idx == 1 || idx == 3);
    return rc;
}

struct Display {
    std::unique_ptr<e09::Panel> panel;
    std::unique_ptr<e09::Touch> touch;
    e09::Pane panes[3] = { { "PROMPT", "", e09::rgb565(230, 230, 230) },
                           { "MODEL", "", e09::rgb565(255, 229, 102) },
                           { "RULE", "", e09::rgb565(126, 226, 168) } };
    int enlarged = 2;                       // the verdict pane, until a touch says otherwise

    // Returns the flip time in ms, or -1 without a panel.
    double show()
    {
        if (!panel) return -1.0;
        panel->compose(panes, enlarged);
        return panel->flip();
    }
};

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
    const int n_names = rule_name_count(rule);

    io_source *src = nullptr, *leds = nullptr;
    elog *log = nullptr;
    int rc = 0;
    try {
        edgellm::Config cfg;
        cfg.model = o.model;
        cfg.grammar = o.grammar;
        cfg.system_prompt = wrap_system(read_text(o.prompt), o.tmpl);
        cfg.state = o.state;
        cfg.n_ctx = o.n_ctx;
        cfg.n_threads = o.threads;
        cfg.n_predict = o.n_predict;
        cfg.temperature = o.temperature;
        edgellm::Engine engine(cfg);

        char err[256] = { 0 };
        src = o.replay.empty() ? io_open_live(o.backend.c_str(), o.spec.c_str(), err, sizeof err)
                               : io_open_replay(o.replay.c_str(), err, sizeof err);
        if (src == nullptr) throw std::runtime_error(err);
        if (!o.leds.empty()) {
            leds = io_open_live("gpio", o.leds.c_str(), err, sizeof err);
            if (leds == nullptr) throw std::runtime_error(err);
        }
        io_source *out = leds != nullptr ? leds : src;

        log = elog_open(o.log.c_str(), edgellm::Engine::version());
        if (log == nullptr) throw std::runtime_error("cannot write " + o.log);
        std::ofstream plog;
        if (!o.panel_log.empty()) {
            plog.open(o.panel_log);
            if (!plog) throw std::runtime_error("cannot write " + o.panel_log);
            plog << "t_s,kind,event,rule,pane,flip_ms\n";
        }

        Display d;
        if (!o.headless) {
            d.panel = std::make_unique<e09::Panel>(o.card);
            d.touch = std::make_unique<e09::Touch>(o.touch);
            std::fprintf(stderr, "e09: panel %s %d by %d, touch %s (%s)\n",
                         d.panel->card().c_str(), d.panel->width(), d.panel->height(),
                         d.touch->node().c_str(), d.touch->name().c_str());
        }

        struct sigaction sa;
        std::memset(&sa, 0, sizeof sa);
        sa.sa_handler = on_signal;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        rule_ctx ctx;
        std::memset(&ctx, 0, sizeof ctx);
        io_event ev;
        int prev = 0, r = 0;
        long n_events = 0, n_model = 0, n_model_errors = 0, n_act_errors = 0, n_touch = 0;
        double sum_prompt = 0.0, sum_generate = 0.0, worst_flip = 0.0;

        while (!stop_requested) {
            int presses = d.touch ? d.touch->poll() : 0;
            for (int p = 0; p < presses; ++p) {
                d.enlarged = (d.enlarged + 1) % 3;
                double ms = d.show();
                ++n_touch;
                if (plog) plog << ms_now() / 1000.0 << ",touch," << n_events << ','
                               << rule->names[prev] << ',' << d.enlarged << ',' << ms << '\n';
            }
            if ((r = io_next(src, &ev)) != 1) break;
            for (int i = 0; i < ev.n; ++i) ev.v[i] *= o.g_per_unit;

            int idx = rule->fn(&ctx, &ev);
            if (idx < 0 || idx >= n_names) {
                std::fprintf(stderr, "%s: rule %s returned %d outside its %d names\n",
                             argv[0], o.project.c_str(), idx, n_names);
                rc = 1;
                break;
            }
            if (actuate(out, idx) != 0) ++n_act_errors;

            bool ask = o.ask == "always" || idx != prev;
            std::string user = ask ? format_prompt(ev, ctx) : std::string();
            d.panes[0].text = ask ? user : "verdict unchanged, no question asked";
            d.panes[2].text = std::string(rule->names[idx]) + "\nevent " +
                              std::to_string(n_events + 1);
            if (ask) d.panes[1].text = "thinking...";
            d.show();                                 // the verdict, before the model

            edgellm_result res;
            const edgellm_result *logged = nullptr;
            std::memset(&res, 0, sizeof res);
            if (ask) {
                try {
                    edgellm::Result a = engine.decide(wrap_user(user, o.tmpl));
                    std::snprintf(res.text, sizeof res.text, "%s", a.text.c_str());
                    res.n_prompt_tokens = a.n_prompt_tokens;
                    res.n_generated = a.n_generated;
                    res.prompt_ms = a.prompt_ms;
                    res.generate_ms = a.generate_ms;
                    res.stub = a.stub ? 1 : 0;
                    ++n_model;
                    sum_prompt += a.prompt_ms;
                    sum_generate += a.generate_ms;
                } catch (const std::exception &e) {
                    std::fprintf(stderr, "%s: %s\n", argv[0], e.what());
                    ++n_model_errors;
                }
                logged = &res;
                double t0 = ms_now();             // the decision has returned
                d.panes[1].text = std::string(res.text[0] != '\0' ? res.text : "(no answer)") +
                                  (res.stub ? "\n(stub)" : "") + "\n" +
                                  std::to_string((int)(res.prompt_ms + res.generate_ms)) + " ms";
                double ms = d.show();             // the model's answer beside the verdict
                double elapsed = ms_now() - t0;
                if (ms >= 0.0) worst_flip = std::max(worst_flip, elapsed);
                if (plog) {
                    plog << ms_now() / 1000.0 << ",decision," << n_events + 1 << ','
                         << rule->names[idx] << ',' << d.enlarged << ',';
                    if (ms >= 0.0) plog << elapsed;
                    plog << '\n';
                }
            }
            if (elog_row(log, &ev, rule->names[idx], logged) != 0) {
                std::fprintf(stderr, "%s: log write failed\n", argv[0]);
                rc = 1;
                break;
            }
            rule_ctx_push(&ctx, &ev);
            prev = idx;
            ++n_events;
            if (o.max_events > 0 && n_events >= o.max_events) break;
            if (o.interval > 0.0) {
                struct timespec ts;
                ts.tv_sec = (time_t)o.interval;
                ts.tv_nsec = (long)((o.interval - (double)ts.tv_sec) * 1e9);
                nanosleep(&ts, nullptr);
            }
        }
        if (rc == 0 && !stop_requested && r < 0 && (o.max_events == 0 || n_events < o.max_events))
            rc = 1;
        std::fprintf(stderr, "%s: %ld events, %ld model answers, %ld model errors, %ld "
                             "actuation errors, %ld touches, mean prompt %.1f ms, mean "
                             "generate %.1f ms, worst frame after a decision %.1f ms, rss %ld "
                             "kB, engine %s, source %s\n",
                     o.project.c_str(), n_events, n_model, n_model_errors, n_act_errors,
                     n_touch, n_model > 0 ? sum_prompt / (double)n_model : 0.0,
                     n_model > 0 ? sum_generate / (double)n_model : 0.0, worst_flip,
                     edgellm::Engine::rss_kb(), edgellm::Engine::version(),
                     io_backend_name(src));
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s: %s\n", argv[0], e.what());
        rc = 1;
    }
    elog_close(log);
    io_close(leds);
    io_close(src);
    return rc;
}
