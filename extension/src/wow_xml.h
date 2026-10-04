#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace godot {

// One element of an interface XML file; text holds the element's own character data.
struct WowXmlNode {
	std::string tag;
	std::vector<std::pair<std::string, std::string>> attributes;
	std::vector<std::unique_ptr<WowXmlNode>> children;
	std::string text;
	int line = 0;

	const std::string *attribute(const char *name) const;
	std::string get(const char *name, const std::string &fallback = std::string()) const;
	bool flag(const char *name, bool fallback = false) const;
	float number(const char *name, float fallback = 0.0f) const;
	const WowXmlNode *child(const char *name) const;
};

// The interface's XML: elements, attributes, comments and CDATA, forgiving of mismatched close tags.
struct WowXmlDocument {
	std::unique_ptr<WowXmlNode> root;
	std::string path;
	std::string error;

	bool parse(const std::string &source);
};

} // namespace godot
