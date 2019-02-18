#pragma once

#include "siphash.h"
#include <src/common/Assert.h>

#include <stdint.h>
#include <vector>

namespace miner {


class Graph {
public:
    struct Cycle {
        // Alternating [u,v] nodes of the edges.
        std::vector<uint32_t> uvs;
        std::vector<uint32_t> edges;
    };

    Graph(uint32_t n, uint32_t ubits, uint32_t vbits);

    void addUToV(uint32_t u, uint32_t v) {
        u_.insert(u, v);
    }

    void addVToU(uint32_t v, uint32_t u) {
        v_.insert(v, u);
    }

    bool uSingleActive(uint32_t uu) {
        return u_.hasSingleActive(uu);
    }

    bool vSingleActive(uint32_t vv) {
        return v_.hasSingleActive(vv);
    }

    void removeUU(uint32_t uu, std::vector<uint32_t>& deactivatedVs) {
        u_.removeEdges(uu, v_, deactivatedVs);
    }

    void removeVV(uint32_t vv, std::vector<uint32_t>& deactivatedUs) {
        v_.removeEdges(vv, u_, deactivatedUs);
    }

    void prune();

    uint32_t getEdgeCount();
    uint32_t getOverflowBucketCount(int uorv) {
        if (uorv == 0) {
            return u_.getOverflowBucketCount();
        } else {
            return v_.getOverflowBucketCount();
        }
    }

    std::vector<Cycle> findCycles(int length);

private:
    struct Bucket {
        static constexpr uint8_t kCapacity = 6;
        uint32_t value[6];
        uint8_t key[6];
        uint8_t insertions = 0;
        uint8_t full = 0;
    };

    class Cyclefinder;

    class Table {
    public:
        Table(uint32_t n, uint32_t bits) :
                bits_(bits), mask_((static_cast<uint32_t>(1) << bits) - 1), shift_(n - bits) {
            MI_EXPECTS(n - bits <= 9);
            uint32_t count = static_cast<uint32_t>(1) << bits;
            buckets_ = (Bucket*)aligned_alloc(sizeof(Bucket), sizeof(Bucket) * count);
            memset(buckets_, 0, sizeof(Bucket) * count);
        }

        ~Table() {
            delete[] buckets_;
        }

        uint32_t getEdgeCount();
        uint32_t getOverflowBucketCount();
        std::vector<uint32_t> getValues(uint32_t key);

        friend class Cyclefinder;

        void insert(uint32_t key, uint32_t value) {
            uint32_t bucket = key >> shift_;
            for (;;) {
                bool succ = Graph::insert(buckets_[bucket], key, value);
                if (succ) {
                    break;
                }
                bucket = (bucket + 1) & mask_;
            }
        }

        bool hasSingleActive(uint32_t key) {
            uint32_t bucket = key >> shift_;
            const uint8_t key1 = key;
            const uint8_t key2 = key ^ 1;
            bool active1 = false;
            bool active2 = false;
            for (;;) {
                bool overflow = scanActive12(buckets_[bucket], key1, key2, active1, active2);
                if (!overflow) {
                    break;
                }
                bucket = (bucket + 1) & mask_;
            }
            return active1 ^ active2;
        }

        void removeEdges(uint32_t key, Table& reverse, std::vector<uint32_t>& deactivated) {
            uint32_t bucket = key >> shift_;
            const uint8_t lookup = static_cast<uint8_t>(key) >> 1;
            const uint32_t prefix = bucket << shift_;
            for (;;) {
                Bucket& b = buckets_[bucket];
                uint8_t bound = b.insertions;
                bool overflow = (bound > 6);
                if (overflow) {
                    bound = 6;
                }
                for (uint8_t i = 0; i < bound; ++i) {
                    if ((b.full & (1 << i)) == 0) {
                        continue;
                    }
                    if ((b.key[i] >> 1) != lookup) {
                        continue;
                    }
                    if (!reverse.removeEdge(b.value[i], prefix | b.key[i])) {
                        deactivated.push_back(b.value[i]);
                    }
                    b.full ^= (1 << i);
                }
                if (!overflow) {
                    break;
                }
                bucket = (bucket + 1) & mask_;
            }
        }

        bool removeEdge(uint32_t key, uint32_t value) {
            uint32_t bucket = key >> shift_;
            bool active = false;
            bool removed = false;
            const uint8_t lookup = key;
            for (;;) {
                Bucket& b = buckets_[bucket];
                uint8_t bound = b.insertions;
                bool overflow = (bound > 6);
                if (overflow) {
                    bound = 6;
                }
                for (uint8_t i = 0; i < bound; ++i) {
                    if ((b.full & (1 << i)) == 0) {
                        continue;
                    }
                    if (b.key[i] != lookup) {
                        continue;
                    }
                    if (b.value[i] == value && !removed) {
                        MI_ENSURES(!removed);
                        removed = true;
                        b.full ^= (1 << i);
                    } else {
                        active = true;
                    }
                }
                if (!overflow) {
                    break;
                }
                bucket = (bucket + 1) & mask_;
            }
            //MI_ENSURES(removed);
            return active;
        }

    private:
        const uint32_t bits_;
        const uint32_t mask_;
        const uint32_t shift_;

        Bucket* buckets_;
    };

    class Cyclefinder {
    public:
        Cyclefinder(uint32_t n, Graph::Table& u, Graph::Table& v): n_(n), u_(u), v_(v) {}
        std::vector<Graph::Cycle> findCycles(int length);
    private:
        void appendFromV(uint32_t v, int length);
        void appendFromU(uint32_t u, int length);
        const uint32_t n_;
        std::vector<Graph::Cycle> cycles_;
        std::vector<uint32_t> prefix_;
        Graph::Table& u_;
        Graph::Table& v_;
    };


    static bool insert(Bucket& bucket, uint32_t key, uint32_t value) {
        if (bucket.insertions >= 6) {
            // Bucket is already full :-(
            bucket.insertions++;
            return false;
        }
        int pos = bucket.insertions++;
        bucket.full |= 1 << pos;
        bucket.key[pos] = key;
        bucket.value[pos] = value;
        return true;
    }

    static bool scanActive12(Bucket& b, uint8_t key1, uint8_t key2, bool& active1, bool& active2) {
        uint8_t bound = b.insertions;
        bool overflow = (bound > 6);
        if (overflow) {
            bound = 6;
        }
        for (uint8_t i = 0; i < bound; ++i) {
            if ((b.full & (1 << i)) == 0) {
                continue;
            }
            if (b.key[i] == key1) {
                active1 = true;
            } else if (b.key[i] == key2) {
                active2 = true;
            }
        }
        return overflow;
    }

    uint32_t n_;
    Table u_;
    Table v_;
};

} /* namespace miner */

