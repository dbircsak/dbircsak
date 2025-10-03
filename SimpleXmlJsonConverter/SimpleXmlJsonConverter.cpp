#include "SimpleXmlJsonConverter.hpp"

#include <pugixml.hpp>

// RapidJSON (headers only)
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace SimpleXmlJsonConverter {

// ---------------------- Utility ----------------------

static inline std::string read_file(const std::string &path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs)
    throw std::runtime_error("Failed to open file: " + path);
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return oss.str();
}

static inline void write_file(const std::string &path,
                              const std::string &data) {
  std::ofstream ofs(path, std::ios::binary);
  if (!ofs)
    throw std::runtime_error("Failed to write file: " + path);
  ofs << data;
}

static inline std::string trim(const std::string &s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b])))
    ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    --e;
  return s.substr(b, e - b);
}

static inline bool is_whitespace_only(const char *s) {
  if (!s)
    return true;
  while (*s) {
    if (!std::isspace(static_cast<unsigned char>(*s)))
      return false;
    ++s;
  }
  return true;
}

static inline std::string json_scalar_to_string(const rapidjson::Value &v) {
  if (v.IsString()) {
    return std::string(v.GetString(), v.GetStringLength());
  } else if (v.IsBool()) {
    return v.GetBool() ? "true" : "false";
  } else if (v.IsNull()) {
    // Represent null explicitly as "null"
    return "null";
  } else if (v.IsNumber()) {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> w(sb);
    v.Accept(w);
    return std::string(sb.GetString(), sb.GetSize());
  }
  return std::string();
}

// ---------------- XML -> JSON core -------------------

static rapidjson::Value
xml_node_to_json_value(const pugi::xml_node &node,
                       rapidjson::Document::AllocatorType &alloc);

// Returns true if node has at least one element child
static bool has_element_children(const pugi::xml_node &node) {
  for (pugi::xml_node ch = node.first_child(); ch; ch = ch.next_sibling()) {
    if (ch.type() == pugi::node_element)
      return true;
  }
  return false;
}

// Collect element children grouped by name
static std::unordered_map<std::string, std::vector<pugi::xml_node>>
group_children_by_name(const pugi::xml_node &node) {
  std::unordered_map<std::string, std::vector<pugi::xml_node>> groups;
  for (pugi::xml_node ch = node.first_child(); ch; ch = ch.next_sibling()) {
    if (ch.type() == pugi::node_element) {
      groups[ch.name()].push_back(ch);
    }
  }
  return groups;
}

// Return trimmed text content (concatenating all text nodes under 'node')
static std::string collect_text(const pugi::xml_node &node) {
  std::ostringstream oss;
  for (pugi::xml_node ch = node.first_child(); ch; ch = ch.next_sibling()) {
    if (ch.type() == pugi::node_pcdata) {
      oss << ch.value();
    } else if (ch.type() == pugi::node_cdata) {
      // Ignored per spec
    }
  }
  return trim(oss.str());
}

static rapidjson::Value
xml_node_to_json_value(const pugi::xml_node &node,
                       rapidjson::Document::AllocatorType &alloc) {

  bool has_attrs = node.first_attribute();
  bool has_elem_children = has_element_children(node);
  std::string text = collect_text(node);
  bool has_text = !text.empty();

  // If only text and no attributes and no element children → return string
  if (!has_attrs && !has_elem_children && has_text) {
    rapidjson::Value s;
    s.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()),
                alloc);
    return s;
  }

  // Otherwise, construct an object with attributes, children, and maybe "#text"
  rapidjson::Value obj(rapidjson::kObjectType);

  // Attributes → "@attr"
  for (pugi::xml_attribute a = node.first_attribute(); a;
       a = a.next_attribute()) {
    std::string key = std::string("@") + a.name();
    rapidjson::Value k;
    k.SetString(key.c_str(), static_cast<rapidjson::SizeType>(key.size()),
                alloc);
    const char *av = a.value();
    rapidjson::Value v;
    v.SetString(av, static_cast<rapidjson::SizeType>(std::strlen(av)), alloc);
    obj.AddMember(k, v, alloc);
  }

  // Children
  if (has_elem_children) {
    auto groups = group_children_by_name(node);
    for (auto &kv : groups) {
      const std::string &name = kv.first;
      const auto &nodes = kv.second;

      rapidjson::Value key;
      key.SetString(name.c_str(), static_cast<rapidjson::SizeType>(name.size()),
                    alloc);

      if (nodes.size() == 1u) {
        rapidjson::Value childVal = xml_node_to_json_value(nodes[0], alloc);
        obj.AddMember(key, childVal, alloc);
      } else {
        rapidjson::Value arr(rapidjson::kArrayType);
        arr.Reserve(static_cast<rapidjson::SizeType>(nodes.size()), alloc);
        for (const auto &n : nodes) {
          rapidjson::Value childVal = xml_node_to_json_value(n, alloc);
          arr.PushBack(childVal, alloc);
        }
        obj.AddMember(key, arr, alloc);
      }
    }
  }

  // If there is text along with attributes or element children → "#text"
  if (has_text) {
    rapidjson::Value k;
    const char *key_text = "#text";
    k.SetString(key_text,
                static_cast<rapidjson::SizeType>(std::strlen(key_text)), alloc);
    rapidjson::Value v;
    v.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()),
                alloc);
    obj.AddMember(k, v, alloc);
  }

  return obj;
}

