
#pragma once

#include <gsl/gsl_assert>
#include <src/util/Logging.h>

#ifndef NDEBUG
    //#define RNR_EXPECTS(x) Expects(x)
    //#define RNR_ENSURES(x) Ensures(x)

/**
 * specifies a precondition that is expected to be true
 * terminates the program if the condition is not met and the program is compiled in debug mode
 */
#define RNR_EXPECTS(x) \
  do { \
    if (!(x)) { \
        LOG(ERROR) << "assertion failed: " << #x; \
        std::terminate(); \
    } \
  } while (0)

/**
* specifies a postcondition that is expected to be made true by the code block above
* terminates the program if the condition is not met and the program is compiled in debug mode
*/
#define RNR_ENSURES(x) \
  do { \
    if (!(x)) { \
        LOG(ERROR) << "assertion failed: " << #x; \
        std::terminate(); \
    } \
  } while (0)
#else
    #define RNR_EXPECTS(x)
    #define RNR_ENSURES(x)
#endif
