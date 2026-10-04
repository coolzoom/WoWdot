#include "wow_xml.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace godot {

const std::string *WowXmlNode::attribute(const char *name) const {
	for (const auto &[key, value] : attributes) {
		if (key == name) {
			return &value;
		}
	}
	return nullptr;
}

std::string WowXmlNode::get(const char *name, const std::string &fallback) const {
	const std::string *value = attribute(name);
	return value ? *value : fallback;
}

bool WowXmlNode::flag(const char *name, bool fallback) const {
	const std::string *value = attribute(name);
	if (!value) {
		return fallback;
	}
	return *value == "true" || *value == "1";
}

float WowXmlNode::number(const char *name, float fallback) const {
	const std::string *value = attribute(name);
	return value ? std::strtof(value->c_str(), nullptr) : fallback;
}

const WowXmlNode *WowXmlNode::child(const char *name) const {
	for (const auto &node : children) {
		if (node->tag == name) {
			return node.get();
		}
	}
	return nullptr;
}

namespace {

void append_utf8(std::string &out, unsigned long code) {
	if (code < 0x80) {
		out += static_cast<char>(code);
	} else if (code < 0x800) {
		out += static_cast<char>(0xC0 | (code >> 6));
		out += static_cast<char>(0x80 | (code & 0x3F));
	} else if (code < 0x10000) {
		out += static_cast<char>(0xE0 | (code >> 12));
		out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (code & 0x3F));
	} else {
		out += static_cast<char>(0xF0 | (code >> 18));
		out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (code & 0x3F));
	}
}

std::string decode_entities(const char *begin, const char *end) {
	std::string out;
	out.reserve(end - begin);
	for (const char *p = begin; p < end; ++p) {
		if (*p != '&') {
			out += *p;
			continue;
		}
		const char *semi = static_cast<const char *>(std::memchr(p, ';', end - p));
		if (!semi || semi - p > 10) {
			out += *p;
			continue;
		}
		std::string name(p + 1, semi);
		if (name == "lt") {
			out += '<';
		} else if (name == "gt") {
			out += '>';
		} else if (name == "amp") {
			out += '&';
		} else if (name == "quot") {
			out += '"';
		} else if (name == "apos") {
			out += '\'';
		} else if (!name.empty() && name[0] == '#') {
			bool hex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
			append_utf8(out, std::strtoul(name.c_str() + (hex ? 2 : 1), nullptr, hex ? 16 : 10));
		} else {
			out += *p;
			continue;
		}
		p = semi;
	}
	return out;
}

bool is_name_char(char c) {
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ':' || c == '-' || c == '.';
}

} // namespace

bool WowXmlDocument::parse(const std::string &source) {
	const char *p = source.data();
	const char *end = p + source.size();
	if (end - p >= 3 && static_cast<unsigned char>(p[0]) == 0xEF && static_cast<unsigned char>(p[1]) == 0xBB && static_cast<unsigned char>(p[2]) == 0xBF) {
		p += 3;
	}
	root = std::make_unique<WowXmlNode>();
	root->tag = "#document";
	std::vector<WowXmlNode *> stack = { root.get() };
	int line = 1;
	auto count_lines = [&line](const char *from, const char *to) {
		for (const char *c = from; c < to; ++c) {
			line += *c == '\n';
		}
	};
	while (p < end) {
		const char *lt = static_cast<const char *>(std::memchr(p, '<', end - p));
		const char *text_end = lt ? lt : end;
		if (text_end > p) {
			stack.back()->text += decode_entities(p, text_end);
			count_lines(p, text_end);
		}
		if (!lt) {
			break;
		}
		p = lt;
		if (std::strncmp(p, "<!--", 4) == 0) {
			const char *close = std::strstr(p + 4, "-->");
			const char *stop = close ? close + 3 : end;
			count_lines(p, stop);
			p = stop;
			continue;
		}
		if (std::strncmp(p, "<![CDATA[", 9) == 0) {
			const char *close = std::strstr(p + 9, "]]>");
			const char *stop = close ? close : end;
			stack.back()->text.append(p + 9, stop);
			count_lines(p, stop);
			p = close ? close + 3 : end;
			continue;
		}
		if (p + 1 < end && (p[1] == '?' || p[1] == '!')) {
			const char *close = static_cast<const char *>(std::memchr(p, '>', end - p));
			const char *stop = close ? close + 1 : end;
			count_lines(p, stop);
			p = stop;
			continue;
		}
		if (p + 1 < end && p[1] == '/') {
			const char *name = p + 2;
			const char *name_end = name;
			while (name_end < end && is_name_char(*name_end)) {
				++name_end;
			}
			std::string tag(name, name_end);
			const char *close = static_cast<const char *>(std::memchr(p, '>', end - p));
			p = close ? close + 1 : end;
			// A stray close tag pops back to its element, or is ignored when nothing open matches.
			for (size_t i = stack.size(); i-- > 1;) {
				if (stack[i]->tag == tag) {
					stack.resize(i);
					break;
				}
			}
			continue;
		}
		auto node = std::make_unique<WowXmlNode>();
		node->line = line;
		++p;
		const char *name_end = p;
		while (name_end < end && is_name_char(*name_end)) {
			++name_end;
		}
		node->tag.assign(p, name_end);
		p = name_end;
		bool self_closing = false;
		while (p < end) {
			while (p < end && std::isspace(static_cast<unsigned char>(*p))) {
				line += *p == '\n';
				++p;
			}
			if (p >= end) {
				break;
			}
			if (*p == '/') {
				self_closing = true;
				++p;
				continue;
			}
			if (*p == '>') {
				++p;
				break;
			}
			const char *key = p;
			while (p < end && is_name_char(*p)) {
				++p;
			}
			if (p == key) {
				++p;
				continue;
			}
			std::string attribute(key, p);
			while (p < end && std::isspace(static_cast<unsigned char>(*p))) {
				++p;
			}
			if (p < end && *p == '=') {
				++p;
				while (p < end && std::isspace(static_cast<unsigned char>(*p))) {
					++p;
				}
				if (p < end && (*p == '"' || *p == '\'')) {
					char quote = *p++;
					const char *close = static_cast<const char *>(std::memchr(p, quote, end - p));
					const char *stop = close ? close : end;
					node->attributes.emplace_back(attribute, decode_entities(p, stop));
					count_lines(p, stop);
					p = close ? close + 1 : end;
				}
			}
		}
		WowXmlNode *raw = node.get();
		stack.back()->children.push_back(std::move(node));
		if (!self_closing) {
			stack.push_back(raw);
		}
	}
	if (root->children.empty()) {
		error = "no elements";
		return false;
	}
	return true;
}

} // namespace godot