static std::string xml_document_to_json_string(const pugi::xml_document &doc) {
  rapidjson::Document d;
  d.SetObject();

  // Find first element node as the root (ignore declarations, comments, etc.)
  pugi::xml_node root = doc.first_child();
  while (root && root.type() != pugi::node_element) {
    root = root.next_sibling();
  }
  if (!root) {
    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
    d.Accept(w);
    return std::string(sb.GetString(), sb.GetSize());
  }

  auto &alloc = d.GetAllocator();

  rapidjson::Value rootVal = xml_node_to_json_value(root, alloc);

  // Wrap root under its element name
  rapidjson::Value key;
  key.SetString(root.name(),
                static_cast<rapidjson::SizeType>(std::strlen(root.name())),
                alloc);
  d.AddMember(key, rootVal, alloc);

  rapidjson::StringBuffer sb;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
  d.Accept(w);
  return std::string(sb.GetString(), sb.GetSize());
}

// ---------------- JSON -> XML core -------------------

static void json_value_to_xml_under(const char *name, const rapidjson::Value &v,
                                    pugi::xml_node parent);

static void set_node_text(pugi::xml_node n, const std::string &s) {
  if (!s.empty())
    n.append_child(pugi::node_pcdata).set_value(s.c_str());
  else
    n.append_child(pugi::node_pcdata).set_value("");
}

static void object_to_xml(const char *name, const rapidjson::Value &obj,
                          pugi::xml_node parent) {
  pugi::xml_node cur = parent.append_child(name);

  if (obj.IsObject()) {
    auto o = obj.GetObject();
    // Attributes
    for (auto it = o.begin(); it != o.end(); ++it) {
      const auto k =
          std::string(it->name.GetString(), it->name.GetStringLength());
      if (!k.empty() && k[0] == '@' && it->value.IsString()) {
        const std::string attrName = k.substr(1);
        cur.append_attribute(attrName.c_str()).set_value(it->value.GetString());
      }
    }
    // #text
    auto jt = o.FindMember("#text");
    if (jt != o.end() && (jt->value.IsString() || jt->value.IsBool() ||
                          jt->value.IsNumber() || jt->value.IsNull())) {
      set_node_text(cur, json_scalar_to_string(jt->value));
    }
    // Children (non-attr, non-#text)
    for (auto it = o.begin(); it != o.end(); ++it) {
      const auto k =
          std::string(it->name.GetString(), it->name.GetStringLength());
      if (!k.empty() && k[0] == '@')
        continue;
      if (k == "#text")
        continue;

      if (it->value.IsArray()) {
        for (auto &elem : it->value.GetArray()) {
          json_value_to_xml_under(k.c_str(), elem, cur);
        }
      } else {
        json_value_to_xml_under(k.c_str(), it->value, cur);
      }
    }
  }
}

static void json_value_to_xml_under(const char *name, const rapidjson::Value &v,
                                    pugi::xml_node parent) {

  if (v.IsObject()) {
    object_to_xml(name, v, parent);
  } else if (v.IsArray()) {
    for (auto &elem : v.GetArray()) {
      json_value_to_xml_under(name, elem, parent);
    }
  } else {
    pugi::xml_node cur = parent.append_child(name);
    set_node_text(cur, json_scalar_to_string(v));
  }
}

static std::string json_document_to_xml_string(const rapidjson::Document &d) {
  pugi::xml_document doc;

  // Decide root element name
  std::string rootName = "root";
  if (d.IsObject()) {
    auto o = d.GetObject();
    if (o.MemberCount() == 1) {
      auto it = o.begin();
      rootName.assign(it->name.GetString(), it->name.GetStringLength());
      json_value_to_xml_under(rootName.c_str(), it->value, doc);
      std::ostringstream oss;
      doc.save(oss, "  ", pugi::format_default, pugi::encoding_utf8);
      return oss.str();
    }
  }

  // Otherwise wrap under <root>
  pugi::xml_node root = doc.append_child(rootName.c_str());
  if (d.IsObject()) {
    auto o = d.GetObject();
    for (auto it = o.begin(); it != o.end(); ++it) {
      const auto k =
          std::string(it->name.GetString(), it->name.GetStringLength());
      if (it->value.IsArray()) {
        for (auto &elem : it->value.GetArray()) {
          json_value_to_xml_under(k.c_str(), elem, root);
        }
      } else {
        json_value_to_xml_under(k.c_str(), it->value, root);
      }
    }
  } else if (d.IsArray()) {
    for (auto &elem : d.GetArray()) {
      json_value_to_xml_under("item", elem, root);
    }
  } else {
    set_node_text(root, json_scalar_to_string(d));
  }

  std::ostringstream oss;
  doc.save(oss, "  ", pugi::format_default, pugi::encoding_utf8);
  return oss.str();
}

// ---------------- Public API ------------------------

std::string xml_to_json(const std::string &xml) {
  pugi::xml_document doc;
  pugi::xml_parse_result ok = doc.load_string(
      xml.c_str(), pugi::parse_default | pugi::parse_declaration);
  if (!ok)
    throw std::runtime_error(std::string("XML parse error: ") +
                             ok.description());
  return xml_document_to_json_string(doc);
}

std::string json_to_xml(const std::string &json) {
  rapidjson::Document d;
  d.Parse(json.c_str());
  if (d.HasParseError()) {
    throw std::runtime_error("JSON parse error");
  }
  return json_document_to_xml_string(d);
}

void xml_file_to_json_file(const std::string &in_xml_path,
                           const std::string &out_json_path) {
  auto xml = read_file(in_xml_path);
  auto js = xml_to_json(xml);
  write_file(out_json_path, js);
}

void json_file_to_xml_file(const std::string &in_json_path,
                           const std::string &out_xml_path) {
  auto js = read_file(in_json_path);
  auto xml = json_to_xml(js);
  write_file(out_xml_path, xml);
}

} // namespace SimpleXmlJsonConverter
