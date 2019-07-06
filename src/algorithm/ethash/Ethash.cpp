
#include <cstring>
#include <src/util/Logging.h>
#include <src/common/PlatformDefines.h>
#include "Ethash.h"
#include "EthSha3.h"
#include <src/util/sph/sph_keccak.h>
#include <src/common/Endian.h>

#define FNV_PRIME    0x01000193

#define fnv(x, y)    (((x) * FNV_PRIME) ^ (y))
#define fnv_reduce(v)  fnv(fnv(fnv((v)[0], (v)[1]), (v)[2]), (v)[3])
#define ETHEREUM_EPOCH_LENGTH 30000UL

namespace miner {

    typedef struct _DAG128 {
        uint32_t Columns[32];
    } DAG128;

    typedef union _Node {
        uint8_t bytes[16 * 4];
        uint32_t words[16];
        uint64_t double_words[16 / 2];
    } Node;

    uint32_t EthCalcEpochNumber(cByteSpan<32> SeedHash) {
        uint8_t TestSeedHash[32] = {0};

        for (uint32_t Epoch = 0; Epoch < 2048; ++Epoch) {
            if (!memcmp(TestSeedHash, SeedHash.data(), 32))
                return Epoch;
            SHA3_256(TestSeedHash, TestSeedHash, 32);
        }

        LOG(ERROR) << "Error on epoch calculation.";

        return std::numeric_limits<uint32_t>::max();
    }

    Node CalcDAGItem(const Node *CacheInputNodes, uint32_t NodeCount, uint32_t NodeIdx) {
        Node DAGNode = CacheInputNodes[NodeIdx % NodeCount];

        DAGNode.words[0] ^= NodeIdx;

        SHA3_512(DAGNode.bytes, DAGNode.bytes, sizeof(Node));

        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t parent_index = fnv(NodeIdx ^ i, DAGNode.words[i % 16]) % NodeCount;
            Node const *parent = CacheInputNodes + parent_index; //&cache_nodes[parent_index];

            for (int j = 0; j < 16; ++j) {
                DAGNode.words[j] *= FNV_PRIME;
                DAGNode.words[j] ^= parent->words[j];
            }
        }

        SHA3_512(DAGNode.bytes, DAGNode.bytes, sizeof(Node));

        return DAGNode;
    }

    // OutHash & MixHash MUST have 32 bytes allocated (at least)
    // OutHash & MixHash are outputs of this function
    void
    LightEthash(uint8_t *MI_RESTRICT OutHash, uint8_t *MI_RESTRICT MixHash, const uint8_t *MI_RESTRICT HeaderPoWHash,
                const Node *Cache, const uint64_t EpochNumber, const uint64_t Nonce) {
        uint32_t MixState[32];
        uint32_t TmpBuf[24];
        auto NodeCount = static_cast<uint32_t>(EthGetCacheSize(EpochNumber) / sizeof(Node));
        uint64_t DagSize;

        // Initial hash - append nonce to header PoW hash and
        // run it through SHA3 - this becomes the initial value
        // for the mixing state buffer. The init value is used
        // later for the final hash, and is therefore saved.
        memcpy(TmpBuf, HeaderPoWHash, 32UL);
        MI_EXPECTS(endian::is_little);
        memcpy(TmpBuf + 8UL, &Nonce, 8UL);
        sha3_512((uint8_t *) TmpBuf, 64UL, (uint8_t *) TmpBuf, 40UL);

        memcpy(MixState, TmpBuf, 64UL);

        // The other half of the state is filled by simply
        // duplicating the first half of its initial value.
        memcpy(MixState + 16UL, MixState, 64UL);

        DagSize = EthGetDAGSize(EpochNumber) / (sizeof(Node) << 1);

        // Main mix of Ethash
        for (uint32_t i = 0, Init0 = MixState[0], MixValue = MixState[0]; i < 64; ++i) {
            auto row = static_cast<uint32_t>(fnv(Init0 ^ i, MixValue) % DagSize);
            Node DAGSliceNodes[2];
            DAGSliceNodes[0] = CalcDAGItem(Cache, NodeCount, row << 1);
            DAGSliceNodes[1] = CalcDAGItem(Cache, NodeCount, (row << 1) + 1);
            auto *DAGSlice = reinterpret_cast<DAG128 *>(DAGSliceNodes);

            for (uint32_t col = 0; col < 32; ++col) {
                MixState[col] = fnv(MixState[col], DAGSlice->Columns[col]);
                MixValue = col == ((i + 1) & 0x1F) ? MixState[col] : MixValue;
            }
        }

        // The reducing of the mix state directly into where
        // it will be hashed to produce the final hash. Note
        // that the initial hash is still in the first 64
        // bytes of TmpBuf - we're appending the mix hash.
        for (int i = 0; i < 8; ++i)
            TmpBuf[i + 16] = fnv_reduce(MixState + (i << 2));

        memcpy(MixHash, TmpBuf + 16, 32UL);

        // Hash the initial hash and the mix hash concatenated
        // to get the final proof-of-work hash that is our output.
        sha3_256(OutHash, 32UL, (uint8_t *) TmpBuf, 96UL);
    }


    EthashRegenhashResult ethash_regenhash(const Work<kEthash> &work, cByteSpan<> dagCache, uint64_t nonce) {
        EthashRegenhashResult result {};

        //LOG(DEBUG) << printfStr("Regenhash: First qword of input: 0x%016llX.", nonce);

        MI_EXPECTS(work.header.size() == 32); //assumed by LightEthash
        LightEthash(result.proofOfWorkHash.data(), result.mixHash.data(),
                work.header.data(), (Node *)dagCache.data(), work.epoch,
                    nonce);

        std::reverse(result.proofOfWorkHash.begin(), result.proofOfWorkHash.end());

        //LOG(DEBUG) << printfStr("Last ulong: 0x%016llX.", *(uint64_t *)&result.proofOfWorkHash[24]);
        return result;
    }

    // Output (cache_nodes) MUST have at least cache_size bytes
    static void EthGenerateCache(uint8_t *cache_nodes_inout, uint64_t cache_nodes_bytesize, cByteSpan<32> &seedhash)
    {
        auto const num_nodes = (uint32_t)(cache_nodes_bytesize / sizeof(Node));
        Node *cache_nodes = (Node *)cache_nodes_inout;

        sha3_512(cache_nodes[0].bytes, 64, seedhash.data(), 32);

        for(uint32_t i = 1; i != num_nodes; ++i) {
            SHA3_512(cache_nodes[i].bytes, cache_nodes[i - 1].bytes, 64);
        }

        for(uint32_t j = 0; j < 3; j++) { // this one can be unrolled entirely, ETHASH_CACHE_ROUNDS is constant
            for(uint32_t i = 0; i != num_nodes; i++) {
                uint32_t const idx = cache_nodes[i].words[0] % num_nodes;
                Node data;
                data = cache_nodes[(num_nodes - 1 + i) % num_nodes];
                for(uint32_t w = 0; w != 16; ++w) { // this one can be unrolled entirely as well
                    data.words[w] ^= cache_nodes[idx].words[w];
                }

                SHA3_512(cache_nodes[i].bytes, data.bytes, sizeof(data));
            }
        }
    }

    DynamicBuffer eth_gen_cache(uint32_t epoch, cByteSpan<32> &seedHash) {

        DynamicBuffer dagCache(EthGetCacheSize(epoch));
        EthGenerateCache(dagCache.data(), (size_t)dagCache.size_bytes(), seedHash);

        return dagCache;
    }
}