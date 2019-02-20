#pragma once

#include <random>

namespace miner {

class Random {
public:
    Random() :
            Random(generateRandomSeedSeq()) {
    }

    Random(unsigned int seed) :
            Random( { seed }) {
    }

    Random(std::initializer_list<unsigned int> il) {
        std::seed_seq s(il);
        engine_.seed(s);
    }

    void nextBytes(char* bytes, int numRequested) {
        std::uniform_int_distribution<char> d;
        for (int i = 0; i < numRequested; i++) {
            bytes[i] = d(engine_);
        }
    }

    bool nextBool() {
        std::uniform_int_distribution<char> d(0, 1);
        return d(engine_) == 0;
    }

    int32_t nextInt() {
        std::uniform_int_distribution<int32_t> d;
        return d(engine_);
    }

    int32_t nextInt(int n) {
        std::uniform_int_distribution<int32_t> d(0, n - 1);
        return d(engine_);
    }

private:
    static std::initializer_list<unsigned int> generateRandomSeedSeq() {
        std::random_device r;
        return {r(), r(), r(), r(), r(), r(), r(), r()};
    }

    std::mt19937 engine_;
};

} // namespace miner
