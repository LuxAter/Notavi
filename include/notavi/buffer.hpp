#ifndef BUFFER_HPP_MSOCEXYA
#define BUFFER_HPP_MSOCEXYA

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace notavi {

struct Buffer {
  struct Row {
    explicit Row(const std::string& line);
    std::string chars;
    std::vector<uint32_t> highlight;
  };

  explicit Buffer(const std::string& filename, std::istream& input);

  std::string filename;
  std::size_t changes;
  bool readonly;

  std::vector<Row> rows;
};

std::shared_ptr<Buffer> load_buffer(const std::string& filename);
bool save_buffer(const std::shared_ptr<Buffer>& buffer);

extern std::unordered_map<std::string, std::shared_ptr<Buffer>> open_buffers;

}  // namespace notavi

#endif /* end of include guard: BUFFER_HPP_MSOCEXYA */
