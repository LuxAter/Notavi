#ifndef TERMINFO_HPP_GRVA8SVB
#define TERMINFO_HPP_GRVA8SVB

#include <string>

#include <notavi/enum.hpp>
#include <notavi/os.hpp>
#include <notavi/compiler.hpp>

namespace notavi {
  namespace terminfo {
    enum Os {
      OS_DARWIN,
      OS_LINUX,
      OS_WINDOWS,
      OS_POSIX,
      OS_UNKNOWN
    };

    Os get_os();
    std::string get_hostname();
    std::string get_username();
  } // namespace terminfo
} // namespace notavi

#endif /* end of include guard: TERMINFO_HPP_GRVA8SVB */
