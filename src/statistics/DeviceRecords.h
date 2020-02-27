#pragma once

#include <cstdint>
#include "Average.h"

#include <src/statistics/StatisticNode.h>

namespace riner {

    class DeviceRecords {

        struct Average {
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

        struct NonceAverage {
            void addRecord(double val, clock::time_point t = clock::now()) {
                mean    .addRecord(val, t);
                interval.addRecord(val, t);
                avg5s   .addRecord(val, t);
                avg30s  .addRecord(val, t);
            }

            Mean mean;
            mutable Mean interval; //mutable, because it gets reset by the reader
            ExpAverage avg5s  {seconds(5)};
            ExpAverage avg30s {seconds(30)};
        };

    public:
        struct Data {
            NonceAverage scannedNonces;
            Average validWorkUnits;
            Average invalidWorkUnits;
        };

        DeviceRecords() = default;
        DeviceRecords(DeviceRecords &&) = default;
        explicit DeviceRecords(DeviceRecords &parentListening); //this is not a copy constructor, its just a constructor that uses the parent to initialize

        Data read() const;

        //call this whenever your algo finished traversing a bunch of nonces
        //this is what the hashrate will get derived from
        void reportScannedNoncesAmount(uint64_t scannedNonces);

        //call this when a solution of your algorithm was found that was verified by the CPU
        //if the solution is valid matching e.g. deviceDifficulty, then the valid flag needs to be set
        //a hardware error or a software bug might produce an invalid solution and the valid flag must be cleared for invalid solutions
        void reportWorkUnit(double difficulty, bool valid);

    private:
        StatisticNode<Data> _node;
    };

}