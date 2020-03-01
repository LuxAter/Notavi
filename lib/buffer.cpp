#include "buffer.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace notavi {
std::unordered_map<std::string, std::shared_ptr<Buffer>> open_buffers;
}  // namespace notavi

notavi::Buffer::Row::Row(const std::string &line)
    : chars(line), highlight(line.size(), 0) {}
notavi::Buffer::Buffer(const std::string &filename, std::istream &input)
    : filename(filename), changes(0), readonly(false), rows() {
  for (std::string line; std::getline(input, line);) {
    rows.emplace_back(line);
  }
}

std::shared_ptr<notavi::Buffer> notavi::load_buffer(
    const std::string &filename) {
  std::unordered_map<std::string, std::shared_ptr<Buffer>>::iterator it;
  if ((it = open_buffers.find(filename)) != open_buffers.end())
    return it->second;
  std::ifstream file_stream(filename);
  if (file_stream.is_open()) {
    open_buffers[filename] = std::make_shared<Buffer>(filename, file_stream);
    file_stream.close();
    return open_buffers[filename];
  }
  return nullptr;
}
bool notavi::save_buffer(const std::shared_ptr<Buffer> &buffer) {
  if (buffer->readonly) return false;
  std::ofstream file_stream(buffer->filename);
  if (file_stream.is_open()) {
    for (typename std::vector<Buffer::Row>::const_iterator row =
             buffer->rows.begin();
         row != buffer->rows.end(); ++row) {
      file_stream.write(row->chars.c_str(), row->chars.size());
    }
    file_stream.close();
    return true;
  }
  return false;
}
