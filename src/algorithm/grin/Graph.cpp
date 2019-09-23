#include <src/algorithm/grin/Graph.h>

namespace miner {

const uint32_t Graph::Bucket::kCapacity;

Graph::Graph(uint32_t n, uint32_t ubits, uint32_t vbits) :
        n_(n), u_(n, ubits), v_(n, vbits) {
}

uint32_t Graph::getEdgeCount() {
    LOG(INFO) << "u=" << u_.getEdgeCount() << ", v=" << v_.getEdgeCount();
    return u_.getEdgeCount();
}

uint32_t Graph::Table::getEdgeCount() {
    uint32_t edges = 0;
    for (auto &bucket : buckets_) {
        edges += __builtin_popcount(bucket.full);
    }
    return edges;
}

uint32_t Graph::Table::getOverflowBucketCount() {
    uint32_t overflows = 0;
    for (auto &bucket : buckets_) {
        if (bucket.insertions > Bucket::kCapacity) {
            overflows++;
        }
    }
    return overflows;
}

std::vector<uint32_t> Graph::Table::getValues(uint32_t key) {
    uint32_t bucket = key >> shift_;
    std::vector<uint32_t> values;
    for (;;) {
        Bucket& b = buckets_[bucket];
        uint32_t bound = b.insertions;
        if (bound == 0) {
            break;
        }
        bool overflow = (bound > Bucket::kCapacity);
        if (overflow) {
            bound = Bucket::kCapacity;
        }
        for (uint32_t i = 0; i < bound; ++i) {
            if ((b.full & (1 << i)) == 0) {
                continue;
            }
            if (b.key[i] != key) {
                continue;
            }
            values.push_back(b.value[i]);
        }
        if (!overflow) {
            break;
        }
        bucket = (bucket + 1) & mask_;
    }
    return values;
}

std::vector<Graph::Cycle> Graph::findCycles(int length) {
    Cyclefinder cyclefinder(n_, u_, v_);
    return cyclefinder.findCycles(length);
}

std::vector<Graph::Cycle> Graph::Cyclefinder::findCycles(int length) {
    cycles_.clear();

    for (auto &bucket : u_.buckets_) {
        for(uint32_t j = 0; j<std::min(bucket.insertions, Bucket::kCapacity); ++j) {
            uint32_t u = bucket.key[j];
            appendFromU(u, length);
            std::vector<uint32_t> ignore;
            u_.removeEdges(u, v_, ignore);
        }
    }

    return cycles_;
}

void Graph::Cyclefinder::appendFromU(uint32_t u, int length) {
    for (uint32_t v : u_.getValues(u)) {
        prefix_.push_back(u);
        prefix_.push_back(v);
        // Recurse:
        appendFromV(v ^ 1, length - 1);
        prefix_.pop_back();
        prefix_.pop_back();
    }
}

void Graph::Cyclefinder::appendFromV(uint32_t v, int length) {
    for (uint32_t u : v_.getValues(v)) {
        prefix_.push_back(u);
        prefix_.push_back(v);
        if (length == 1) {
            if (u == (prefix_[0] ^ 1)) {
                // We have found a cycle!
                Cycle c;
                c.uvs = prefix_;
                cycles_.push_back(std::move(c));
            }
        } else {
            // Recurse:
            appendFromU(u ^ 1, length - 1);
        }
        prefix_.pop_back();
        prefix_.pop_back();
    }
}

void Graph::Table::prune(Table& reverse) {
    std::vector<uint32_t> deactivatedK;
    std::vector<uint32_t> deactivatedV;

    for (auto &bucket : buckets_) {
        for (uint32_t j = 0; j < std::min(bucket.insertions, Bucket::kCapacity); ++j) {
            uint32_t key = bucket.key[j];
            if (!hasSingleActive(key)) {
                continue;
            }
            removeEdges(key, reverse, deactivatedV);
            while (!deactivatedV.empty()) {
                uint32_t v = deactivatedV.back();
                deactivatedV.pop_back();
                reverse.removeEdges(v, *this, deactivatedK);
                while (!deactivatedK.empty()) {
                    uint32_t key2 = deactivatedK.back();
                    deactivatedK.pop_back();
                    removeEdges(key2, reverse, deactivatedV);
                }
            }
        }
    }
}

}
/* namespace miner */
