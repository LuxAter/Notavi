#include "terminfo.hpp"

#ifdef NOTAVI_OS_POSIX
#include <unistd.h>
#endif

#include <string>

#include <notavi/enum.hpp>
#include <notavi/os.hpp>
#include <notavi/compiler.hpp>

notavi::terminfo::Os notavi::terminfo::get_os(){
#if defined(NOTAVI_OS_WINDOWS)
  return OS_WINDOWS;
#elif defined(NOTAVI_OS_APPLE)
  return OS_DARWIN;
#elif defined(NOTAVI_OS_UNIX)
  return OS_LINUX;
#elif defined(NOTAVI_OS_POSIX)
  return OS_POSIX;
#else
  return OS_UNKNOWN;
#endif
}
std::string notavi::terminfo::get_hostname(){
#if defined(NOTAVI_OS_WINDOWS)
  DWORD buf_char_count = 255;
  TCHAR info_buf[255];
  if(!GetComputerName(info_buf, &buf_char_count)) {
    return std::string();
  }
  return std::string(info_buf);
#elif defined(NOTAVI_OS_POSIX)
  char buf[255];
  gethostname(buf, 255);
  return std::string(buf);
#else
  return std::string();
#endif
}
std::string notavi::terminfo::get_username(){
#if defined(NOTAVI_OS_WINDOWS)
  DWORD buf_char_count = 255;
  TCHAR info_buf[255];
  if(!GetUserName(info_buf, &buf_char_count)) {
    return std::string();
  }
  return std::string(info_buf);
#elif defined(NOTAVI_OS_POSIX)
  char buf[255];
  getlogin_r(buf, 255);
  return std::string(buf);
#else
  return std::string();
#endif
}
