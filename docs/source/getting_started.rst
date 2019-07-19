Getting started
===============

Early development version of a multi-algorithm GPU miner written in C++14.
Requires Cmake 3.12

Goals
-----

One of our main goals is to create a multi-algorithm miner which can be extended effortlessly with new algorithm types, algorithm implementations and  pool protocols, without requiring extensive knowledge
about the entire application.
Multiple compute and GPU monitoring APIs will be supported.

The following code snippets illustrate how we intend the subsystems to interact. Our current implementation may use a slightly different syntax though. Some of the shown functionality is also not yet implemented.

RAII objects
------------

.. code-block:: c++
   :linenos:

   auto pool = PoolEthashStratum{ url, usr, pwd };
   //pool communication starts and runs in parallel until the object is destroyed

   auto gpus = compute.getAllDevices(kDeviceTypeGPU);

   auto algo = AlgoEthashCL{ gpus, pool };
   //algorithm starts and runs in parallel until the object is destroyed


Backup Pools
------------

.. code-block:: cpp

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



