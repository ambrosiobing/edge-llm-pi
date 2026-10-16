// variant_b_capi.cpp: catalogue variant B, C++17 over the C API, used only to price
// variant B against the C baseline of project e02. The C API is the volume's own
// edgellm.h, reached through the header-only RAII face common/cpp/edgellm.hpp that
// e07, e09 and e12 build on; nothing here includes llama.h. The loop is the same
// decision as e02_main.c: the same rule symbol, prompt, grammar, replay and log shape,
// so tools/agreement.py scores it against the one oracle.
//
// It prints VmRSS from /proc/self/status and the mean prompt and generation times the
// engine reports per decision. State restore versus cold decode is inside the engine
// and is measured by variant A; this file measures what the C++ layer adds over C.
//
// Built against the real engine; never in the stub CI build.
#include "edgellm.hpp"
#include "rules.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

extern "C" const rule_desc rule_e02;

namespace {

std::string slurp(const std::string &path)
{
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot read " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

const char *decision_of(const std::string &text)
{
    for (const char *const *n = rule_e02.names; *n; ++n)
        if (text.find(*n) != std::string::npos) return *n;
    return "none";
}

bool parse_row(const std::string &line, io_event &ev)
{
    std::stringstream ss(line);
    std::string t, tag, a, b;
    if (!std::getline(ss, t, ',') || !std::getline(ss, tag, ',') ||
        !std::getline(ss, a, ',') || !std::getline(ss, b, ',')) return false;
    std::memset(&ev, 0, sizeof ev);
    ev.t_s = std::atof(t.c_str());
    std::snprintf(ev.tag, sizeof ev.tag, "%s", tag.c_str());
    ev.n = 2;
    ev.v[0] = std::atof(a.c_str());
    ev.v[1] = std::atof(b.c_str());
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    edgellm::Config cfg;
    std::string replay, prompt = "prompt.txt", log_path = "e02_b.csv";
    cfg.grammar = "grammar.gbnf";
    cfg.state = "e02_b.state";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](std::string &dst) { if (i + 1 < argc) dst = argv[++i]; };
        if (a == "--model") next(cfg.model);
        else if (a == "--replay") next(replay);
        else if (a == "--grammar") next(cfg.grammar);
        else if (a == "--prompt") next(prompt);
        else if (a == "--state") next(cfg.state);
        else if (a == "--log") next(log_path);
        else if (a == "--threads" && i + 1 < argc) cfg.n_threads = std::atoi(argv[++i]);
    }
    if (cfg.model.empty() || replay.empty()) {
        std::fprintf(stderr, "usage: e02_variant_b --model <gguf> --replay <csv>"
                             " [--grammar f] [--prompt f] [--state f] [--log f] [--threads n]\n");
        return 2;
    }
    try {
        cfg.system_prompt = slurp(prompt);
        edgellm::Engine eng(cfg);
        std::ifstream in(replay);
        std::ofstream log(log_path);
        if (!in || !log) throw std::runtime_error("cannot open the replay or the log");
        log << "t_s,tag,rule,model,prompt_ms,generate_ms\n";
        std::string line;
        std::getline(in, line);                                     // the header
        rule_ctx ctx;
        std::memset(&ctx, 0, sizeof ctx);
        double sum_p = 0, sum_g = 0;
        long n_rows = 0, n_free = 0;
        char user[256];
        while (std::getline(in, line)) {
            io_event ev;
            if (!parse_row(line, ev)) continue;
            int idx = rule_e02.fn(&ctx, &ev);
            std::snprintf(user, sizeof user, "level %.1f, slope %.1f per second. Decide.",
                          ev.v[0], ev.v[1]);
            edgellm::Result r = eng.decide(user);
            const char *m = decision_of(r.text);
            if (std::strcmp(m, "none") == 0) ++n_free;
            log << ev.t_s << ',' << ev.tag << ',' << rule_e02.names[idx] << ',' << m << ','
                << r.prompt_ms << ',' << r.generate_ms << '\n';
            sum_p += r.prompt_ms;
            sum_g += r.generate_ms;
            ++n_rows;
        }
        std::printf("variant B (C++ over the C API): rows %ld, engine %s\n", n_rows,
                    edgellm::Engine::version());
        std::printf("mean prompt_ms %.1f, mean generate_ms %.1f, rows without a literal %ld\n",
                    n_rows ? sum_p / n_rows : 0.0, n_rows ? sum_g / n_rows : 0.0, n_free);
        std::printf("VmRSS kB %ld\n", edgellm::Engine::rss_kb());
        return n_free ? 1 : 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "e02_variant_b: %s\n", e.what());
        return 1;
    }
}
