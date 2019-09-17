//
//

#include <utility>
#include "Average.h"

namespace miner {
    using namespace std::chrono;

    double ExpAverage::getExpAvg(double avg, clock::duration delta_t, seconds lambda, double new_value) const {
        duration<double> seconds(delta_t);
        double alpha = exp(-seconds / lambda);
        return (1 - alpha) * new_value / seconds.count() + alpha * avg; //(avg * seconds.count() + (1 - alpha) * new_value) / (seconds.count() * (2 - alpha));
    }

    void ExpAverage::addRecord(double weight, clock::time_point time) {
        auto time_diff = time - last_update;
        rate = getExpAvg(rate, time_diff, decay_exp, 1);
        weight_rate = getExpAvg(weight_rate, time_diff, decay_exp, weight);
        last_update = time;
    }

    double ExpAverage::getRate(clock::time_point time) const {
        auto time_diff = time - last_update;
        return getExpAvg(rate, time_diff, decay_exp);
    }

    double ExpAverage::getWeightRate(clock::time_point time) const {
        auto time_diff = time - last_update;
        return getExpAvg(weight_rate, time_diff, decay_exp);
    }


    clock::duration Mean::getElapsedTime(clock::time_point time) {
        return time - first_update;
    }

    void Mean::addRecord(double weight, clock::time_point time) {
        double inv_total_time = 1 / std::chrono::duration<double>(time - first_update).count();
        double time_diff = std::chrono::duration<double>(time - last_update).count();
        total++;
        weight_total += weight;
        rate += (1 - time_diff * rate) * inv_total_time;
        weight_rate += (weight - time_diff * weight_rate) * inv_total_time;
        last_update = time;
    }

    std::pair<double, double> Mean::getAndReset(clock::time_point time) {
        std::pair<double, double> res(getWeightRate(time), getRate(time));
        total = 0;
        weight_rate = rate = weight_total = 0;
        first_update = time;
        last_update = time;
        return res;
    }

    double Mean::getWeightRate(clock::time_point time) const {
        auto total_time = duration<double>(time - first_update);
        return (last_update - first_update) / total_time * weight_rate;
    }

    double Mean::getRate(clock::time_point time) const {
        auto total_time = duration<double>(time - first_update);
        return (last_update - first_update) / total_time * rate;
    }
}