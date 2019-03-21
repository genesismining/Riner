
#include "StatisticNode.h"

#include "Average.h"

namespace miner {

    namespace {
        void example() {

            struct PoolRecords {

                struct Record {
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
                };

                Record accepted;
                Record rejected;
                Record duplicate;
            };

            StatisticsNode<PoolRecords> rec;

            StatisticsNode<PoolRecords> ch0;
            StatisticsNode<PoolRecords> ch1;

            StatisticNode<PoolRecords> ch00;

            rec.addListener(ch0);
            rec.addListener(ch1);
            ch0.addListener(ch00);

            bool isAccepted = true;
            bool isDuplicate = false;
            int diff = +1;

            rec.lockedForEach([isAccepted, isDuplicate, diff](PoolRecords &p) {
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

            PoolRecords recs = rec.getValue();
        }
    }
};
