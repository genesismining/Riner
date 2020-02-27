#pragma once

#if defined (__unix__)
#   define RNR_PLATFORM_UNIX
#endif

#if defined (__linux__)
#   define RNR_PLATFORM_LINUX
#endif

#if defined(__WIN32__)
#   define RNR_PLATFORM_WIN32
#endif

#if defined(__APPLE__)
#   define RNR_PLATFORM_APPLE
#endif

//restrict keyword
#if defined(__clang__)
#define RNR_RESTRICT __restrict__

#elif defined(__GNUC__) || defined(__GNUG__)
#define RNR_RESTRICT __restrict__

#elif defined(_MSC_VER)
#define RNR_RESTRICT __declspec(restrict)

#else
#define RNR_RESTRICT

#endif