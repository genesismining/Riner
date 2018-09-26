
#pragma once

#include <gsl/gsl_assert>

#ifndef NDEBUG
    #define MI_EXPECTS(x) Expects(x)
    #define MI_ENSURES(x) Ensures(x)
#else
    #define MI_EXPECTS(x)
    #define MI_ENSURES(x)
#endif