#pragma once

#include <src/statistics/Average.h>
#include <src/statistics/StatisticNode.h>

namespace miner {

    class PoolRecords {

        struct Entry {
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
        };

        struct Data {
            Entry acceptedShares;
            Entry rejectedShares;
            Entry duplicateShares;
        };

        StatisticNode<Data> _node;

    public:
        using Data = Data;

        PoolRecords() {};
        PoolRecords(PoolRecords &&) = delete;
        explicit PoolRecords(PoolRecords &parentListening); //this is not a copy constructor, its just a constructor that uses the parent to initialize

        void reportShare(bool isAccepted, bool isDuplicate);

        Data read() const;

    };

}