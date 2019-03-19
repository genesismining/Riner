
#pragma once

namespace miner {

    class Statistics {

        //able to create child objects for every gpu to report data
        //batch all information coming from a gpu in a single call (so that clock::now() only has to be called once for those)
        //reported data should be as raw as possible (n hashes, vs hashes/time);
        //locking happens in report calls

        //interesting pool info:
        // - number accepted, rejected, stale shares
    };

}