# Miner (working title)

Early development version of a multi-algorithm GPU miner written in C++14.

Requires Cmake 3.12

## Goals

One of our main goals is to create a multi-algorithm miner which can be extended effortlessly with new algorithm types, algorithm implementations and  pool protocols, without requiring extensive knowledge
about the entire application.
Multiple compute and GPU monitoring APIs will be supported.


The following code snippets illustrate how we intend the subsystems to interact. Our current implementation may use a slightly different syntax though. Some of the shown functionality is also not yet implemented.

### RAII Objects

```cpp
auto pool = PoolEthashStratum{ url, usr, pwd };
//pool communication starts and runs in parallel until the object is destroyed

auto gpus = compute.getAllDevices(kDeviceTypeGPU);

auto algo = AlgoEthashCL{ gpus, pool };
//algorithm starts and runs in parallel until the object is destroyed
```

### Backup Pools

```cpp
//construct a pool switcher with one main pool and 2 backup pools
//all 3 connections are open simultaneously so that, in case a pool
//connection dies, work from another pool can be provided to the
//algorithm immediately

auto pools = PoolSwitcher{ 
    PoolEthashStratum{ url0, usr0, pwd0 },
    PoolEthashStratum{ url1, usr1, pwd1 },
    PoolEthashGetWork{ url2, usr2, pwd2 }
};

auto gpus = compute.getAllDevices(kDeviceTypeGPU);

auto algo = AlgoEthashCL{ gpus, pools };
```

### Pools: `Pool` Interface
Every Pool and PoolSwitcher implementation uses the Pool interface to talk to algorithms

```cpp
class Pool {
   virtual unique_ptr<Work> tryGetWork();
   virtual void submitWork(unique_ptr<WorkResult>);
};

//the pool classes from previous examples implement this interface
class PoolEthashStratum      : public Pool {
class PoolEthashGetWork      : public Pool {
class PoolCryptoNightStratum : public Pool {

class PoolSwitcher           : public Pool {
```


### Work, WorkResult
Each pool's work is communicated to the algorithm via the Work and WorkResult types of the respective algorithm-kind (e.g. Ethash, Equihash, CryptoNight) by calling `Pool::tryGetWork`.
Each Work type has a corresponding WorkResult type that is returned to the pool via `Pool::submitWork`
```cpp
Work<kEquihash>        ---.makeResult()-->  WorkResult<kEquihash>
Work<kCryptoNightV8>   ---.makeResult()-->  WorkResult<kCryptoNightV8>
Work<kEthash>          ---.makeResult()-->  WorkResult<kEthash>
```

For example `Work<kEthash>` could look like this:

```cpp
template<>
struct Work<kEthash> {
    Bytes<32> target;
    Bytes<32> headerHash;
    Bytes<32> seedHash;
    uint32_t extraNonce;
    uint32_t epoch;

    unique_ptr<ProtocolData> pdata; //contains protocol-specific 
    //information such as jobId for stratum. Each pool implementation
    //may specify their own ProtocolData subclass
}
```
With the corresponding result type `WorkResult<kEthash>`:

```cpp
template<>
struct WorkResult<kEthash> {
    uint64_t nonce;
    Bytes<32> proofOfWork;
    Bytes<32> mixHash;

    ProtocolData pdata;
}
```


The work loop of an algorithm's implementation may look similar to the following: (simplified)

```
void AlgoEquihashVk::perDeviceThreadFunction() {
    while (!shutdown) {

        if (auto work = pool.tryGetWork<kEthash>()) {

            if (work.expired()) //pools can flag work as expired
                continue;
                
            //makeResult copies stratum jobId implicitly
            auto result = work->makeResult();

            result.nonce       = ???;
            result.proofOfWork = ???;
            result.mixHash     = ???;

            pool.submitWork(std::move(result));
        }
    }
}
```

### Algorithms and their assigned devices

Every algorithm's implementation gets a set of device IDs (GPUs) passed in its
constructor. The implementation can then request handles specific to the compute APIs that the algorithm
intends to use.

```
AlgoEquihashCL::AlgoEquihashCL(AlgoArgs args)
: pool(args.workProvider) {
    
   for (auto &pcieId : args.assignedDevices) {
      
      //obtain compute api handles from pcieId
      cl::Device cl       = args.compute.getDeviceOpenCL(pcieId);
      VkPhysicalDevice vk = args.compute.getDeviceVulkan(pcieId); //example. unused

      launchThread(perDeviceThreadFunction, std::move(cl));
   }
}
```

## License

This project is licensed under the terms of the GPLv3 - see [LICENSE.md](LICENSE.md)


