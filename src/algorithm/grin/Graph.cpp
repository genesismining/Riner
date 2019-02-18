#include <src/algorithm/grin/Graph.h>

namespace miner {

Graph::Graph(uint32_t n, uint32_t ubits, uint32_t vbits) :
        n_(n), u_(n, ubits), v_(n, vbits) {
}

uint32_t Graph::getEdgeCount() {
    LOG(INFO) << "u=" << u_.getEdgeCount() << ", v=" << v_.getEdgeCount();
    return u_.getEdgeCount();
}

uint32_t Graph::Table::getEdgeCount() {
    uint32_t edges = 0;
    for (uint32_t i = 0; i < (1 << bits_); ++i) {
        Bucket& bucket = buckets_[i];
        edges += __builtin_popcount(bucket.full);
    }
    return edges;
}

uint32_t Graph::Table::getOverflowBucketCount() {
    uint32_t overflows = 0;
    uint8_t maxi = 0;
    for (uint32_t i = 0; i < (1 << bits_); ++i) {
        Bucket& bucket = buckets_[i];
        if (bucket.insertions > 6) {
            overflows++;
        }
        maxi = std::max(bucket.insertions, maxi);
    }
    LOG(INFO) << "max insertions: " << (uint32_t)maxi;
    return overflows;
}

std::vector<uint32_t> Graph::Table::getValues(uint32_t key) {
    uint32_t bucket = key >> shift_;
    uint8_t lookup = key;
    std::vector<uint32_t> values;
    for (;;) {
        Bucket& b = buckets_[bucket];
        uint8_t bound = b.insertions;
        if (bound == 0) {
            break;
        }
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
            values.push_back(b.value[i]);
        }
        if (!overflow) {
            break;
        }
        bucket = (bucket + 1) & mask_;
    }
    return values;
}

void Graph::prune() {
    std::vector<uint32_t> deactivatedU;
    std::vector<uint32_t> deactivatedV;

    const uint32_t nodes = static_cast<uint32_t>(1) << n_;
    for (uint32_t u = 0; u < nodes; u += 2) {
        if (!uSingleActive(u)) {
            continue;
        }
        removeUU(u, deactivatedV);
        while (!deactivatedV.empty()) {
            uint32_t v = deactivatedV.back();
            deactivatedV.pop_back();
            removeVV(v, deactivatedU);
            while (!deactivatedU.empty()) {
                uint32_t uu = deactivatedU.back();
                deactivatedU.pop_back();
                removeUU(uu, deactivatedV);
            }
        }
    }

    LOG(INFO)<< "Remaining edges after scanning u nodes: " << getEdgeCount();

    for (uint32_t v = 0; v < nodes; v += 2) {
        if (!vSingleActive(v)) {
            continue;
        }
        removeVV(v, deactivatedU);
        while (!deactivatedU.empty()) {
            uint32_t u = deactivatedU.back();
            deactivatedU.pop_back();
            removeUU(u, deactivatedV);
            while (!deactivatedV.empty()) {
                uint32_t vv = deactivatedV.back();
                deactivatedV.pop_back();
                removeVV(vv, deactivatedU);
            }
        }
    }

    LOG(INFO)<<"Remaining edges after scanning u nodes: " << getEdgeCount();
}

std::vector<Graph::Cycle> Graph::findCycles(int length) {
    Cyclefinder cyclefinder(n_, u_, v_);
    return cyclefinder.findCycles(length);
}

std::vector<Graph::Cycle> Graph::Cyclefinder::findCycles(int length) {
    cycles_.clear();

    for (uint32_t u = 0; u < static_cast<uint32_t>(1) << n_; u += 2) {
        appendFromU(u, length);
        std::vector<uint32_t> ignore;
        u_.removeEdges(u, v_, ignore);
    }

    return cycles_;
}

void Graph::Cyclefinder::appendFromU(uint32_t u, int length) {
    for(uint32_t v: u_.getValues(u)) {
        prefix_.push_back(u);
        prefix_.push_back(v);
        // Recurse:
        appendFromV(v ^ 1, length - 1);
        prefix_.pop_back();
        prefix_.pop_back();
    }
}

void Graph::Cyclefinder::appendFromV(uint32_t v, int length) {
    for(uint32_t u: v_.getValues(v)) {
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
            appendFromU(u ^ 1 , length -1);
        }
        prefix_.pop_back();
        prefix_.pop_back();
    }
}


}
/* namespace miner */
