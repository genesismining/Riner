#pragma once

#include <random>

namespace riner {

class Random {
public:
    Random() :
            Random(generateRandomSeedSeq()) {
    }

    Random(unsigned int seed) :
            Random(std::vector<unsigned int>{seed}) {
    }

    Random(std::vector<unsigned int> il) {
        std::seed_seq s(il.begin(), il.end());
        engine_.seed(s);
    }

    void getNextBytes(char* bytes, int numRequested) {
        std::uniform_int_distribution<char> d;
        for (int i = 0; i < numRequested; i++) {
            bytes[i] = d(engine_);
        }
    }

    template<typename T>
    T getUniform() {
        std::uniform_int_distribution<T> d;
        return d(engine_);
    }

    template<typename T>
    T getUniform(T max) {
        std::uniform_int_distribution<T> d(0, max);
        return d(engine_);
    }

private:
    static std::vector<unsigned int> generateRandomSeedSeq() {
        std::random_device r;
        return {r(), r(), r(), r(), r(), r(), r(), r()};
    }

    std::mt19937 engine_;
};

} // namespace miner
