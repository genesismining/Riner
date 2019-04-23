#pragma once

#include <cstdint>
#include "Average.h"

#include <src/statistics/StatisticNode.h>

namespace miner {

    class DeviceRecords {

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

    public:
        struct Data {
            Entry traversedNonces;
            Entry failedShareVerifications;
        };

        DeviceRecords() = default;
        DeviceRecords(DeviceRecords &&) = default;
        explicit DeviceRecords(DeviceRecords &parentListening); //this is not a copy constructor, its just a constructor that uses the parent to initialize

        Data read() const;

        //call this whenever your algo finished traversing a bunch of nonces
        //this is what the hashrate will get derived from
        void reportAmtTraversedNonces(uint64_t traversedNonces);

        //call this when a "hardware error" happened, e.g. the found solution of your
        //algorithm does not stand verification on the cpu
        void reportFailedShareVerification();

        uint64_t getVerificationDifficulty() const;

    private:
        StatisticNode<Data> _node;
    };

}