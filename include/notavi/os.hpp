#ifndef OS_HPP_P8XKRV4M
#define OS_HPP_P8XKRV4M

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  #define NOTAVI_OS_WINDOWS
  #ifdef _WIN64
    #define NOTAVI_OS_WINDOWS_64
  #else
    #define NOTAVI_OS_WINDOWS_32
  #endif
#elif __APPLE__
  #define NOTAVI_OS_APPLE
  #define NOTAVI_OS_UNIX
  #define NOTAVI_OS_POSIX
  #include <TargetConditionals.h>
  #if TARGET_IPHONE_SIMULATOR
    #define NOTAVI_OS_IPHONE_SIMULATOR
  #elif TARGET_OS_IPHONE
    #define NOTAVI_OS_IPHONE
  #elif TARGET_OS_MAC
    #define NOTAVI_OS_MAC
  #else
    #error "Unknown Apple Platform"
  #endif
#elif __linux__
  #define NOTAVI_OS_LINUX
  #define NOTAVI_OS_UNIX
  #define NOTAVI_OS_POSIX
#elif __unix__
  #define NOTAVI_OS_UNIX
  #define NOTAVI_OS_POSIX
#elif defined(_POSIX_VERSION)
  #define NOTAVI_OS_POSIX
#else
  #error "Unknown OS"
#endif

#endif /* end of include guard: OS_HPP_P8XKRV4M */
