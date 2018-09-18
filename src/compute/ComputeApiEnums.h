
#pragma once

namespace miner {

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

}