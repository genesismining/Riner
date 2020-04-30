#pragma once

#include <src/common/Chrono.h>
#include <cmath>
#include <utility>

namespace riner {

    /**
     * running Exponential Average class used by statistics objects
     * use addRecord to add additional data points and getRate() or getWeightRate() for reading results
     */
    class ExpAverage {
        seconds decay_exp;
        double rate{0};
        double weight_rate{0};
        clock::time_point last_update{clock::now()};

        double getExpAvg(double avg, clock::duration delta_t, seconds lambda, double new_value = 0) const;

    public:
        explicit ExpAverage(seconds decay_exp_) : decay_exp(decay_exp_) {
        }

        const seconds &getDecayExp() const {return decay_exp;}

        void addRecord(double weight, clock::time_point time = clock::now());
        double getRate(clock::time_point time = clock::now()) const;
        double getWeightRate(clock::time_point time = clock::now()) const;
    };

    /**
     * running mean class used by statistics objects
     * use addRecord to add additional data points and getRate() or getWeightRate() for reading results
     */
    class Mean {
        uint64_t total{0};
        double weight_total{0};
        double rate{0};
        double weight_rate{0};
        clock::time_point first_update{clock::now()};
        clock::time_point last_update{first_update};

    public:

        clock::duration getElapsedTime(clock::time_point time = clock::now());

        uint64_t getTotal() const {
            return total;
        }

        double getTotalWeight() const {
            return weight_total;
        }

        void addRecord(double weight, clock::time_point time = clock::now());

        double getRate(clock::time_point time = clock::now()) const;

        double getWeightRate(clock::time_point time = clock::now()) const;

        std::pair<double, double> getAndReset(clock::time_point time = clock::now());
    };

}
