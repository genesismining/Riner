cmake_minimum_required(VERSION 3.12)
project(Miner)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wno-unknown-attributes")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -Wno-unknown-attributes")

#OpenCL
find_package(OpenCL REQUIRED)
include_directories(${OpenCL_INCLUDE_DIRS})
link_directories(${OpenCL_LIBRARY})

add_executable(Miner
        src/application/Main.cpp
        src/application/Application.cpp src/application/Application.h
        src/network/TcpLineSubscription.cpp src/network/TcpLineSubscription.h
        src/network/TcpJsonSubscription.cpp src/network/TcpJsonSubscription.h
        src/common/Pointers.h
        src/common/StringSpan.h
        src/common/OpenCL.h
        src/common/Optional.h
        src/common/Span.h
        src/common/Json.h
        src/util/Logging.h

        lib/easyloggingpp/src/easylogging++.cc #TODO: this doesn't seem to belong here, figure out how to get easylogging working via the add_subdirectory
        src/util/FileUtils.cpp src/util/FileUtils.h)

target_include_directories (Miner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#add subdirectories
add_subdirectory("lib/gsl")
add_subdirectory("lib/json")
add_subdirectory("lib/type_safe")
add_subdirectory("lib/easyloggingpp")

add_compile_definitions(ASIO_STANDALONE) #tell asio to not include boost headers

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