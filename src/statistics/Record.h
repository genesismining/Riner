
#pragma once

#include <src/common/Chrono.h>
#include <cmath>
#include <src/util/LockUtils.h>

namespace miner {

    struct AggregateValue {
        clock::time_point initTime;
        uint64_t count {};
        uint64_t value {};

        double avg() const {
            return double(value) / count;
        }

        double countPerTime(clock::duration dt) const {
            return double(count) / dt.count();
        }

        double valuePerTime(clock::duration dt) const {
            return double(value) / dt.count();
        }
    };

    class MovingAvgExp {
        clock::duration lambda;
        clock::time_point lastUpdateTime;
        double avg;

    public:
        MovingAvgExp();
        void update(double value, clock::time_point time = clock::now());
    };

    class Record {
        clock::time_point firstUpdateTime;
        clock::time_point lastUpdateTime;

        inline clock::duration totalDuration() {
            return lastUpdateTime - firstUpdateTime;
        }

        AggregateValue total;

        AggregateValue avgRate5s;
        AggregateValue avgRate1m;

        inline static double getExpAvg(double avg, clock::duration deltaT, std::chrono::seconds lambda, double new_value = 0) {
            std::chrono::duration<double> seconds(deltaT);
            double alpha = exp(-seconds / lambda);
            return (1 - alpha) * new_value / seconds.count() + alpha * avg;
        }

    public:
        void reset() {
            firstUpdateTime = lastUpdateTime = clock::now(); //TODO: risk of division by zero?
            total = decltype(total)();
            avgRate5s.count = avgRate1m.count = 0;
            avgRate5s.value = avgRate1m.value = 0;
        }

    };

    class RecordNode {
    };

    void foo() {

        LockGuarded<Record> recL;

        recL.lock()->reset();


    }


}

