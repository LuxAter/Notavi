#ifndef EDITOR_HPP_2GWGEISO
#define EDITOR_HPP_2GWGEISO

#include <memory>

#include "buffer.hpp"
#include "util.hpp"

namespace notavi {
struct Editor {
  point<std::size_t> cursor, offset;
  std::shared_ptr<Buffer> buffer;
};
}  // namespace notavi

#endif /* end of include guard: EDITOR_HPP_2GWGEISO */
