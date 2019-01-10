
#pragma once

#include <gsl/gsl_assert>
#include <src/util/Logging.h>

#ifndef NDEBUG
    //#define MI_EXPECTS(x) Expects(x)
    //#define MI_ENSURES(x) Ensures(x)

#define MI_EXPECTS(x) \
  do { \
    if (!(x)) { \
        LOG(ERROR) << "assertion failed: " << #x; \
        std::terminate(); \
    } \
  } while (0)

#define MI_ENSURES(x) \
  do { \
    if (!(x)) { \
        LOG(ERROR) << "assertion failed: " << #x; \
        std::terminate(); \
    } \
  } while (0)
#else
    #define MI_EXPECTS(x)
    #define MI_ENSURES(x)
#endif