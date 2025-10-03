# SimpleXmlJsonConverter

A self-contained C++17 project that converts between XML and JSON using pugixml 1.11.4 and RapidJSON 1.1.0.

---

## Features

- Mapping rules (only mode):
  - XML attributes → "@attr" keys  
  - Repeated sibling elements → JSON arrays  
  - Element text:  
    - If it’s the only content → string value  
    - Otherwise → "#text" key  
  - Ignore comments, CDATA, processing instructions, doctypes, and mixed content  
  - Conversion is bidirectional and deterministic (lossy by design)  
  - JSON numbers/bools/null are written as text when converting back to XML  

- Public API (see SimpleXmlJsonConverter.hpp):

  namespace SimpleXmlJsonConverter {
    std::string xml_to_json(const std::string& xml);
    std::string json_to_xml(const std::string& json);
    void xml_file_to_json_file(const std::string& in_xml_path, const std::string& out_json_path);
    void json_file_to_xml_file(const std::string& in_json_path, const std::string& out_xml_path);
  }

- CLI (main.cpp):

  SimpleXmlJsonConverter <input.(xml|json)> [output]

  - Detects type by extension or first non-space char ('<' → XML, '{'/'[' → JSON).  
  - If [output] is omitted → auto-selects <input> with the opposite extension.  

---

## Requirements

- pugixml 1.11.4: place pugixml.hpp and pugixml.cpp in the same directory.  
- RapidJSON 1.1.0: header-only, discovered via find_path in CMake.  

---

## Build

./build.sh

This script:  
- Configures a build/ directory  
- Builds in Release mode  
- Installs SimpleXmlJsonConverter binary into the source directory  

---

## Usage

### Convert XML → JSON

./SimpleXmlJsonConverter sample.xml  
# writes sample.json  

### Convert JSON → XML

./SimpleXmlJsonConverter sample.json  
# writes sample.xml  

---

## Notes

- Minimal mapping is lossy by design:  
  - XML declarations, comments, processing instructions, doctypes, and mixed content are ignored.  
- Pretty-printed JSON is produced for readability.  
- Implementation avoids:  
  - rapidjson::Value::Swap  
  - Direct calls to Document::MemberBegin() (uses GetObject().begin() style instead).  
