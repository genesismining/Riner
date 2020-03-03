cmake_minimum_required(VERSION 3.9)
project(riner)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS}         -Wall -Werror=return-type -Wno-unknown-attributes -Wno-ignored-attributes -Wno-deprecated-declarations -Wno-noexcept-type")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   ${CMAKE_CXX_FLAGS} -Og -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS} -O2 -mtune=native -msse4.1")

#download external ethash OpenCL kernel
file(DOWNLOAD https://raw.githubusercontent.com/genesismining/sgminer-gm/bc138413d9098d6ed285df1bdebc70b47f69bcac/kernel/ethash-new.cl ${CMAKE_CURRENT_SOURCE_DIR}/src/kernel/ethash.cl TLS_VERIFY ON STATUS FILE_RET)
if(NOT ("0" IN_LIST FILE_RET))
    list(GET FILE_RET 1 ERROR)
    message(WARNING "Download of ethash.cl kernel failed: ${ERROR}")
endif()

#Protocol Buffers
find_package(Protobuf REQUIRED)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
file(GLOB_RECURSE ProtoFiles "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")
protobuf_generate_cpp(ProtoSources ProtoHeaders ${ProtoFiles})
#message(STATUS "ProtoSources: ${ProtoSources}")
#message(STATUS "ProtoHeaders: ${ProtoHeaders}")
#add_library(lib_riner STATIC ${ProtoSources} ${ProtoHeaders})

add_library(lib_riner
        ${ProtoSources} ${ProtoHeaders}
        src/application/Application.cpp src/application/Application.h
        src/application/Registry.cpp src/application/Registry.h src/application/RegistryImpl.cpp
        src/application/ShutdownState.cpp src/application/ShutdownState.h
        src/application/ApiServer.cpp src/application/ApiServer.h
        src/application/Device.cpp src/application/Device.h
        src/application/CLI.cpp src/application/CLI.h
        src/application/LoggingMain.cpp
        src/config/Config.cpp src/config/Config.h
        src/config/ConfigDefaultValues.h
        src/algorithm/Algorithm.cpp src/algorithm/Algorithm.h
        src/algorithm/ethash/AlgoEthashCL.cpp src/algorithm/ethash/AlgoEthashCL.h
        src/algorithm/ethash/DagFile.cpp src/algorithm/ethash/DagFile.h
        src/algorithm/ethash/DagCache.cpp src/algorithm/ethash/DagCache.h
        src/algorithm/ethash/EthSha3.cpp src/algorithm/ethash/EthSha3.h
        src/algorithm/dummy/AlgoDummy.cpp src/algorithm/dummy/AlgoDummy.h
        src/statistics/StatisticNode.cpp src/statistics/StatisticNode.h
        src/statistics/Average.cpp src/statistics/Average.h
        src/statistics/DeviceRecords.cpp src/statistics/DeviceRecords.h
        src/statistics/PoolRecords.cpp src/statistics/PoolRecords.h
        src/kernel/siphash.h
        src/algorithm/grin/Graph.cpp src/algorithm/grin/Graph.h
        src/algorithm/grin/AlgoCuckatoo31Cl.cpp src/algorithm/grin/AlgoCuckatoo31Cl.h
        src/algorithm/grin/Cuckatoo.cpp src/algorithm/grin/Cuckatoo.h
        src/crypto/blake2b-ref.cpp src/crypto/blake2.h src/crypto/blake2-impl.h
        src/network/IOTypeLayer.h
        src/network/JsonIO.cpp src/network/JsonIO.h
        src/network/LineIO.cpp src/network/LineIO.h
        src/network/BaseIO.cpp src/network/BaseIO.h
        src/network/JsonRpcIO.cpp src/network/JsonRpcIO.h
        src/network/JsonRpcMessage.cpp src/network/JsonRpcMessage.h
        src/network/JsonRpcBuilder.cpp src/network/JsonRpcBuilder.h
        src/network/JsonRpcInvoke.h
        src/network/JsonRpcMethod.cpp src/network/JsonRpcMethod.h
        src/network/JsonRpcHandlerMap.cpp src/network/JsonRpcHandlerMap.h
        src/network/JsonRpcUtil.cpp src/network/JsonRpcUtil.h
        src/network/SslDesc.h
        src/network/Socket.cpp src/network/Socket.h
        src/network/CxnHandle.cpp src/network/CxnHandle.h
        src/pool/Pool.cpp src/pool/Pool.h
        src/pool/Work.cpp src/pool/Work.h
        src/pool/WorkCuckoo.h
        src/pool/WorkEthash.h
        src/pool/WorkQueue.h
        src/pool/PoolSwitcher.cpp src/pool/PoolSwitcher.h
        src/pool/PoolEthash.cpp src/pool/PoolEthash.h
        src/pool/PoolDummy.cpp src/pool/PoolDummy.h
        src/pool/PoolGrin.cpp src/pool/PoolGrin.h
        src/compute/DeviceId.cpp src/compute/DeviceId.h
        src/compute/ComputeModule.cpp src/compute/ComputeModule.h
        src/compute/ComputeApiEnums.cpp src/compute/ComputeApiEnums.h
        src/compute/opencl/CLProgramLoader.cpp src/compute/opencl/CLProgramLoader.h
        src/common/Chrono.h
        src/common/Endian.h
        src/common/Pointers.h
        src/common/OpenCL.h
        src/common/Optional.h
        src/common/Variant.h
        src/common/Span.h
        src/common/Json.h
        src/common/JsonForward.h
        src/common/Assert.h
        src/common/PlatformDefines.h
        src/common/Future.h
        src/gpu_api/GpuApi.cpp src/gpu_api/GpuApi.h
        src/gpu_api/AmdgpuApi.cpp src/gpu_api/AmdgpuApi.h
        src/util/Bytes.h
        src/util/Copy.h
        src/util/Logging.h
        src/util/TemplateUtils.h
        src/util/ConfigUtils.cpp src/util/ConfigUtils.h
        src/util/Random.h
        src/util/StringUtils.cpp src/util/StringUtils.h
        src/util/TaskExecutorPool.h src/util/TaskExecutorPool.cpp
        src/util/OptionalAccess.h
        src/util/LockUtils.cpp src/util/LockUtils.h
        src/util/FileUtils.cpp src/util/FileUtils.h
        src/util/HexString.cpp src/util/HexString.h
        src/util/AsioErrorUtil.cpp src/util/AsioErrorUtil.h
        src/util/DifficultyTarget.cpp src/util/DifficultyTarget.h
        src/util/DynamicBuffer.h
        src/util/Barrier.cpp src/util/Barrier.h
        src/util/TestSslData.h
        )

target_include_directories (lib_riner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(riner src/application/Main.cpp)
target_link_libraries(riner lib_riner)

target_include_directories (riner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(JSON_BuildTests OFF CACHE BOOL "build json tests") #disable building json tests by default

#add subdirectories
add_subdirectory("lib/gsl")
add_subdirectory("lib/json")
add_subdirectory("lib/easyloggingpp")

target_compile_definitions(lib_riner PUBLIC CL_HPP_MINIMUM_OPENCL_VERSION=120)
target_compile_definitions(lib_riner PUBLIC CL_HPP_TARGET_OPENCL_VERSION=120)
target_compile_definitions(lib_riner PUBLIC ASIO_STANDALONE) #tell asio to not include boost headers
#target_compile_definitions(lib_riner PUBLIC BOOST_ASIO_ENABLE_HANDLER_TRACKING) #async debugging handler tracking

target_compile_definitions(lib_riner PUBLIC ELPP_THREAD_SAFE ELPP_FEATURE_CRASH_LOG) #tell easylogging++ to be threadsafe
#target_compile_definitions(lib_riner PUBLIC ELPP_DISABLE_LOGS) #disable all easylogging++ logging

#target_compile_definitions(lib_riner PUBLIC GSL_UNENFORCED_ON_CONTRACT_VIOLATION) #tell GSL to not perform performance heavy checks

include_directories(
        lib/asio/asio/include
        lib/cl2hpp/include
        lib/gsl/include
        lib/json/include
        lib/Optional
        lib/variant
        lib/easyloggingpp/src
)

if(APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)

    #asio string_view fix on MacOS
    target_compile_definitions(lib_riner PUBLIC ASIO_DISABLE_STD_EXPERIMENTAL_STRING_VIEW)
endif()

#Protocol Buffers
target_link_libraries(lib_riner ${PROTOBUF_LIBRARY})

#OpenCL
find_package(OpenCL REQUIRED)
target_link_libraries (lib_riner OpenCL::OpenCL)

#OpenSSL
find_package(OpenSSL)
if (OpenSSL_FOUND)
    target_link_libraries (lib_riner OpenSSL::SSL)
    target_compile_definitions(lib_riner PUBLIC HAS_OPENSSL)
else()
    message(WARNING "OpenSSL not found - building without TLS support")
endif()

#Threads
find_package(Threads REQUIRED)
target_link_libraries (riner Threads::Threads)

#Google Tests
find_package(GTest)

if (GTest_FOUND)
    enable_testing()
    include(GoogleTest)
    add_executable(tests
        src/algorithm/grin/CuckatooTest.cpp
        src/algorithm/grin/GraphTest.cpp
        src/config/ConfigTest.cpp
        src/pool/WorkCuckatoo31Test.cpp
        src/network/JrpcTest.cpp
        src/application/TestMain.cpp)
    target_link_libraries(tests gmock GTest::GTest)
    target_link_libraries(tests lib_riner)

    gtest_add_tests(TARGET tests)
    #add_test(NAME monolithic COMMAND tests)
endif()
