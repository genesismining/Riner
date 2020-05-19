//
//

#pragma once

#include <src/util/Logging.h>
#include <lib/cl2hpp/include/cl2.hpp>

namespace riner {
    
    /**
     * @param err an opencl opencl error value
     * @return a c_string containing the opencl error macro name (e.g. CL_SUCCESS => "CL_SUCCESS")
     */
    const char *cl_error_name(cl_int err);

#define RNR_RETURN_ON_CL_ERR(err, msg, return_) \
do { \
    if (err) { \
        LOG_N_TIMES(10, ERROR) << "cl error: " << cl_error_name(err) << " "; \
        return return_ ; \
    } \
} \
while (false)

#define RNR_CL_ERR(err, msg) \
do { \
    if (err) { \
        LOG_N_TIMES(10, ERROR) << "cl error: " << cl_error_name(err) << " "; \
    } \
} \
while (false)

}
