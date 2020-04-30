
#pragma once

#include <string>

namespace riner {

    enum CompApi {
        kOpenCL,
        kVulkan,

        kCompApiCount
    };

    /**
     * enum representing common compute device vendors. used by `DeviceId`
     */
    enum VendorEnum {
        kUnknown,
        kNvidia,
        kAMD,
        kIntel,

        kVendorEnumCount
    };

    /**
     * convert Vendor enum to a printable string (e.g. "AMD" for kAMD)
     */
    std::string stringFromVendorEnum(VendorEnum e);

}
