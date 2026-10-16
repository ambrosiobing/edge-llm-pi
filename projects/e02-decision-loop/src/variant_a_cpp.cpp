// variant_a_cpp.cpp: catalogue variant A, C++17 linked to llama.cpp directly through
// llama.h, used only to price variant A against the C baseline of project e02. The
// loop is the same decision as e02_main.c on the shared main: the same rule symbol,
// the same prompt.txt, the same grammar, the same replay CSV, the same log shape, so
// tools/agreement.py scores all three against the one oracle.
//
// It prints the figures the comparison table wants: VmRSS from /proc/self/status, the
// mean prompt and generation times, and, with --cold, the same loop with the system
// prompt decoded again for every event instead of restored from the saved state. The
// acceptance test of the chapter is the ratio of those two prompt times.
//
// Built against llama.cpp at the tag in cmake/llama_tag.cmake; never in the stub CI
// build. It links edgellm_core as well, only because rules_e02.c registers itself.
#include "llama.h"
#include "rules.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" const rule_desc rule_e02;

namespace {

struct Options {
    std::string model;
    std::string replay;
    std::string grammar = "grammar.gbnf";
    std::string prompt  = "prompt.txt";
    std::string state   = "e02_a.state";
    std::string log     = "e02_a.csv";
    int  threads   = 4;
    int  n_predict = 8;
    bool cold      = false;
};

double ms_now()
{
    using clk = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clk::now().time_since_epoch()).count();
}

long vmrss_kb()
{
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("VmRSS:", 0) == 0) return std::atol(line.c_str() + 6);
    return -1;
}

