// edgellm.hpp: a header-only RAII face over common/c/include/edgellm.h for the C++
// projects (e01, e07, e09, e12 and e02's comparison). Nothing here touches llama.h;
// the engine boundary stays the C API, which is the point of variant B.
//
//     edgellm::Config cfg;
//     cfg.model = "models/qwen2.5-0.5b-instruct-q4_k_m.gguf";
//     cfg.grammar = "grammars/e07_pressure.gbnf";
//     cfg.system_prompt = read_file("prompt.txt");
//     edgellm::Engine engine(cfg);                 // throws std::runtime_error
//     edgellm::Result r = engine.decide("pressure 1013.2 hPa, slope -0.4 hPa/h");
//     std::cout << r.text << " in " << r.generate_ms << " ms\n";
#ifndef EDGELLM_HPP
#define EDGELLM_HPP

#include "edgellm.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace edgellm {

struct Config {
    std::string model;
    std::string grammar;
    std::string system_prompt;
    std::string state;            // empty keeps the system-prompt state resident
    int n_ctx = 512;
    int n_threads = 4;
    int n_predict = 8;
    float temperature = 0.0f;
    unsigned seed = 1;

    // The C view. Valid only while this Config outlives the call that uses it.
    edgellm_config raw() const
    {
        edgellm_config c;
        edgellm_config_init(&c);
        c.model_path = model.c_str();
        c.grammar_path = grammar.c_str();
        c.system_prompt = system_prompt.c_str();
        c.state_path = state.empty() ? nullptr : state.c_str();
        c.n_ctx = n_ctx;
        c.n_threads = n_threads;
        c.n_predict = n_predict;
        c.temperature = temperature;
        c.seed = seed;
        return c;
    }
};

struct Result {
    std::string text;
    int n_prompt_tokens = 0;
    int n_generated = 0;
    double prompt_ms = 0.0;
    double generate_ms = 0.0;
    bool stub = false;
};

class Engine {
public:
    explicit Engine(const Config &cfg)
    {
        char err[256] = { 0 };
        edgellm_config c = cfg.raw();
        e_ = edgellm_open(&c, err, sizeof(err));
        if (e_ == nullptr)
            throw std::runtime_error(err[0] != '\0' ? err : "edgellm_open failed");
    }
    ~Engine() { edgellm_close(e_); }

    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    Engine(Engine &&other) noexcept : e_(other.e_) { other.e_ = nullptr; }
    Engine &operator=(Engine &&other) noexcept
    {
        if (this != &other) {
            edgellm_close(e_);
            e_ = other.e_;
            other.e_ = nullptr;
        }
        return *this;
    }

    Result decide(const std::string &user_text)
    {
        edgellm_result out;
        if (edgellm_decide(e_, user_text.c_str(), &out) != 0)
            throw std::runtime_error("edgellm_decide failed");
        Result r;
        r.text = out.text;
        r.n_prompt_tokens = out.n_prompt_tokens;
        r.n_generated = out.n_generated;
        r.prompt_ms = out.prompt_ms;
        r.generate_ms = out.generate_ms;
        r.stub = out.stub != 0;
        return r;
    }

    // The underlying handle, for the C ABI consumers (e18's daemon) that want it.
    edgellm_t *handle() const { return e_; }

    static const char *version() { return edgellm_engine_version(); }
    static long rss_kb() { return edgellm_rss_kb(); }

private:
    edgellm_t *e_ = nullptr;
};

} // namespace edgellm

#endif // EDGELLM_HPP
