
#pragma once

#include <lib/cl2hpp/include/cl2.hpp>
#include <src/common/Optional.h>
#include <src/util/Bytes.h>

#include <vector>

namespace riner {

    class CLProgramLoader {

        optional<cl::Program> compileCLFile(cl::Context context, const std::vector<std::string> &sources,
                                            const std::string &options);

        const std::string clSourceDir;
        const std::string precompiledKernelDir;

    public:
        CLProgramLoader(std::string clSourceDir, std::string precompiledKernelDir);

        optional<cl::Program> loadProgram(cl::Context context, std::string clFileInDir,
                                          const std::string &clCompilerOptions) {
            std::vector<std::string> files = {std::move(clFileInDir)};
            return loadProgram(std::move(context), files, clCompilerOptions);
        }

        optional<cl::Program> loadProgram(cl::Context context, const std::vector<std::string> &clFilesInDir,
                                          const std::string &clCompilerOptions);
    };

}
