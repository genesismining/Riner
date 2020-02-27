
#pragma once

#include <gsl/gsl_assert>
#include <src/util/Logging.h>

#ifndef NDEBUG
    //#define RNR_EXPECTS(x) Expects(x)
    //#define RNR_ENSURES(x) Ensures(x)

#define RNR_EXPECTS(x) \
  do { \
    if (!(x)) { \
        LOG(ERROR) << "assertion failed: " << #x; \
        std::terminate(); \
    } \
  } while (0)

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