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
