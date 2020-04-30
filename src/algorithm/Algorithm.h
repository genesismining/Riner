

#pragma once

#include <src/compute/ComputeModule.h>
#include <src/util/Copy.h>
#include <src/util/ConfigUtils.h>
#include <src/pool/Pool.h>

#include <vector>

namespace riner {

    /**
     * @brief Args passed into every Algorithm subclass ctor
     * If you want to add args to every algorithm ctor, add them to this struct instead
     */
    struct AlgoConstructionArgs {
        ComputeModule &compute;
        std::vector<std::reference_wrapper<Device>> assignedDevices;
        Pool &workProvider; //reference to the poolswitcher
    };

    /**
     * @brief common base class for all AlgoImpls
     * when subclassing Algorithm, do not forget to add your algorithm to the registry in 'Registry.cpp'
     */
    class Algorithm {
    public:
        virtual ~Algorithm() = default;

        DELETE_COPY_AND_MOVE(Algorithm);

    protected:
        Algorithm() = default; //only instanciable from subclasses
    };

}
