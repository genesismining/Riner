//
//

#include "ComputeApiEnums.h"
#include <src/util/Logging.h>
#include <src/util/StringUtils.h>

namespace miner {

    std::string stringFromVendorEnum(VendorEnum e) {
        switch(e) {
            case kAMD:      return "AMD";
            case kNvidia:   return "Nvidia";
            case kIntel:    return "Intel";
            default: return "invalid algo type";
        }
    }

}
