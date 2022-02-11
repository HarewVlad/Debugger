struct Source {
  std::unordered_map<std::string, std::vector<Line>> filename_to_lines;
  std::unordered_map<DWORD64, std::string> line_address_to_filename;
  std::map<DWORD64, Line> address_to_line; // Need to be ordered
};