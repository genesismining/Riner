.. image:: ../assets/img_banner.jpg

| 

Introduction
===============

.. note::

    Please note that this documentation is still very much under construction. More details will be added soon.

What is Riner
-------------

Riner is a cryptocurrency mining application that can run a variety of "proof of work" algorithms on GPUs and communicate using different pool mining protocols.
The codebase is designed to make adding new algorithms as well as new pool protocols as simple and quick as possible.

Getting Started
================

Dependencies
------------
In order to build Riner the following dependencies need to be installed:

- OpenCL

- `CMake <https://cmake.org/>`_

- `GoogleTest <https://github.com/google/googletest>`_

.. | `OpenSSL <https://www.openssl.org/source/>`_

Build
-----

first, clone the Riner repository and update submodules

.. code::

    git clone https://github.com/genesismining/Riner.git
    cd Riner
    git submodule update

then build the project using :code:`cmake` and :code:`make` or configure CMake to output build files for your favourite IDE.

.. code::

    cmake .
    make

.. note:: 

    Running cmake will also download additional files such as ethash.cl from other repositories.

now if we run the resulting executable, the following message will show up in the console output:

.. code::

    no config path command line argument (--config /path/to/config.json)

Riner requires a .json configuration file to run any algorithms. For now only a basic OpenCL implementation of ethash can be run (which requires a GPU with 4GB of memory). Alternatively an example algorithm is available that can be used as a starting point for implementing your own proof of work in Riner.

In either case, providing a configuration file as described in the following section is required

Configuration
-------------

Here is an example of a valid config.json file

.. code::

    {
        "config_version": "1.0",
        "global_settings": {
            "temp_cutoff": 85,
            "temp_overheat": 80,
            "temp_target": 76,
            "api_port": 4028,
            "opencl_kernel_dir": "dir/containing/kernels",
            "start_profile": "myProfile"
        },
        "pools": [
            {
                "type": "ethash",
                "protocol": "stratum2",
                "host": "eth-eu1.nanopool.org",
                "port": 9999,
                "username": "0x0",
                "password": "x"
            },
        ],
        "device_profiles": {
            "myGpu": {
                "comment": "example GPU profile",
                "algorithms": {
                    "comment": "these settings will be used if AlgoEthashCL is launched with a gpu that has the "myGpu" profile assigned",

                    "AlgoEthashCL": {
                        "core_clock_MHz": 1145,
                        "memory_clock_MHz": 2100,
                        "power_limit_W": 110,
                        "core_voltage_mV": 850,
                        "num_threads": 1,
                        "work_size": 128,
                        "raw_intensity": 4194304 
                    }
                }
            }
        },
        "profiles": {
            "myProfile": {
                "device_default": ["myGpu", "AlgoEthashCL"]
            }
        }
    }

The root json object that must contain the following keys:

- "config_version"
    version of the config file, must be "1.0"

- "pools"
    json list of pool objects.

- "device_profiles"
    json object with device profiles as named members. Device profiles are settings which can later be assigned to specific devices (GPUs)

- "profiles"
    json object with profiles as named members. A profile maps device profiles to specific devices (GPUs). The 

- "global_settings"
    a json object that contains parameters. Most importantly:

    - "start_profile" 
        the profile that gets started when the Riner executable is run
    - "opencl_kernel_dir"
        path to the directory containing OpenCL kernel files (such as ethash.cl for that was downloaded via cmake) which will be compiled on demand

.. note:: 

    A more comprehensive documentation of the config file will be added as soon as its structure is finalized.

Running Riner
-------------

in order to run ethash, the "opencl_kernel_dir" option in "global_settings" must be set to the directory that ethash.cl was downloaded into by cmake.
If your system doesn't have a GPU installed that is sufficient for running ethash, but you still like to contribute to Riner, you can use the example algorithm by entering "AlgoDummy" instead of "AlgoEthashCL" in the "myProfile" profile.
