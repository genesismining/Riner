
#include <utility>
#include <src/common/Variant.h>
#include <src/common/Optional.h>
#include "CLProgramLoader.h"
#include <src/util/FileUtils.h>
#include <src/util/Logging.h>

namespace miner {

    CLProgramLoader::CLProgramLoader(std::string clSourceDir, std::string precompiledKernelDir)
    : clSourceDir(std::move(clSourceDir))
    , precompiledKernelDir(std::move(precompiledKernelDir)) {
    }

    optional<cl::Program> CLProgramLoader::loadProgram(cl::Context &context, cstring_span clFileInDir, cstring_span options) {
        auto clFilePath = clSourceDir;
        clFilePath.append(clFileInDir.data(), static_cast<size_t>(clFileInDir.size()));

        return compileCLFile(context, clFilePath, options);
    }

    optional<cl::Program> CLProgramLoader::compileCLFile(cl::Context &context, cstring_span clFile, cstring_span options) {
        auto source = file::readFileIntoString(clFile);
        if (!source)
            return nullopt;

        cl_int err = 0;

        std::string fullOptions = " -I " + clSourceDir + " ";
        fullOptions.append(options.data(), (size_t)options.size());

        cl::Program program(context, {source.value()}, err);

        { //compilation
            std::mutex mut;
            mut.lock();

            auto onDidCompile = [] (cl_program, void *mut) {
                ((std::mutex *) mut)->unlock();
            };

            program.build(fullOptions.c_str(), onDidCompile, &mut); //does not block until compilation is finished
            mut.lock(); //waits until onDidCompile was called
            mut.unlock();
        }

        bool atLeastOneFailed = false;

        auto devicesStatus = program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>();
        for (auto &pair : devicesStatus) {
            auto &device = pair.first;
            auto status = pair.second;

            if (status != CL_BUILD_SUCCESS) {
                atLeastOneFailed = true;

                auto name = device.getInfo<CL_DEVICE_NAME>();
                auto log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);

                LOG(ERROR) << "#" << status << " failed building cl program " << gsl::to_string(clFile) <<
                "\non device '" << name <<
                "\nwith options '" << fullOptions << "'" <<
                "\nBuild Log: \n" << log;
            }
        }

        if (atLeastOneFailed)
            return nullopt;
        return std::move(program);
    }

}