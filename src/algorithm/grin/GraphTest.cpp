#include <src/algorithm/grin/Graph.h>

#include <src/common/Optional.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>

namespace miner {
namespace {

TEST(Graph, AddUEdges) {
    Graph g(14 /* n */, 8 /* ubits */,  6 /* vbits */);

    ASSERT_FALSE(g.uSingleActive(0x705));

    g.addUToV(0x705, 0x123);
    g.addUToV(0x700, 0x000);
    ASSERT_TRUE(g.uSingleActive(0x705));

    g.addUToV(0x705, 0x222);
    ASSERT_TRUE(g.uSingleActive(0x705));

    g.addUToV(0x704, 0x200);
    ASSERT_FALSE(g.uSingleActive(0x705));
    ASSERT_FALSE(g.uSingleActive(0x704));
}

TEST(Graph, AddUEdgesOverflow) {
    Graph g(14 /* n */, 8 /* ubits */,  6 /* vbits */);
    for(int i=0; i<15; ++i) {
        g.addUToV(0x500, i);
    }
    ASSERT_TRUE(g.uSingleActive(0x500));

    g.addUToV(0x501, 0x666);
    ASSERT_FALSE(g.uSingleActive(0x500));
}

TEST(Graph, AddVEdges) {
    Graph g(14 /* n */, 8 /* ubits */,  6 /* vbits */);

    ASSERT_FALSE(g.vSingleActive(0x755));

    g.addVToU(0x755, 0x123);
    g.addVToU(0x755, 0x000);
    ASSERT_TRUE(g.vSingleActive(0x755));

    g.addVToU(0x755, 0x222);
    ASSERT_TRUE(g.vSingleActive(0x755));

    g.addVToU(0x754, 0x200);
    ASSERT_FALSE(g.vSingleActive(0x755));
    ASSERT_FALSE(g.vSingleActive(0x754));
}

TEST(Graph, AddVEdgesOverflow) {
    Graph g(14 /* n */, 8 /* ubits */,  6 /* vbits */);
    for(int i=0; i<15; ++i) {
        g.addVToU(0x500, i);
    }
    ASSERT_TRUE(g.vSingleActive(0x500));

    g.addVToU(0x501, 0x666);
    ASSERT_FALSE(g.vSingleActive(0x500));
}

void addEdge(Graph& g, uint32_t u, uint32_t v) {
    g.addUToV(u, v);
    g.addVToU(v, u);
}

void fillGraph(Graph& g) {
    addEdge(g, 0x430, 0x270);
    addEdge(g, 0x431, 0x272);
    addEdge(g, 0x432, 0x273);
    addEdge(g, 0x432, 0x274);
    addEdge(g, 0x434, 0x277);
    addEdge(g, 0x435, 0x275);
    addEdge(g, 0x436, 0x273);
    addEdge(g, 0x437, 0x271);
    addEdge(g, 0x438, 0x275);
    addEdge(g, 0x438, 0x279);
    addEdge(g, 0x439, 0x279);
    addEdge(g, 0x43a, 0x279);
    EXPECT_EQ(12, g.getEdgeCount());
}

TEST(Graph, RemoveInactiveEdges) {
    Graph g(14 /* n */, 8 /* ubits */,  6 /* vbits */);
    fillGraph(g);

    std::vector<uint32_t> deactivatedV;
    EXPECT_TRUE(g.uSingleActive(0x432));
    g.removeUU(0x432, deactivatedV);
    EXPECT_THAT(deactivatedV, testing::UnorderedElementsAre(0x274));

    std::vector<uint32_t> deactivatedU;
    EXPECT_TRUE(g.vSingleActive(0x274));
    g.removeVV(0x274, deactivatedU);
    EXPECT_THAT(deactivatedU, testing::UnorderedElementsAre(0x435));

    deactivatedV.clear();
    EXPECT_TRUE(g.uSingleActive(0x435));
    g.removeUU(0x435, deactivatedV);
    EXPECT_THAT(deactivatedV, testing::UnorderedElementsAre(0x277));

    deactivatedU.clear();
    EXPECT_FALSE(g.vSingleActive(0x277));
    g.removeVV(0x277, deactivatedU);
    EXPECT_THAT(deactivatedU, testing::UnorderedElementsAre());

    deactivatedU.clear();
    EXPECT_TRUE(g.vSingleActive(0x278));
    g.removeVV(0x278, deactivatedU);
    EXPECT_THAT(deactivatedU, testing::UnorderedElementsAre(0x438, 0x439, 0x43a));

    deactivatedV.clear();
    g.removeUU(0x438, deactivatedV);
    g.removeUU(0x439, deactivatedV);
    g.removeUU(0x43a, deactivatedV);
    EXPECT_THAT(deactivatedV, testing::UnorderedElementsAre());

    EXPECT_EQ(4, g.getEdgeCount());
}

TEST(Graph, FindCycles) {
    Graph g(14 /* n */, 8 /* ubits */,  6 /* vbits */);
    fillGraph(g);
    std::vector<Graph::Cycle> cycles = g.findCycles(4);
    ASSERT_EQ(1, cycles.size());
    EXPECT_THAT(cycles[0].uvs, testing::ElementsAre(0x430, 0x270, 0x437, 0x271, 0x436, 0x273, 0x431, 0x272));
}

TEST(Graph, PruneAndFindCycles) {
    Graph g(14 /* n */, 8 /* ubits */,  7 /* vbits */);
    fillGraph(g);
    g.pruneFromU();
    LOG(INFO) << "r =" << g.getEdgeCount();
    g.pruneFromV();
    EXPECT_EQ(4, g.getEdgeCount());
    std::vector<Graph::Cycle> cycles = g.findCycles(4);
    ASSERT_EQ(1, cycles.size());
    EXPECT_THAT(cycles[0].uvs, testing::ElementsAre(0x430, 0x270, 0x437, 0x271, 0x436, 0x273, 0x431, 0x272));
}

void resolveEdges(const SiphashKeys* keys, int n, Graph::Cycle& cycle) {
    cycle.edges.resize(cycle.uvs.size() / 2);
    uint32_t nodemask = (1 << n) - 1;

    for(uint32_t i=0; i <= nodemask; ++i) {
        uint32_t u = siphash24(keys, 2 * i + 0) & nodemask;
        uint32_t v = siphash24(keys, 2 * i + 1) & nodemask;
        for (int j = 0; j < 42; ++j) {
            if (u == cycle.uvs[2*j] && v == cycle.uvs[2*j+1]) {
                cycle.edges[j] = i;
            }
        }
    }
    std::sort(cycle.edges.begin(), cycle.edges.end());
}

TEST(Graph, Find42Cycles) {
    const int n = 19;
    SiphashKeys keys { 0x8a15fa55af1d8dc3, 0xe59246b80d41ad02, 0xb6d8c79874229737, 0x7dc6b1b676f6976 };
    Graph g(n, n - 2, n - 2);
    const uint32_t edges = 1 << n;
    const uint32_t nodemask = edges - 1;
    for(uint32_t i=0; i<edges; ++i) {
        uint32_t u = siphash24(&keys, 2 * i + 0) & nodemask;
        uint32_t v = siphash24(&keys, 2 * i + 1) & nodemask;
        g.addUToV(u, v);
        g.addVToU(v, u);
    }
    LOG(INFO) << "Overflow counts: u=" << g.getOverflowBucketCount(0) << ", v=" << g.getOverflowBucketCount(1);
    g.pruneFromU();
    g.pruneFromV();
    EXPECT_EQ(84, g.getEdgeCount());
    std::vector<Graph::Cycle> cycles = g.findCycles(42);
    EXPECT_EQ(3, cycles.size());
    for(auto& cycle: cycles) {
        resolveEdges(&keys, n, cycle);
    }
    cycles.erase(
            std::remove_if(cycles.begin(), cycles.end(),
                    [](const auto& cycle) {return cycle.edges[0] == cycle.edges[1];}),
            cycles.end());
    EXPECT_EQ(1, cycles.size());
    EXPECT_THAT(cycles[0].edges, testing::ElementsAreArray({
            600, 3722, 41720, 58739, 69765, 70649, 70819, 83353, 114099, 120100,
            120944, 122765, 126884, 129309, 129462, 146983, 174642, 193359,
            193491, 201392, 237344, 239521, 265543, 269672, 273007, 275406,
            278480, 279040, 302490, 315554, 327414, 360947, 364487, 409474,
            427119, 441744, 457145, 460995, 461491, 468091, 499462, 516637}));
}

} // namespace
} // miner
