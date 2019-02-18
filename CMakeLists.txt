cmake_minimum_required(VERSION 3.9)
project(Miner)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -Wall -Werror=return-type -Wno-unknown-attributes -Wno-ignored-attributes -Wno-deprecated-declarations")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14 -Wall -Werror=return-type -Wno-unknown-attributes -Wno-ignored-attributes -Wno-deprecated-declarations")

#OpenCL
find_package(OpenCL REQUIRED)
include_directories(${OpenCL_INCLUDE_DIRS})
link_directories(${OpenCL_LIBRARY})

add_library(LibMiner
        src/application/Application.cpp src/application/Application.h
        src/application/Config.cpp src/application/Config.h
        src/application/Types.cpp src/application/Types.h
        src/application/LoggingMain.cpp
        src/algorithm/Algorithm.h
        src/algorithm/ethash/AlgoEthashCL.cpp
        src/algorithm/ethash/AlgoEthashCL.h
        src/algorithm/ethash/DagFile.cpp src/algorithm/ethash/DagFile.h
        src/algorithm/ethash/DagCache.cpp src/algorithm/ethash/DagCache.h
        src/algorithm/ethash/Ethash.cpp src/algorithm/ethash/Ethash.h
        src/algorithm/ethash/EthSha3.cpp src/algorithm/ethash/EthSha3.h
        src/algorithm/grin/siphash.h
        src/algorithm/grin/Graph.cpp src/algorithm/grin/Graph.h
        src/algorithm/grin/AlgoCuckatoo31Cl.cpp src/algorithm/grin/AlgoCuckatoo31Cl.h
        src/algorithm/grin/Cuckatoo.cpp src/algorithm/grin/Cuckatoo.h
        src/algorithm/AlgoFactory.cpp src/algorithm/AlgoFactory.h
        src/crypto/blake2b-ref.cpp src/crypto/blake2.h src/crypto/blake2-impl.h
        src/network/TcpLineProtocolUtil.cpp src/network/TcpLineProtocolUtil.h
        src/network/TcpJsonProtocolUtil.cpp src/network/TcpJsonProtocolUtil.h
        src/network/TcpJsonRpcProtocolUtil.cpp src/network/TcpJsonRpcProtocolUtil.h
        src/network/JrpcBuilder.cpp src/network/JrpcBuilder.h
        src/pool/PoolFactory.cpp src/pool/PoolFactory.h
        src/pool/WorkQueue.h
        src/pool/AutoRefillQueue.h
        src/pool/Pool.cpp src/pool/Pool.h
        src/pool/Work.cpp src/pool/Work.h
        src/pool/WorkEthash.h
        src/pool/PoolSwitcher.cpp src/pool/PoolSwitcher.h
        src/pool/PoolEthash.cpp src/pool/PoolEthash.h
        src/pool/PoolGrin.cpp src/pool/PoolGrin.h
        src/compute/DeviceId.cpp src/compute/DeviceId.h
        src/compute/ComputeModule.cpp src/compute/ComputeModule.h
        src/compute/ComputeApiEnums.h
        src/compute/opencl/CLProgramLoader.cpp src/compute/opencl/CLProgramLoader.h
        src/common/Endian.h
        src/common/WorkCommon.cpp src/common/WorkCommon.h
        src/common/Pointers.h
        src/common/StringSpan.h
        src/common/OpenCL.h
        src/common/Optional.h
        src/common/Variant.h
        src/common/Span.h
        src/common/Json.h
        src/common/JsonForward.h
        src/common/Assert.h
        src/common/PlatformDefines.h
        src/util/Bytes.h
        src/util/Copy.h
        src/util/Logging.h
        src/util/ConfigUtils.cpp src/util/ConfigUtils.h
        src/util/StringUtils.cpp src/util/StringUtils.h
        src/util/OptionalAccess.h
        src/util/LockUtils.cpp src/util/LockUtils.h
        src/util/FileUtils.cpp src/util/FileUtils.h
        src/util/HexString.cpp src/util/HexString.h
        src/util/DynamicBuffer.cpp src/util/DynamicBuffer.h
        src/util/sph/sph_keccak.h
        src/util/sph/sph_types.h

        lib/easyloggingpp/src/easylogging++.cc #TODO: this doesn't seem to belong here, figure out how to get easylogging working via the add_subdirectory
)

target_include_directories (LibMiner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(Miner src/application/Main.cpp)
target_link_libraries(Miner LibMiner)

target_include_directories (Miner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#add subdirectories
add_subdirectory("lib/gsl")
add_subdirectory("lib/json")
add_subdirectory("lib/type_safe")
add_subdirectory("lib/easyloggingpp")

target_compile_definitions(LibMiner PUBLIC CL_HPP_MINIMUM_OPENCL_VERSION=120)
target_compile_definitions(LibMiner PUBLIC CL_HPP_TARGET_OPENCL_VERSION=120)
target_compile_definitions(LibMiner PUBLIC ASIO_STANDALONE) #tell asio to not include boost headers
target_compile_definitions(LibMiner PUBLIC BOOST_ASIO_ENABLE_HANDLER_TRACKING) #async debugging handler tracking

target_compile_definitions(LibMiner PUBLIC ELPP_THREAD_SAFE) #tell easylogging++ to be threadsafe
#target_compile_definitions(LibMiner PUBLIC ELPP_DISABLE_LOGS) #disable all easylogging++ logging

#target_compile_definitions(LibMiner PUBLIC GSL_UNENFORCED_ON_CONTRACT_VIOLATION) #tell GSL to not perform performance heavy checks

include_directories(
        lib/asio/asio/include
        lib/cl2hpp/include
        lib/gsl/include
        lib/json/include
        lib/type_safe/include
        lib/type_safe/external/debug_assert
        lib/easyloggingpp/src
)

#OpenCL
target_link_libraries (Miner ${OpenCL_LIBRARY})
target_link_libraries (Miner -static-libgcc -static-libstdc++)

#Threads
find_package(Threads REQUIRED)
target_link_libraries (Miner Threads::Threads)

#Google Tests
include(GoogleTest)
find_package(GTest REQUIRED)
 
add_executable(tests 
    src/algorithm/grin/GraphTest.cpp
    src/network/JrpcBuilderTest.cpp
)
target_link_libraries(tests gtest gtest_main gmock  Threads::Threads)
target_link_libraries(tests LibMiner)
target_include_directories (Miner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}) 
 
gtest_add_tests(TARGET tests)
add_test(NAME monolithic COMMAND tests)
