#pragma once

#include <cstdint>
#include "Average.h"

#include <src/statistics/StatisticNode.h>

namespace riner {

    /**
     * class for gathering statistics per compute device (e.g. per GPU)
     * XRecords types are wired in a tree structure that aggregates their statistics data.
     * The tree structure is created via the `XRecords(XRecords &parent)` constructor
     */
    class DeviceRecords {

        //the classes Average and NonceAverage are convenience wrappers around the individual Mean and ExpAverage values involved
        //adding a record to Average or NonceAverage will propagate that data point to the contained Mean and ExpAverage values.
        struct Average {
            /**
             * add a Record and propagate it to the contained Mean and ExpAverage objects
             */
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

        //see comment above class Average
        struct NonceAverage {
            /**
             * add a Record and propagate it to the contained Mean and ExpAverage objects
             */
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
        /**
         * actual statistics that are being tracked
         * see reportX methods for more details
         */
        struct Data {
            NonceAverage scannedNonces;
            Average validWorkUnits;
            Average invalidWorkUnits;
        };

        DeviceRecords() = default;
        
        /**
         * regular move constructor, nothing different here.
         */
        DeviceRecords(DeviceRecords &&) = default;
        
        /**
         * this is not a copy constructor, its just a constructor that uses the listening parent to initialize
         */
        explicit DeviceRecords(DeviceRecords &parentListening);

        /**
         * reads the current state of the statistics data and copies it out
         */
        Data read() const;

        /**
         * call this whenever your algo finished traversing a bunch of nonces
         * this is what the hashrate will get derived from
         */
        void reportScannedNoncesAmount(uint64_t scannedNonces);

        /**
         * call this when a solution of your algorithm was found that was verified by the CPU.
         * if the solution is valid matching e.g. deviceDifficulty, then the valid flag needs to be set.
         * a hardware error or a software bug might produce an invalid solution and the valid flag must be cleared for invalid solutions
         */
        void reportWorkUnit(double difficulty, bool valid);

    private:
        StatisticNode<Data> _node;
    };

}
