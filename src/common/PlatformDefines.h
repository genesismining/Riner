#pragma once

#if defined (__unix__)
#   define MI_PLATFORM_UNIX
#endif

#if defined (__linux__)
#   define MI_PLATFORM_LINUX
#endif

#if defined(__WIN32__)
#   define MI_PLATFORM_WIN32
#endif

#if defined(__APPLE__)
#   define MI_PLATFORM_APPLE
#endif

//restrict keyword
#if defined(__clang__)
#define MI_RESTRICT __restrict__

#elif defined(__GNUC__) || defined(__GNUG__)
#define MI_RESTRICT __restrict__

#elif defined(_MSC_VER)
#define MI_RESTRICT __declspec(restrict)

#else
#define MI_RESTRICT

#endif