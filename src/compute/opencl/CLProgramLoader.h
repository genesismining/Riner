
#pragma once

#include <src/common/StringSpan.h>
#include <lib/cl2hpp/include/cl2.hpp>
#include <src/common/Optional.h>
#include <src/util/Bytes.h>

namespace miner {

    class CLProgramLoader {

        optional<cl::Program> compileCLFile(cl::Context &, cstring_span clFile, cstring_span options);

        const std::string clSourceDir;
        const std::string precompiledKernelDir;

    public:
        CLProgramLoader(std::string clSourceDir, std::string precompiledKernelDir);

        optional<cl::Program> loadProgram(cl::Context &, cstring_span clFileInDir, cstring_span clCompilerOptions);
    };

}