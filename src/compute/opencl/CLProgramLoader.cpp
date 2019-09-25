
#include <utility>
#include <src/common/Variant.h>
#include <src/common/Optional.h>
#include "CLProgramLoader.h"
#include <src/util/FileUtils.h>
#include <src/util/Logging.h>
#include <condition_variable>

namespace miner {

    CLProgramLoader::CLProgramLoader(std::string clSourceDir, std::string precompiledKernelDir)
    : clSourceDir(std::move(clSourceDir))
    , precompiledKernelDir(std::move(precompiledKernelDir)) {
    }

    opt::optional<cl::Program> CLProgramLoader::loadProgram(cl::Context context, const std::vector<cstring_span>& clFilesInDir, cstring_span options) {
        std::vector<std::string> sources;
        for(auto& file: clFilesInDir) {
            std::string clFilePath = clSourceDir;
            clFilePath.append(file.data(), static_cast<size_t>(file.size()));

            opt::optional<std::string> source = file::readFileIntoString(clFilePath);
            if (!source) {
                LOG(ERROR) << "failed to read " << clFilePath;
                return opt::nullopt;
            }
            sources.push_back(std::move(*source));
        }
        return compileCLFile(context, sources, options);
    }

    opt::optional<cl::Program> CLProgramLoader::compileCLFile(cl::Context context, std::vector<std::string> sources, cstring_span options) {
        cl_int err = 0;

        std::string fullOptions = " -I " + clSourceDir + " ";
        fullOptions.append(options.data(), (size_t)options.size());

        cl::Program program(context, sources, &err);

        { //compilation
            std::mutex mut;
            std::condition_variable cv;
            std::unique_lock<std::mutex> lock(mut);

            auto onDidCompile = [] (cl_program, void *cv) {
                ((std::condition_variable *) cv)->notify_one();
            };

            program.build(fullOptions.c_str(), onDidCompile, &cv); //does not block until compilation is finished
            cv.wait(lock, [&] {//wait until no device is building anymore
                auto devicesStatus = program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>();
                for (auto &pair : devicesStatus) {
                    auto status = pair.second;
                    if (status == CL_BUILD_IN_PROGRESS) {
                        return false;
                    }
                }
                return true;
            });
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

                LOG(ERROR) << "#" << status << " failed building cl program on device '" << name <<
                "\nwith options '" << fullOptions << "'" <<
                "\nbuild log: \n" << log;
            }
        }

        if (atLeastOneFailed) {
            return opt::nullopt;
        }
        return std::move(program);
    }

}
