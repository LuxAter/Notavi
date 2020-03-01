#ifndef SERDE_HPP_T40XDSEI
#define SERDE_HPP_T40XDSEI

#include <nlohmann/json.hpp>
#include <cpptoml.h>

namespace notavi {
  using namespace cpptoml;
  using Json = nlohmann::json;
  typedef std::shared_ptr<cpptoml::table> Toml;
}  // namespace notavi

#endif /* end of include guard: SERDE_HPP_T40XDSEI */
