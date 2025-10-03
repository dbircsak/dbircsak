#pragma once
#include <string>

namespace SimpleXmlJsonConverter {
// Convert an XML string to JSON (minimal/deterministic mapping described in
// README)
std::string xml_to_json(const std::string &xml);

// Convert a JSON string to XML (inverse mapping; lossy but deterministic)
std::string json_to_xml(const std::string &json);

// File helpers
void xml_file_to_json_file(const std::string &in_xml_path,
                           const std::string &out_json_path);
void json_file_to_xml_file(const std::string &in_json_path,
                           const std::string &out_xml_path);
} // namespace SimpleXmlJsonConverter
