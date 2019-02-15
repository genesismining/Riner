#ifndef OPTIMINER_POW_CUCKOO_SIPHASH_H_
#define OPTIMINER_POW_CUCKOO_SIPHASH_H_

#ifndef __OPENCL_VERSION__
  #include <stdint.h>
#else
  typedef uchar uint8_t ;
  typedef char int8_t ;
  typedef ushort uint16_t;
  typedef short int16_t;
  typedef uint  uint32_t;
  typedef int   int32_t;
  typedef ulong uint64_t;
  typedef long  int64_t;
#endif

struct SiphashKeys {
    uint64_t k0;
    uint64_t k1;
};

#define ROTL(x,b) (uint64_t)( ((x) << (b)) | ( (x) >> (64 - (b))) )
#define SIPROUND \
        do { \
            v0 += v1; v2 += v3; v1 = ROTL(v1,13); \
            v3 = ROTL(v3,16); v1 ^= v0; v3 ^= v2; \
            v0 = ROTL(v0,32); v2 += v1; v0 += v3; \
            v1 = ROTL(v1,17); v3 = ROTL(v3,21);   \
            v1 ^= v2; v3 ^= v0; v2 = ROTL(v2,32); \
        } while(0)

inline uint64_t siphash24(struct SiphashKeys keys, uint64_t nonce) {
    uint64_t v0 = keys.k0 ^ 0x736f6d6570736575ULL;
    uint64_t v1 = keys.k1 ^ 0x646f72616e646f6dULL;
    uint64_t v2 = keys.k0 ^ 0x6c7967656e657261ULL;
    uint64_t v3 = keys.k1 ^ 0x7465646279746573ULL ^ nonce;
    SIPROUND;
    SIPROUND;
    v0 ^= nonce;
    v2 ^= 0xff;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    return (v0 ^ v1) ^ (v2 ^ v3);
}


#endif /* OPTIMINER_POW_CUCKOO_SIPHASH_H_ */

