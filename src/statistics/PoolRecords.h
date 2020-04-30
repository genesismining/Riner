#pragma once

#include <src/statistics/Average.h>
#include <src/statistics/StatisticNode.h>

namespace riner {
    /**
     * class for gathering statistics per pool connection
     * XRecords types are wired in a tree structure that aggregates their statistics data.
     * The tree structure is created via the `XRecords(XRecords &parent)` constructor
     *
     * The layout of this class is similar to `DeviceRecords`. see `DeviceRecords.h` for more documentation
     */
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

            /**
             * returns a pool connection duration estimate based on accepted shares time interval
             */
            clock::duration connectionDuration() const;
        };

        PoolRecords() = default;
        PoolRecords(PoolRecords &&) = delete;
        void addListener(PoolRecords &parentListening);

        /**
         * call this method to report that a share of a certain difficulty was sent to
         * the pool and whether the pool accepted it (and whether it was a duplicate or not)
         */
        void reportShare(double difficulty, bool isAccepted, bool isDuplicate);

        Data read() const;
        
        /**
         * reset the averaging intervals of Data's members for all listeners
         */
        void resetInterval();

    private:

        StatisticNode<Data> _node;

    };

}
