.. image:: ../assets/img_banner_white.jpg

| 

Introduction
===============

What is Riner
-------------

Riner is a cryptocurrency mining application that can run a variety of "proof of work" algorithms on GPUs and communicate using different pool mining protocols.
The codebase is designed to make adding new algorithms as well as new pool protocols as simple and quick as possible.

Getting Started
================

Dependencies
------------

In order to build Riner the following dependencies should be installed:

- C++ compiler with support of the C++-14 standard

- `CMake <https://cmake.org/>`_ (version 3.9 or later)

- Build tool supported by :code:`cmake` like :code:`make` (Unix), :code:`Visual Studio` (Windows), :code:`Xcode` (macOS) etc.

- `OpenCL <https://www.khronos.org/registry/OpenCL/specs/2.2/html/OpenCL_ICD_Installation.html>`_

- `Protocol Buffers <https://github.com/protocolbuffers/protobuf>`_

- for SSL/TLS support (optional): `OpenSSL <https://www.openssl.org/source/>`_

- for building tests (optional): `GoogleTest <https://github.com/google/googletest>`_

Build
-----

First, install the required dependencies. On Ubuntu 18.04 the following packages might be installed:

.. code::

    sudo apt install cmake make g++
    sudo apt install opencl-headers ocl-icd-libopencl1
    # if /usr/lib/libOpenCL.so does not exist, then create it as symlink to /usr/lib/x86_64-linux-gnu/libOpenCL.so.1
    sudo apt install libprotobuf-dev protobuf-compiler
    sudo apt install libssl-dev
    sudo apt install googletest && ( cd /usr/src/googletest; sudo cmake . && sudo make install; cd - )
    sudo apt install git

Some distributions split :code:`GoogleTest` into two packages, e.g. :code:`gtest` and :code:`gmock`.

Afterwards, clone the Riner repository and update submodules

.. code::

    git clone --recurse-submodules https://github.com/genesismining/Riner.git

Then build the project using :code:`cmake` and :code:`make` or configure CMake to output build files for your favourite IDE.

.. code::

    cmake .
    make riner

.. note:: 

    Running cmake will also download additional files such as ethash.cl from other repositories.

Now if we run the resulting executable, the following message will show up in the console output:

.. code::

    no config path command line argument (--config=/path/to/config.textproto)

Riner requires a .textproto configuration file to run any algorithms. For now only a basic OpenCL implementation of Cuckatoo31 and ethash can be run, which requires a GPU with 4GB of memory and a working OpenCL driver. Alternatively an example algorithm is available that can be used as a starting point for implementing your own proof of work in Riner.

In either case, providing a configuration file as described in the following section is required

Configuration
-------------

Here is an example of a valid config.textproto file

.. code::

    version: "0.1"

    global_settings {
        api_port: 4028
        opencl_kernel_dir: "kernel/dir/" #dir which contains the opencl kernel files (e.g. ethash.cl)
        start_profile_name: "my_profile" #when running Riner with this config file, the tasks of "my_profile" get launched
    }

    profile {
        name: "my_profile"
        # now specify tasks that will run in parallel as this profile is started

        # Task 1: run AlgoEthashCL on all devices
        task {
            run_on_all_remaining_devices: true
            #run this task on every gpu except gpu 1, which will run the task below

            run_algoimpl_with_name: "EthashCL"
            #run the OpenCL ethash AlgoImpl. run riner --list-algoimpls for a list of all available AlgoImpls

            use_device_profile_with_name: "my_default_gpu_profile" 
            #my_gpu_profile has specific settings for AlgoEthashCL for this gpu type.
        }

        # Task 1: run AlgoCuckatoo31 on GPU 1 (use --list-devices to find out about indices)
        task {
            device_index: 1
            #gpu 1 will not run the task above, but this one.

            run_algoimpl_with_name: "Cuckatoo31Cl"

            use_device_profile_with_name: "my_AMD_gpu_profile" 
            #my_gpu_profile has specified other settings for AlgoCuckatoo31Cl
        }
    }

    device_profile {
        name: "my_AMD_gpu_profile"
        #device profile, example name suggests they should be used for AMD gpus

        settings_for_algoimpl { #key value pairs
            key: "EthashCL"
            #these settings are applied if AlgoEthashCL is started with this gpu profile

            value {
            num_threads: 4
            work_size: 1024
            }
        }

        settings_for_algoimpl {
            key: "Cuckatoo31Cl"

            value: { #settings:
            work_size: 512
            }
        }
    }

    device_profile {
        name: "my_default_gpu_profile"
        #generic device profile for no particular gpu

        settings_for_algoimpl {
            key: "EthashCL"
            
            value: {
            num_threads: 4
            work_size: 1024
            }
        }
    }

    # pools are listed by priority. 
    # e.g. if there are two ethash pools, the top most one will get used first, 
    # if that pool is unresponsive, work will get taken from the next ethash pool instead.
    # To prevent idle time in such a case, riner has connections to all pools, even unused ones.
    # if an unresponsive pool reawakens, its work will get used again.

    pool { #if this pool is active it will provide work to all running AlgoImpls that are of pow_type "ethash"
        pow_type: "ethash"
        protocol: "stratum2"
        host: "127.0.0.1"
        port: 2345
        username: "user"
        password: "password"
    }

    pool {
        pow_type: "cuckatoo31"
        protocol: "stratum"
        host: "127.0.0.1"
        port: 2346
        username: "user"
        password: "password"
    }

    pool { #2nd ethash pool as a backup if the first one is unresponsive
        pow_type: "ethash"
        protocol: "stratum2"
        host: "127.0.0.1"
        port: 2347
        username: "user"
        password: "password"
    }

.. note:: 
    For more detailed information about the config file structure, see the protobuf source "Config.proto" in the `src/config` folder.

Running Riner
-------------

in order to run ethash, the "opencl_kernel_dir" option in "global_settings" must be set to the directory that ethash.cl was downloaded into by cmake.

Running Tutorial Code
---------------------

If you want to get started adding new algorithms or pool protocols to Riner, the quickest way to get started is looking at the "AlgoDummy" and "PoolDummy" classes, which are extensively documented.
To run the tutorial code, just run

.. code::
    ./riner --run-tutorial

and read the log messages (you can change log verbosity via the -v=X command)