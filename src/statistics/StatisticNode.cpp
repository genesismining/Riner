
#include <src/common/Json.h>
#include "StatisticNode.h"

#include "Average.h"

namespace miner {

    namespace {
        void example() {

            struct PoolStats : public JsonSerializable {

                struct Entry {
                    void addRecord(double val, clock::time_point t = clock::now()) {
                        mean    .addRecord(val, t);
                        interval.addRecord(val, t);
                        avg30s  .addRecord(val, t);
                        avg5m   .addRecord(val, t);
                    }

                    Mean mean;
                    Mean interval;
                    ExpAverage avg30s;
                    ExpAverage avg5m;

                    nl::json toJson() const {
                        return {{"TODO", "implement PoolStats::Entry::toJson"}};
                    };
                };

                Entry accepted;
                Entry rejected;
                Entry duplicate;

                nl::json toJson() const override {
                    return {
                            {"accepted", accepted.toJson()},
                            {"rejected", rejected.toJson()},
                            {"duplicate", duplicate.toJson()},
                    };
                };
            };

            StatisticNode<PoolStats> rec;

            StatisticNode<PoolStats> ch0;
            StatisticNode<PoolStats> ch1;

            StatisticNode<PoolStats> ch00;

            rec.addListener(ch0);
            rec.addListener(ch1);
            ch0.addListener(ch00);

            bool isAccepted = true;
            bool isDuplicate = false;
            int diff = +1;

            rec.lockedForEach([isAccepted, isDuplicate, diff] (PoolStats &p) {
                auto t = clock::now();

                if (isAccepted) {
                    if (!isDuplicate) {
                        p.accepted.addRecord(diff, t);
                        return;
                    }
                    p.duplicate.addRecord(diff, t);
                }
                p.rejected.addRecord(diff, t);
            });

            PoolStats recs = rec.getValue();
        }
    }
};
