
#pragma once

#include <src/common/StringSpan.h>
#include <lib/cl2hpp/include/cl2.hpp>
#include <src/common/Optional.h>
#include <src/util/Bytes.h>

#include <vector>

namespace miner {

    class CLProgramLoader {

        optional<cl::Program> compileCLFile(cl::Context context, std::vector<std::string> sources, cstring_span options);

        const std::string clSourceDir;
        const std::string precompiledKernelDir;

    public:
        CLProgramLoader(std::string clSourceDir, std::string precompiledKernelDir);

        optional<cl::Program> loadProgram(cl::Context context, cstring_span clFileInDir, cstring_span clCompilerOptions) {
            std::vector<cstring_span> files = {clFileInDir};
            return loadProgram(context, std::move(files), clCompilerOptions);
        }

        optional<cl::Program> loadProgram(cl::Context context, const std::vector<cstring_span>& clFilesInDir, cstring_span clCompilerOptions);
    };

}
