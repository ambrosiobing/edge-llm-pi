// cpp_wrapper_test.cpp: compiles common/cpp/edgellm.hpp and, under the stub, checks
// that Engine::decide returns the grammar's first expansion. With the real engine the
// test needs a model file and is skipped at configure time (tests/CMakeLists.txt).
#include "edgellm.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: cpp_wrapper_test <grammar.gbnf> <prompt.txt>\n");
        return 2;
    }
    std::ifstream in(argv[2]);
    std::stringstream ss;
    ss << in.rdbuf();

    edgellm::Config cfg;
    cfg.grammar = argv[1];
    cfg.system_prompt = ss.str();
    try {
        edgellm::Engine engine(cfg);
        edgellm::Result r = engine.decide("SoC temperature 42.0 C. Classify.");
        const std::string want = "{\"decision\":\"COOL\"}";
        if (r.text != want || !r.stub) {
            std::printf("FAIL: got '%s' stub=%d, want '%s'\n", r.text.c_str(), r.stub ? 1 : 0,
                        want.c_str());
            return 1;
        }
        edgellm::Engine moved(std::move(engine));
        edgellm::Result again = moved.decide("SoC temperature 85.0 C. Classify.");
        if (again.text != want) {
            std::printf("FAIL after move: got '%s'\n", again.text.c_str());
            return 1;
        }
        std::printf("cpp_wrapper_test: ok, engine %s\n", edgellm::Engine::version());
        return 0;
    } catch (const std::exception &e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }
}
