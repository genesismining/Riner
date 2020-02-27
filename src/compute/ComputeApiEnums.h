
#pragma once

#include <string>

namespace riner {

    enum CompApi {
        kOpenCL,
        kVulkan,

        kCompApiCount
    };

    enum VendorEnum {
        kUnknown,
        kNvidia,
        kAMD,
        kIntel,

        kVendorEnumCount
    };


    std::string stringFromVendorEnum(VendorEnum e);

}