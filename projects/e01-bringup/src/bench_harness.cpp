// bench_harness.cpp: the volume's own tokens-per-second instrument, priced against the
// upstream llama-bench on the same board, model and thread count. It is published only
// beside that figure and only when the two agree within 10 percent; otherwise this
// program is the suspect, not the board.
//
// What it does, in llama-bench's own shape: load a GGUF through the C API, decode a
// 64-token prompt in one batch (the "pp 64" test), then generate 32 tokens one at a time
// with greedy sampling (the "tg 32" test), each timed with std::chrono::steady_clock,
// repeated 5 times with the memory cleared between repeats, and print the mean and the
// standard deviation of each as a Markdown table whose columns match llama-bench -o md.
// The plus-minus is written "+/-" so the source stays ASCII. The last lines print the
// resident set size from /proc/self/status, the thread count and the engine build.
//
// The prompt is the first 64 tokens of prompt.txt, so the prompt text decides nothing
// about speed; the program refuses a prompt that tokenizes to fewer than 64 tokens.
// A sampled end-of-generation token is decoded like any other: this is a throughput
// instrument, not a chat.
//
// Built against llama.cpp at the tag in cmake/llama_tag.cmake; the stub CI build skips it.
#include "llama.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string model;
    std::string prompt = "prompt.txt";
    int n_pp = 64;
    int n_tg = 32;
    int threads = 4;
    int reps = 5;
};

struct Stat {
    double mean = 0.0;
    double sd = 0.0;
};

double ms_now()
{
    using clk = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clk::now().time_since_epoch()).count();
}

Stat stat_of(const std::vector<double> &xs)
{
    Stat s;
    if (xs.empty()) return s;
    for (double x : xs) s.mean += x;
    s.mean /= (double)xs.size();
    if (xs.size() > 1) {
        double acc = 0.0;
        for (double x : xs) acc += (x - s.mean) * (x - s.mean);
        s.sd = std::sqrt(acc / (double)(xs.size() - 1));   // sample deviation, as llama-bench
    }
    return s;
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
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string basename_of(const std::string &path)
{
    size_t i = path.find_last_of("/\\");
    return i == std::string::npos ? path : path.substr(i + 1);
}

} // namespace

int main(int argc, char **argv)
{
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--model" && i + 1 < argc) o.model = argv[++i];
        else if (a == "--prompt" && i + 1 < argc) o.prompt = argv[++i];
        else if (a == "-p" && i + 1 < argc) o.n_pp = std::atoi(argv[++i]);
        else if (a == "-n" && i + 1 < argc) o.n_tg = std::atoi(argv[++i]);
        else if (a == "-t" && i + 1 < argc) o.threads = std::atoi(argv[++i]);
        else if (a == "-r" && i + 1 < argc) o.reps = std::atoi(argv[++i]);
    }
    if (o.model.empty()) {
        std::fprintf(stderr, "usage: e01_bench_harness --model <gguf> [--prompt prompt.txt]"
                             " [-p 64] [-n 32] [-t 4] [-r 5]\n");
        return 2;
    }

    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    llama_model *model = llama_model_load_from_file(o.model.c_str(), mp);
    if (!model) {
        std::fprintf(stderr, "cannot load %s\n", o.model.c_str());
        return 1;
    }
    const llama_vocab *vocab = llama_model_get_vocab(model);
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx = 512;
    cp.n_batch = 512;
    cp.n_threads = o.threads;
    cp.n_threads_batch = o.threads;
    llama_context *ctx = llama_init_from_model(model, cp);
    if (!ctx) {
        std::fprintf(stderr, "cannot create a context\n");
        return 1;
    }

    std::string text = slurp(o.prompt);
    std::vector<llama_token> toks(text.size() + 16);
    int n = llama_tokenize(vocab, text.c_str(), (int32_t)text.size(), toks.data(),
                           (int32_t)toks.size(), true, true);
    if (n < o.n_pp) {
        std::fprintf(stderr, "%s tokenizes to %d tokens, fewer than the %d the test needs\n",
                     o.prompt.c_str(), n, o.n_pp);
        return 1;
    }
    toks.resize((size_t)o.n_pp);

    llama_sampler *smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    std::vector<double> pp, tg;
    for (int r = 0; r < o.reps; ++r) {
        llama_memory_clear(llama_get_memory(ctx), true);
        double t0 = ms_now();
        if (llama_decode(ctx, llama_batch_get_one(toks.data(), (int32_t)toks.size())) != 0) {
            std::fprintf(stderr, "prompt decode failed in repeat %d\n", r);
            return 1;
        }
        double t1 = ms_now();
        pp.push_back(1000.0 * o.n_pp / (t1 - t0));

        double t2 = ms_now();
        for (int i = 0; i < o.n_tg; ++i) {
            llama_token tok = llama_sampler_sample(smpl, ctx, -1);
            llama_sampler_accept(smpl, tok);
            if (llama_decode(ctx, llama_batch_get_one(&tok, 1)) != 0) {
                std::fprintf(stderr, "generation decode failed in repeat %d token %d\n", r, i);
                return 1;
            }
        }
        double t3 = ms_now();
        tg.push_back(1000.0 * o.n_tg / (t3 - t2));
    }

    char desc[128] = "";
    llama_model_desc(model, desc, sizeof desc);
    double size_mib = (double)llama_model_size(model) / (1024.0 * 1024.0);
    double params_b = (double)llama_model_n_params(model) / 1e9;
    Stat spp = stat_of(pp), stg = stat_of(tg);
    std::printf("| model | size | params | backend | threads | test | t/s |\n");
    std::printf("| ----- | ---- | ------ | ------- | ------- | ---- | --- |\n");
    std::printf("| %s | %.0f MiB | %.2f B | CPU | %d | pp %d | %.2f +/- %.2f |\n",
                desc, size_mib, params_b, o.threads, o.n_pp, spp.mean, spp.sd);
    std::printf("| %s | %.0f MiB | %.2f B | CPU | %d | tg %d | %.2f +/- %.2f |\n",
                desc, size_mib, params_b, o.threads, o.n_tg, stg.mean, stg.sd);
    std::printf("\nharness: %s, %d repeats, prompt file %s\n", basename_of(o.model).c_str(),
                o.reps, o.prompt.c_str());
    std::printf("VmRSS kB %ld\n", vmrss_kb());
    std::printf("system: %s\n", llama_print_system_info());

    llama_sampler_free(smpl);
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