std::string slurp(const std::string &path)
{
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot read " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<llama_token> tokenize(const llama_vocab *vocab, const std::string &text, bool special)
{
    std::vector<llama_token> out(text.size() + 16);
    int n = llama_tokenize(vocab, text.c_str(), (int32_t)text.size(), out.data(),
                           (int32_t)out.size(), special, true);
    if (n < 0) throw std::runtime_error("tokenize failed");
    out.resize((size_t)n);
    return out;
}

const char *decision_of(const std::string &text)
{
    for (const char *const *n = rule_e02.names; *n; ++n)
        if (text.find(*n) != std::string::npos) return *n;
    return "none";
}

// The engine, owned by one object so every exit path frees in the right order.
class Engine {
public:
    Engine(const Options &o)
    {
        llama_backend_init();
        llama_model_params mp = llama_model_default_params();
        model_ = llama_model_load_from_file(o.model.c_str(), mp);
        if (!model_) throw std::runtime_error("cannot load " + o.model);
        vocab_ = llama_model_get_vocab(model_);
        llama_context_params cp = llama_context_default_params();
        cp.n_ctx = 512;
        cp.n_batch = 512;
        cp.n_threads = o.threads;
        cp.n_threads_batch = o.threads;
        ctx_ = llama_init_from_model(model_, cp);
        if (!ctx_) throw std::runtime_error("cannot create a context");
        grammar_ = slurp(o.grammar);
        sampler_ = llama_sampler_chain_init(llama_sampler_chain_default_params());
        llama_sampler_chain_add(sampler_,
                                llama_sampler_init_grammar(vocab_, grammar_.c_str(), "root"));
        llama_sampler_chain_add(sampler_, llama_sampler_init_greedy());
    }
    ~Engine()
    {
        if (sampler_) llama_sampler_free(sampler_);
        if (ctx_) llama_free(ctx_);
        if (model_) llama_model_free(model_);
        llama_backend_free();
    }
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;

    void decode(std::vector<llama_token> &toks)
    {
        if (llama_decode(ctx_, llama_batch_get_one(toks.data(), (int32_t)toks.size())) != 0)
            throw std::runtime_error("decode failed");
    }
    void clear() { llama_memory_clear(llama_get_memory(ctx_), true); }
    void save(const std::string &path, const std::vector<llama_token> &toks)
    {
        if (!llama_state_save_file(ctx_, path.c_str(), toks.data(), toks.size()))
            throw std::runtime_error("state save failed");
    }
    void load(const std::string &path)
    {
        std::vector<llama_token> toks(512);
        size_t n = 0;
        if (!llama_state_load_file(ctx_, path.c_str(), toks.data(), toks.size(), &n))
            throw std::runtime_error("state load failed");
    }
    std::string generate(int n_predict)
    {
        std::string out;
        char piece[64];
        llama_sampler_reset(sampler_);
        for (int i = 0; i < n_predict; ++i) {
            llama_token tok = llama_sampler_sample(sampler_, ctx_, -1);
            llama_sampler_accept(sampler_, tok);
            if (llama_vocab_is_eog(vocab_, tok)) break;
            int n = llama_token_to_piece(vocab_, tok, piece, sizeof piece, 0, true);
            if (n > 0) out.append(piece, (size_t)n);
            std::vector<llama_token> one{tok};
            decode(one);
        }
        return out;
    }
    const llama_vocab *vocab() const { return vocab_; }

private:
    llama_model *model_ = nullptr;
    llama_context *ctx_ = nullptr;
    const llama_vocab *vocab_ = nullptr;
    llama_sampler *sampler_ = nullptr;
    std::string grammar_;
};

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
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](std::string &dst) { if (i + 1 < argc) dst = argv[++i]; };
        if (a == "--model") next(o.model);
        else if (a == "--replay") next(o.replay);
        else if (a == "--grammar") next(o.grammar);
        else if (a == "--prompt") next(o.prompt);
        else if (a == "--state") next(o.state);
        else if (a == "--log") next(o.log);
        else if (a == "--threads" && i + 1 < argc) o.threads = std::atoi(argv[++i]);
        else if (a == "--cold") o.cold = true;
    }
    if (o.model.empty() || o.replay.empty()) {
        std::fprintf(stderr, "usage: e02_variant_a --model <gguf> --replay <csv> [--cold]"
                             " [--grammar f] [--prompt f] [--state f] [--log f] [--threads n]\n");
        return 2;
    }
    try {
        Engine eng(o);
        std::string sys = slurp(o.prompt);
        std::vector<llama_token> sys_toks = tokenize(eng.vocab(), sys, true);
        eng.clear();
        eng.decode(sys_toks);
        eng.save(o.state, sys_toks);

        std::ifstream in(o.replay);
        std::ofstream log(o.log);
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
            std::vector<llama_token> user_toks = tokenize(eng.vocab(), user, false);
            double t0 = ms_now();
            if (o.cold) {                          // the whole prompt again, every event
                eng.clear();
                std::vector<llama_token> all = sys_toks;
                all.insert(all.end(), user_toks.begin(), user_toks.end());
                eng.decode(all);
            } else {                               // restore the saved state, decode the user text
                eng.load(o.state);
                eng.decode(user_toks);
            }
            double t1 = ms_now();
            std::string text = eng.generate(o.n_predict);
            double t2 = ms_now();
            const char *model = decision_of(text);
            if (std::strcmp(model, "none") == 0) ++n_free;
            log << ev.t_s << ',' << ev.tag << ',' << rule_e02.names[idx] << ',' << model << ','
                << (t1 - t0) << ',' << (t2 - t1) << '\n';
            sum_p += t1 - t0;
            sum_g += t2 - t1;
            ++n_rows;
        }
        std::printf("variant A (C++ on llama.h): rows %ld, mode %s\n", n_rows,
                    o.cold ? "cold" : "restore");
        std::printf("mean prompt_ms %.1f, mean generate_ms %.1f, rows without a literal %ld\n",
                    n_rows ? sum_p / n_rows : 0.0, n_rows ? sum_g / n_rows : 0.0, n_free);
        std::printf("VmRSS kB %ld\n", vmrss_kb());
        return n_free ? 1 : 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "e02_variant_a: %s\n", e.what());
        return 1;
    }
}
