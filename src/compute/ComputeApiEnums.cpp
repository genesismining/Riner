//
//

#include "ComputeApiEnums.h"
#include <src/util/Logging.h>
#include <src/util/StringUtils.h>

namespace riner {

    std::string stringFromVendorEnum(VendorEnum e) {
        switch(e) {
            case kUnknown:  return "unknown vendor";
            case kAMD:      return "AMD";
            case kNvidia:   return "Nvidia";
            case kIntel:    return "Intel";
            default: return "invalid vendor type";
        }
    }

}
