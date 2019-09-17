#pragma once

#include <src/statistics/Average.h>
#include <src/statistics/StatisticNode.h>

namespace miner {

    class PoolRecords {

        struct Averages {
            void addRecord(double val, clock::time_point t = clock::now()) {
                mean    .addRecord(val, t);
                interval.addRecord(val, t);
                avg30s  .addRecord(val, t);
                avg5m   .addRecord(val, t);
            }

            Mean mean;
            mutable Mean interval; //mutable, because it gets reset by the reader
            ExpAverage avg30s {seconds(30)};
            ExpAverage avg5m  {minutes(5)};

            inline auto getAndReset(clock::time_point time) {
                return interval.getAndReset(time);
            }
        };

    public:

        struct Data {
            Averages acceptedShares;
            Averages rejectedShares;
            Averages duplicateShares;

            clock::duration connectionDuration() const;
        };

        PoolRecords() = default;
        PoolRecords(PoolRecords &&) = delete;
        void addListener(PoolRecords &parentListening);

        void reportShare(double difficulty, bool isAccepted, bool isDuplicate);

        Data read() const;

        void resetInterval();

    private:

        StatisticNode<Data> _node;

    };

}
