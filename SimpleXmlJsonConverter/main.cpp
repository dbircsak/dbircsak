#include "SimpleXmlJsonConverter.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

static std::string read_all(const fs::path &p) {
  std::ifstream ifs(p, std::ios::binary);
  if (!ifs)
    throw std::runtime_error("Failed to open: " + p.string());
  std::string data((std::istreambuf_iterator<char>(ifs)),
                   std::istreambuf_iterator<char>());
  return data;
}

static std::string ltrim(const std::string &s) {
  size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
    ++i;
  return s.substr(i);
}

static bool looks_like_xml(const std::string &content) {
  std::string t = ltrim(content);
  return !t.empty() && t[0] == '<';
}

static bool looks_like_json(const std::string &content) {
  std::string t = ltrim(content);
  return !t.empty() && (t[0] == '{' || t[0] == '[');
}

static std::string replace_extension(const fs::path &in, const char *new_ext) {
  fs::path p = in;
  p.replace_extension(new_ext);
  return p.string();
}

int main(int argc, char **argv) {
  try {
    if (argc < 2 || argc > 3) {
      std::cerr
          << "Usage: SimpleXmlJsonConverter <input.(xml|json)> [output]\n";
      return 1;
    }

    fs::path inPath(argv[1]);
    fs::path outPath;
    const bool hasOut = (argc == 3);
    if (hasOut)
      outPath = fs::path(argv[2]);

    // Determine input type by extension or sniff
    std::string ext = inPath.has_extension() ? inPath.extension().string() : "";
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    bool isXml = false;
    bool isJson = false;

    if (ext == ".xml")
      isXml = true;
    else if (ext == ".json")
      isJson = true;
    else {
      // Sniff first non-space char
      std::string content = read_all(inPath);
      if (looks_like_xml(content))
        isXml = true;
      else if (looks_like_json(content))
        isJson = true;
      else {
        std::cerr
            << "Unable to detect input type (not XML/JSON by sniffing).\n";
        return 2;
      }
    }

    if (!hasOut) {
      if (isXml)
        outPath = replace_extension(inPath, ".json");
      else
        outPath = replace_extension(inPath, ".xml");
    }

    if (isXml) {
      SimpleXmlJsonConverter::xml_file_to_json_file(inPath.string(),
                                                    outPath.string());
    } else {
      SimpleXmlJsonConverter::json_file_to_xml_file(inPath.string(),
                                                    outPath.string());
    }

    std::cout << "Wrote: " << outPath.string() << "\n";
    return 0;

  } catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 3;
  }
}
