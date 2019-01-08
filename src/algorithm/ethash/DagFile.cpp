

#include "DagFile.h"
#include "Ethash.h"
#include <src/util/Logging.h>

namespace miner {

    uint32_t DagFile::getEpoch() const {
        return epoch;
    }

    DagFile::operator bool() const {
        return valid;
    }

    size_t DagFile::getSize() const {
        return size;
    }

    bool DagFile::generate(const DagCacheContainer &cache, cByteSpan<32> seedHash,
                  const cl::Context &context, const cl::Device &device, cl::Program &generateDagProgram) {

        cl_int err;
        auto epoch = cache.getEpoch();

        //allocate dag memory on the gpu
        const auto futureEpochs = 6; //preallocate this far ahead

        if (allocationMaxEpoch < epoch) {
            allocationMaxEpoch = epoch + futureEpochs;

            clDagBuffer = cl::Buffer{context,
                CL_MEM_READ_WRITE |
                CL_MEM_HOST_NO_ACCESS,
                EthGetDAGSize(allocationMaxEpoch), nullptr, &err
            };

            if (err) {
                LOG(ERROR) << "allocation of clDagBuffer failed";
                return false;
            }

        }
        MI_ENSURES(clDagBuffer());

        //upload dag cache which is required for dag creation
        auto cacheSpan = cache.getCache();

        cl::Buffer clDagCache{context,
            CL_MEM_READ_ONLY |
            CL_MEM_COPY_HOST_PTR |
            CL_MEM_HOST_WRITE_ONLY,
            (size_t)cacheSpan.size(),
            (void *)cacheSpan.data(), &err
        };

        if (err) {
            LOG(ERROR) << "#" << err << " creation of clDagCache failed";
            return false;
        }

        auto genDagKernel = cl::Kernel(generateDagProgram, "GenerateDAG", &err);
        if (err) {
            LOG(ERROR) << "#" << err << " could not get dag generation kernel from cl program";
            return false;
        }

        cl::CommandQueue cmdQueue(context, device, 0, &err);
        if (err) {
            LOG(ERROR) << "#" << err << " failed creating a command queue for dag generation";
            return false;
        }

        //run the kernel
        cl_uint arg = 0;
        genDagKernel.setArg(arg++, 0);
        genDagKernel.setArg(arg++, clDagCache);
        genDagKernel.setArg(arg++, clDagBuffer);
        genDagKernel.setArg(arg++, cacheSpan.size() / 64);

        auto dagSize = EthGetDAGSize(epoch);
        cl::NDRange offset{0};
        cl::NDRange dagItems{dagSize / 64};

        cmdQueue.enqueueNDRangeKernel(genDagKernel, offset, dagItems);
        cmdQueue.finish(); //clFinish();

        clDagCache = {}; //clReleaseMemObject

        this->epoch = epoch;
        this->size = dagSize;
        MI_EXPECTS(this->size != 0);
        valid = true;
        return true;
    }

    cl::Buffer &DagFile::getCLBuffer() {
        MI_EXPECTS(valid);
        return clDagBuffer;
    }





}