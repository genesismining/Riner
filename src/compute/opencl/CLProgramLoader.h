
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
        /**
         * initialize with a directory string for the directory which contains the opencl kernels and the
         * directory where precompiled headers may get stored in.
         */
        CLProgramLoader(std::string clSourceDir, std::string precompiledKernelDir);

        /**
         * loads text from a file and compiles it to a cl::Program.
         * This function is thread safe. (see `clCreateProgramWithSource` & `clBuildProgram` documentation
         * for more details, as these functions are wrapped here)
         * param context the opencl context
         * param clFileInKernelDir path to the kernel file relative to the clSourceDir provided in the constructor (which comes from the config file's global settings ultimately)
         * return a cl::Program or nullopt if any complications happened from the loading of the file(s) to the compilation of the program.
         */
        optional<cl::Program> loadProgram(cl::Context context, std::string clFileInKernelDir,
                                          const std::string &clCompilerOptions) {
            std::vector<std::string> files = {std::move(clFileInKernelDir)};
            return loadProgram(std::move(context), files, clCompilerOptions);
        }
        
        /**
         * multi-file version of `loadProgram` above
         */
        optional<cl::Program> loadProgram(cl::Context context, const std::vector<std::string> &clFilesInKernelDir,
                                          const std::string &clCompilerOptions);
    };

}
