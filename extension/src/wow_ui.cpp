#include "wow_ui.h"
#include "wow_ui_lua.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cstring>

namespace godot {

namespace {

struct KindInfo {
	const char *tag;
	WowUI::Kind kind;
};

const KindInfo KINDS[] = {
	{ "Frame", WowUI::KIND_FRAME },
	{ "Button", WowUI::KIND_BUTTON },
	{ "LootButton", WowUI::KIND_BUTTON },
	{ "CheckButton", WowUI::KIND_CHECK_BUTTON },
	{ "EditBox", WowUI::KIND_EDIT_BOX },
	{ "ScrollFrame", WowUI::KIND_SCROLL_FRAME },
	{ "ScrollingMessageFrame", WowUI::KIND_SCROLLING_MESSAGE_FRAME },
	{ "MessageFrame", WowUI::KIND_MESSAGE_FRAME },
	{ "Slider", WowUI::KIND_SLIDER },
	{ "StatusBar", WowUI::KIND_STATUS_BAR },
	{ "ColorSelect", WowUI::KIND_COLOR_SELECT },
	{ "Model", WowUI::KIND_MODEL },
	{ "PlayerModel", WowUI::KIND_MODEL },
	{ "DressUpModel", WowUI::KIND_MODEL },
	{ "TabardModel", WowUI::KIND_MODEL },
	{ "ModelFFX", WowUI::KIND_MODEL },
	{ "Minimap", WowUI::KIND_MINIMAP },
	{ "GameTooltip", WowUI::KIND_GAME_TOOLTIP },
	{ "SimpleHTML", WowUI::KIND_SIMPLE_HTML },
	{ "Cooldown", WowUI::KIND_COOLDOWN },
	{ "MovieFrame", WowUI::KIND_MOVIE_FRAME },
	{ "WorldFrame", WowUI::KIND_WORLD_FRAME },
	{ "TaxiRouteFrame", WowUI::KIND_FRAME },
	{ "Texture", WowUI::KIND_TEXTURE },
	{ "FontString", WowUI::KIND_FONT_STRING },
	{ "Font", WowUI::KIND_FONT },
};

const KindInfo *kind_for(const std::string &tag) {
	for (const KindInfo &info : KINDS) {
		if (tag == info.tag) {
			return &info;
		}
	}
	return nullptr;
}

std::string dir_of(const std::string &path) {
	size_t slash = path.find_last_of("\\/");
	return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

std::string trim(const std::string &text) {
	size_t begin = text.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos) {
		return std::string();
	}
	size_t end = text.find_last_not_of(" \t\r\n");
	return text.substr(begin, end - begin + 1);
}

std::vector<std::string> split_names(const std::string &list) {
	std::vector<std::string> out;
	size_t from = 0;
	while (from <= list.size()) {
		size_t comma = list.find(',', from);
		std::string part = trim(list.substr(from, comma == std::string::npos ? std::string::npos : comma - from));
		if (!part.empty()) {
			out.push_back(part);
		}
		if (comma == std::string::npos) {
			break;
		}
		from = comma + 1;
	}
	return out;
}

std::string std_string(const String &text) {
	CharString utf8 = text.utf8();
	return std::string(utf8.get_data(), utf8.length());
}

String godot_string(const std::string &text) {
	return String::utf8(text.c_str(), static_cast<int64_t>(text.size()));
}

} // namespace

bool WowUI::kind_for_type(const std::string &type, Kind &r_kind, String &r_type) {
	for (const KindInfo &info : KINDS) {
		if (String(info.tag).nocasecmp_to(godot_string(type)) == 0) {
			r_kind = info.kind;
			r_type = info.tag;
			return true;
		}
	}
	return false;
}

WowUI::WowUI() {
	set_mouse_filter(MOUSE_FILTER_STOP);
	set_focus_mode(FOCUS_NONE);
	// GetTime counts from engine start, the clock the host's cooldowns and casts are kept in.
	start_usec = 0.0;
	open_lua();
}

WowUI::~WowUI() {
	close_lua();
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs) {
		for (const RID &rid : items) {
			rs->free_rid(rid);
		}
	}
}

void WowUI::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			set_process(true);
			break;
		case NOTIFICATION_PROCESS:
			update_frame(get_process_delta_time());
			begin_draw();
			if (is_visible_in_tree()) {
				std::vector<int> order;
				collect_draw(order);
				DrawState base;
				for (int id : order) {
					draw_frame(*widgets[id], base);
				}
			}
			end_draw();
			break;
		case NOTIFICATION_RESIZED:
			invalidate();
			break;
		default:
			break;
	}
}

void WowUI::report(const String &message) {
	if (errors.size() < 2000) {
		errors.push_back(message);
	}
	static constexpr int PRINT_LIMIT = 400;
	if (errors.size() <= PRINT_LIMIT) {
		UtilityFunctions::printerr("WowUI: ", message);
	}
}

void WowUI::note_missing(const std::string &name) {
	if (missing.insert(name).second) {
		UtilityFunctions::print_verbose("WowUI: missing ", godot_string(name));
	}
}

PackedStringArray WowUI::get_missing() const {
	PackedStringArray out;
	std::vector<std::string> sorted(missing.begin(), missing.end());
	std::sort(sorted.begin(), sorted.end());
	for (const std::string &name : sorted) {
		out.push_back(godot_string(name));
	}
	return out;
}

// Files.

bool WowUI::has_file(const String &path) const {
	if (!disk_root.is_empty() && FileAccess::file_exists(disk_root.path_join(path.replace("\\", "/")))) {
		return true;
	}
	return archive.is_valid() && archive->has(path);
}

namespace {

bool read_bytes(const Ref<WowArchive> &archive, const String &disk_root, const String &path, std::string &r_out) {
	if (!disk_root.is_empty()) {
		String disk = disk_root.path_join(path.replace("\\", "/"));
		if (FileAccess::file_exists(disk)) {
			PackedByteArray bytes = FileAccess::get_file_as_bytes(disk);
			r_out.assign(reinterpret_cast<const char *>(bytes.ptr()), bytes.size());
			return true;
		}
	}
	if (archive.is_null()) {
		return false;
	}
	std::vector<uint8_t> data;
	if (!archive->read_bytes(WowArchive::normalize(path), data)) {
		return false;
	}
	r_out.assign(reinterpret_cast<const char *>(data.data()), data.size());
	return true;
}

} // namespace

bool WowUI::read_file(const String &path, std::string &r_out) const {
	return read_bytes(archive, disk_root, path, r_out);
}

String WowUI::read_text(const String &path) const {
	std::string bytes;
	if (!read_bytes(archive, disk_root, path, bytes)) {
		return String();
	}
	return godot_string(bytes);
}

bool WowUI::load_toc(const String &path) {
	std::string bytes;
	if (!read_bytes(archive, disk_root, path, bytes)) {
		report("cannot read " + path);
		return false;
	}
	std::string dir = dir_of(std_string(path));
	size_t from = 0;
	while (from < bytes.size()) {
		size_t newline = bytes.find('\n', from);
		std::string line = trim(bytes.substr(from, newline == std::string::npos ? std::string::npos : newline - from));
		from = newline == std::string::npos ? bytes.size() : newline + 1;
		if (line.empty() || line[0] == '#') {
			continue;
		}
		String file = godot_string(dir + line);
		if (file.to_lower().ends_with(".lua")) {
			load_lua_file(file);
		} else if (file.to_lower().ends_with(".xml")) {
			load_xml_file(file);
		}
	}
	return true;
}

bool WowUI::load_lua_file(const String &path) {
	std::string bytes;
	if (!read_bytes(archive, disk_root, path, bytes)) {
		report("cannot read " + path);
		return false;
	}
	return run_chunk(bytes, "@" + std_string(path));
}

bool WowUI::load_xml_file(const String &path) {
	std::string bytes;
	if (!read_bytes(archive, disk_root, path, bytes)) {
		report("cannot read " + path);
		return false;
	}
	auto document = std::make_unique<WowXmlDocument>();
	document->path = std_string(path);
	if (!document->parse(bytes)) {
		report(path + String(": ") + godot_string(document->error));
		return false;
	}
	const WowXmlDocument *doc = document.get();
	documents.push_back(std::move(document));
	for (const auto &node : doc->root->children) {
		if (node->tag == "Ui") {
			process_ui(*node, dir_of(doc->path));
		} else if (node->tag == "Bindings") {
			for (const auto &binding : node->children) {
				if (binding->tag != "Binding") {
					continue;
				}
				std::string name = binding->get("name");
				if (!bindings.count(name)) {
					binding_order.push_back(name);
				}
				bindings[name] = binding->text;
				if (binding->flag("runOnUp")) {
					bindings_on_up.insert(name);
				}
			}
		}
	}
	return true;
}

void WowUI::process_ui(const WowXmlNode &root, const std::string &dir) {
	for (const auto &child : root.children) {
		const std::string &tag = child->tag;
		if (tag == "Script") {
			std::string file = child->get("file");
			if (!file.empty()) {
				load_lua_file(godot_string(dir + file));
			}
			if (!trim(child->text).empty()) {
				run_chunk(child->text, "=" + dir + " Script " + std::to_string(child->line));
			}
		} else if (tag == "Include") {
			load_xml_file(godot_string(dir + child->get("file")));
		} else if (kind_for(tag)) {
			int parent = -1;
			std::string parent_name = child->get("parent");
			if (!parent_name.empty()) {
				parent = find(godot_string(parent_name));
			}
			build(*child, parent, false, true);
		}
	}
}

void WowUI::load_bindings(const String &path) {
	load_xml_file(path);
}

PackedStringArray WowUI::get_binding_names() const {
	PackedStringArray out;
	for (const std::string &name : binding_order) {
		out.push_back(godot_string(name));
	}
	return out;
}

bool WowUI::run_binding(const String &name, bool down) {
	auto found = bindings.find(std_string(name));
	if (found == bindings.end()) {
		return false;
	}
	if (!down && !bindings_on_up.count(found->first)) {
		return true;
	}
	lua_pushstring(L, down ? "down" : "up");
	lua_setglobal(L, "keystate");
	run_chunk(found->second, "=binding " + found->first);
	return true;
}

// Widgets.

int WowUI::find(const String &name) const {
	if (name.is_empty() || !L) {
		return -1;
	}
	CharString utf8 = name.utf8();
	lua_getglobal(L, utf8.get_data());
	int id = widget_from(-1);
	lua_pop(L, 1);
	return id;
}

int WowUI::widget_from(int index) const {
	if (!lua_istable(L, index)) {
		return -1;
	}
	lua_rawgeti(L, index, 0);
	int id = -1;
	if (lua_islightuserdata(L, -1)) {
		id = static_cast<int>(reinterpret_cast<intptr_t>(lua_touserdata(L, -1))) - 1;
	}
	lua_pop(L, 1);
	return widget(id) ? id : -1;
}

int WowUI::check_widget(lua_State *state, int index) const {
	int id = widget_from(index);
	if (id < 0) {
		luaL_error(state, "bad self: an interface object is needed");
	}
	return id;
}

String WowUI::resolve_name(const String &name, int parent) const {
	if (!name.contains("$parent")) {
		return name;
	}
	String parent_name;
	for (int p = parent; p >= 0; p = widgets[p]->parent) {
		if (!widgets[p]->name.is_empty()) {
			parent_name = widgets[p]->name;
			break;
		}
	}
	return name.replace("$parent", parent_name);
}

int WowUI::create_widget(Kind kind, const String &type, const String &name, int parent, bool region) {
	auto w = std::make_unique<Widget>();
	int id = static_cast<int>(widgets.size());
	w->id = id;
	w->kind = kind;
	w->type = type;
	w->name = name;
	w->parent = kind == KIND_FONT ? -1 : parent;
	w->mouse = kind == KIND_BUTTON || kind == KIND_CHECK_BUTTON || kind == KIND_EDIT_BOX || kind == KIND_SLIDER;
	w->auto_focus = kind == KIND_EDIT_BOX;
	if (kind == KIND_EDIT_BOX || kind == KIND_SCROLLING_MESSAGE_FRAME || kind == KIND_MESSAGE_FRAME) {
		w->font.justify_h = JUSTIFY_START;
	}
	if (kind == KIND_STATUS_BAR || kind == KIND_SLIDER) {
		w->max_value = 1.0f;
	}
	if (Widget *p = widget(w->parent)) {
		w->strata = p->strata;
		w->level = p->level + (region ? 0 : 1);
		w->scale = 1.0f;
		(region ? p->regions : p->children).push_back(id);
		w->visible = p->visible;
	} else {
		w->visible = kind != KIND_FONT;
	}
	if (region) {
		w->layer = LAYER_ARTWORK;
	}
	widgets.push_back(std::move(w));
	push_widget(id);
	if (!name.is_empty()) {
		CharString utf8 = name.utf8();
		lua_pushvalue(L, -1);
		lua_setglobal(L, utf8.get_data());
	}
	lua_pop(L, 1);
	invalidate();
	return id;
}

int WowUI::build(const WowXmlNode &node, int parent, bool is_region_hint, bool run_load) {
	const KindInfo *info = kind_for(node.tag);
	if (!info) {
		return -1;
	}
	std::string raw_name = node.get("name");
	if (node.flag("virtual")) {
		if (!raw_name.empty()) {
			templates[raw_name] = &node;
		}
		if (info->kind != KIND_FONT) {
			return -1;
		}
	}
	bool region = info->kind == KIND_TEXTURE || info->kind == KIND_FONT_STRING || is_region_hint;
	String name = resolve_name(godot_string(raw_name), parent);
	if (info->kind == KIND_FONT) {
		int id = find(name);
		if (id < 0 || widgets[id]->kind != KIND_FONT) {
			id = create_widget(KIND_FONT, "Font", name, -1, false);
		}
		Widget &font = *widgets[id];
		inherit_font(font.font, node.get("inherits"));
		apply_font(font.font, node);
		return id;
	}
	int id = create_widget(info->kind, info->tag, name, parent, region);
	// Template children run OnLoad as they are made, and some read their parent's id.
	if (node.attribute("id")) {
		widgets[id]->frame_id = static_cast<int>(node.number("id"));
	}
	apply_inherits(id, node.get("inherits"));
	apply(id, node);
	if (!region && run_load) {
		load_on(id);
	}
	return id;
}

void WowUI::load_on(int id) {
	call_script(id, "OnLoad");
}

void WowUI::apply_inherits(int id, const std::string &inherits) {
	for (const std::string &name : split_names(inherits)) {
		auto found = templates.find(name);
		if (found != templates.end() && found->second->tag == "Font") {
			inherit_font(widgets[id]->font, name);
			reset_layout(*widgets[id]);
			continue;
		}
		if (found == templates.end()) {
			// A FontString may inherit a Font object made outside the templates.
			Widget &w = *widgets[id];
			if (w.kind == KIND_FONT_STRING || w.kind == KIND_EDIT_BOX || w.kind == KIND_SCROLLING_MESSAGE_FRAME || w.kind == KIND_MESSAGE_FRAME) {
				inherit_font(w.font, name);
			} else {
				report("no template " + godot_string(name) + " for " + w.name);
			}
			continue;
		}
		apply_inherits(id, found->second->get("inherits"));
		apply(id, *found->second);
	}
}

void WowUI::inherit_font(FontInfo &font, const std::string &names) {
	for (const std::string &name : split_names(names)) {
		int id = find(godot_string(name));
		if (id >= 0 && widgets[id]->kind == KIND_FONT) {
			font = widgets[id]->font;
			continue;
		}
		auto found = templates.find(name);
		if (found != templates.end()) {
			inherit_font(font, found->second->get("inherits"));
			apply_font(font, *found->second);
		}
	}
}

namespace {

float abs_value(const WowXmlNode &node) {
	if (const WowXmlNode *abs = node.child("AbsValue")) {
		return abs->number("val");
	}
	return node.number("val");
}

bool dimension(const WowXmlNode &node, float &r_x, float &r_y, bool &r_has_x, bool &r_has_y) {
	const WowXmlNode *source = node.child("AbsDimension");
	if (!source) {
		source = &node;
	}
	r_has_x = source->attribute("x") != nullptr;
	r_has_y = source->attribute("y") != nullptr;
	r_x = source->number("x");
	r_y = source->number("y");
	return r_has_x || r_has_y;
}

void inset(const WowXmlNode &node, float &r_left, float &r_right, float &r_top, float &r_bottom) {
	const WowXmlNode *source = node.child("AbsInset");
	if (!source) {
		source = &node;
	}
	r_left = source->number("left");
	r_right = source->number("right");
	r_top = source->number("top");
	r_bottom = source->number("bottom");
}

Color xml_color(const WowXmlNode &node, const Color &fallback = Color(1, 1, 1, 1)) {
	return Color(node.number("r", fallback.r), node.number("g", fallback.g), node.number("b", fallback.b), node.number("a", fallback.a));
}

int justify_from(const std::string &value, int fallback) {
	if (value == "LEFT" || value == "TOP") {
		return WowUI::JUSTIFY_START;
	}
	if (value == "CENTER" || value == "MIDDLE") {
		return WowUI::JUSTIFY_MIDDLE;
	}
	if (value == "RIGHT" || value == "BOTTOM") {
		return WowUI::JUSTIFY_END;
	}
	return fallback;
}

} // namespace

void WowUI::apply_font(FontInfo &font, const WowXmlNode &node) {
	if (const std::string *file = node.attribute("font")) {
		font.file = godot_string(*file);
	}
	if (const std::string *outline = node.attribute("outline")) {
		font.outline = *outline == "NORMAL" || *outline == "THICK";
		font.thick = *outline == "THICK";
	}
	font.justify_h = justify_from(node.get("justifyH"), font.justify_h);
	font.justify_v = justify_from(node.get("justifyV"), font.justify_v);
	if (node.attribute("spacing")) {
		font.spacing = node.number("spacing");
	}
	for (const auto &child : node.children) {
		if (child->tag == "FontHeight") {
			font.height = abs_value(*child);
		} else if (child->tag == "Color") {
			font.color = xml_color(*child);
		} else if (child->tag == "Shadow") {
			font.shadow = Color(0, 0, 0, 1);
			for (const auto &part : child->children) {
				if (part->tag == "Color") {
					font.shadow = xml_color(*part, Color(0, 0, 0, 1));
				} else if (part->tag == "Offset") {
					float x, y;
					bool has_x, has_y;
					dimension(*part, x, y, has_x, has_y);
					font.shadow_offset = Vector2(x, y);
				}
			}
		}
	}
}

void WowUI::apply_size(int id, const WowXmlNode &node) {
	Widget &w = *widgets[id];
	float x, y;
	bool has_x, has_y;
	dimension(node, x, y, has_x, has_y);
	if (has_x) {
		w.width = x;
		w.has_width = x != 0.0f;
	}
	if (has_y) {
		w.height = y;
		w.has_height = y != 0.0f;
	}
	invalidate();
}

void WowUI::apply_anchors(int id, const WowXmlNode &node) {
	Widget &w = *widgets[id];
	for (const auto &child : node.children) {
		if (child->tag != "Anchor") {
			continue;
		}
		Anchor anchor;
		anchor.point = wowui::point_from(godot_string(child->get("point", "TOPLEFT")));
		anchor.relative_point = wowui::point_from(godot_string(child->get("relativePoint", child->get("point", "TOPLEFT"))));
		std::string relative = child->get("relativeTo");
		if (!relative.empty()) {
			anchor.relative_name = resolve_name(godot_string(relative), w.parent);
			anchor.relative = find(anchor.relative_name);
			if (anchor.relative < 0) {
				anchor.relative = -3;
			}
		}
		if (const WowXmlNode *offset = child->child("Offset")) {
			bool has_x, has_y;
			dimension(*offset, anchor.x, anchor.y, has_x, has_y);
		}
		// A second anchor on the same point replaces the first.
		bool replaced = false;
		for (Anchor &existing : w.anchors) {
			if (existing.point == anchor.point) {
				existing = anchor;
				replaced = true;
			}
		}
		if (!replaced) {
			w.anchors.push_back(anchor);
		}
	}
	invalidate();
}

void WowUI::apply(int id, const WowXmlNode &node) {
	Widget *w = widgets[id].get();
	bool region = w->kind == KIND_TEXTURE || w->kind == KIND_FONT_STRING;
	if (node.attribute("hidden")) {
		w->shown = !node.flag("hidden");
		update_visibility(id, false);
	}
	if (node.flag("setAllPoints")) {
		w->anchors.clear();
		Anchor a;
		a.point = a.relative_point = TOPLEFT;
		w->anchors.push_back(a);
		a.point = a.relative_point = BOTTOMRIGHT;
		w->anchors.push_back(a);
	}
	if (node.attribute("alpha")) {
		w->alpha = node.number("alpha", 1.0f);
	}
	if (node.attribute("scale") && w->kind != KIND_MODEL) {
		w->scale = node.number("scale", 1.0f);
	}
	if (node.attribute("id")) {
		w->frame_id = static_cast<int>(node.number("id"));
	}
	if (!region) {
		if (const std::string *strata = node.attribute("frameStrata")) {
			int value = wowui::strata_from(godot_string(*strata));
			std::vector<int> stack = { id };
			while (!stack.empty()) {
				Widget &each = *widgets[stack.back()];
				stack.pop_back();
				each.strata = value;
				stack.insert(stack.end(), each.children.begin(), each.children.end());
			}
		}
		if (node.attribute("frameLevel")) {
			set_level(id, static_cast<int>(node.number("frameLevel")));
		}
		if (node.attribute("toplevel")) {
			w->toplevel = node.flag("toplevel");
		}
		if (node.attribute("movable")) {
			w->movable = node.flag("movable");
		}
		if (node.attribute("resizable")) {
			w->resizable = node.flag("resizable");
		}
		if (node.attribute("enableMouse")) {
			w->mouse = node.flag("enableMouse");
		}
		if (node.attribute("enableKeyboard")) {
			w->keyboard = node.flag("enableKeyboard");
		}
		if (node.attribute("clampedToScreen")) {
			w->clamped = node.flag("clampedToScreen");
		}
	}
	switch (w->kind) {
		case KIND_TEXTURE:
			if (const std::string *file = node.attribute("file")) {
				set_texture(id, godot_string(*file));
			}
			if (const std::string *mode = node.attribute("alphaMode")) {
				w->blend = *mode == "ADD" ? BLEND_ADD : *mode == "MOD" ? BLEND_MOD : *mode == "ALPHAKEY" ? BLEND_ALPHAKEY : *mode == "DISABLE" ? BLEND_DISABLE : BLEND_BLEND;
			}
			break;
		case KIND_FONT_STRING:
			apply_font(w->font, node);
			if (node.attribute("nonspacewrap")) {
				w->nonspacewrap = node.flag("nonspacewrap");
			}
			if (node.attribute("maxLines")) {
				w->max_lines = static_cast<int>(node.number("maxLines"));
			}
			break;
		case KIND_STATUS_BAR:
		case KIND_SLIDER:
			if (node.attribute("minValue")) {
				w->min_value = node.number("minValue");
			}
			if (node.attribute("maxValue")) {
				w->max_value = node.number("maxValue");
			}
			if (node.attribute("defaultValue")) {
				w->value = node.number("defaultValue");
			}
			if (node.attribute("valueStep")) {
				w->value_step = node.number("valueStep");
			}
			if (node.attribute("orientation")) {
				w->vertical = node.get("orientation") == "VERTICAL";
			}
			break;
		case KIND_EDIT_BOX:
			if (node.attribute("letters")) {
				w->max_letters = static_cast<int>(node.number("letters"));
			}
			if (node.attribute("numeric")) {
				w->numeric = node.flag("numeric");
			}
			if (node.attribute("password")) {
				w->password = node.flag("password");
			}
			if (node.attribute("multiLine")) {
				w->multi_line = node.flag("multiLine");
			}
			if (node.attribute("autoFocus")) {
				w->auto_focus = node.flag("autoFocus");
			}
			if (node.attribute("historyLines")) {
				w->history_lines = static_cast<int>(node.number("historyLines"));
			}
			if (node.attribute("ignoreArrows")) {
				w->ignore_arrows = node.flag("ignoreArrows");
			}
			break;
		case KIND_SCROLLING_MESSAGE_FRAME:
		case KIND_MESSAGE_FRAME:
			if (node.attribute("displayDuration")) {
				w->time_visible = node.number("displayDuration");
			}
			if (node.attribute("maxLines")) {
				w->max_messages = std::max(1, static_cast<int>(node.number("maxLines")));
			}
			if (node.attribute("insertMode")) {
				w->insert_top = node.get("insertMode") == "TOP";
			}
			if (node.attribute("fade")) {
				w->fading = node.flag("fade");
			}
			break;
		case KIND_CHECK_BUTTON:
			if (node.attribute("checked")) {
				w->checked = node.flag("checked");
			}
			break;
		case KIND_MODEL: {
			if (const std::string *file = node.attribute("file")) {
				w->cooldown_model = godot_string(*file).to_lower().contains("cooldown");
				w->state["Model"] = Array::make(godot_string(*file));
				model_call(id, "SetModel", Array::make(godot_string(*file)));
			}
			if (node.attribute("fogNear")) {
				model_call(id, "SetFogNear", Array::make(node.number("fogNear")));
			}
			if (node.attribute("fogFar")) {
				model_call(id, "SetFogFar", Array::make(node.number("fogFar")));
			}
			break;
		}
		default:
			break;
	}
	if (const std::string *text = node.attribute("text")) {
		// The text attribute names a global string, or is shown as written when there is none.
		lua_getglobal(L, text->c_str());
		String value = lua_isstring(L, -1) ? wowui::to_string(L, -1) : godot_string(*text);
		lua_pop(L, 1);
		set_text(id, value);
	}
	for (const auto &child : node.children) {
		apply_child(id, *child);
		w = widgets[id].get();
	}
}

int WowUI::button_texture(int id, const WowXmlNode &node, int layer) {
	int parent_id = id;
	String name = resolve_name(godot_string(node.get("name")), parent_id);
	int texture = create_widget(KIND_TEXTURE, "Texture", name, parent_id, true);
	widgets[texture]->layer = layer;
	apply_inherits(texture, node.get("inherits"));
	apply(texture, node);
	Widget &t = *widgets[texture];
	if (t.anchors.empty() && !t.has_width && !t.has_height) {
		// A button's state textures fill it unless placed.
		Anchor a;
		a.point = a.relative_point = TOPLEFT;
		t.anchors.push_back(a);
		a.point = a.relative_point = BOTTOMRIGHT;
		t.anchors.push_back(a);
	}
	return texture;
}

void WowUI::apply_child(int id, const WowXmlNode &child) {
	Widget *w = widgets[id].get();
	const std::string &tag = child.tag;
	if (tag == "Size") {
		apply_size(id, child);
	} else if (tag == "Anchors") {
		apply_anchors(id, child);
	} else if (w->kind == KIND_TEXTURE || w->kind == KIND_FONT_STRING) {
		apply_region(id, child);
	} else if (tag == "Layers") {
		for (const auto &layer_node : child.children) {
			if (layer_node->tag != "Layer") {
				continue;
			}
			int layer = wowui::layer_from(godot_string(layer_node->get("level", "ARTWORK")));
			for (const auto &region : layer_node->children) {
				int region_id = -1;
				if (region->tag == "Texture" || region->tag == "FontString") {
					if (region->flag("virtual")) {
						build(*region, id, true, false);
						continue;
					}
					const KindInfo *info = kind_for(region->tag);
					String name = resolve_name(godot_string(region->get("name")), id);
					region_id = create_widget(info->kind, info->tag, name, id, true);
					widgets[region_id]->layer = layer;
					apply_inherits(region_id, region->get("inherits"));
					apply(region_id, *region);
				}
			}
		}
	} else if (tag == "Frames") {
		for (const auto &frame : child.children) {
			build(*frame, id, false, true);
		}
	} else if (tag == "Scripts") {
		String owner = w->name.is_empty() ? String("anonymous ") + w->type : w->name;
		for (const auto &script : child.children) {
			int ref = compile(script->text, "=" + std_string(owner) + ":" + script->tag);
			set_script(id, script->tag, ref);
		}
	} else if (tag == "Backdrop") {
		if (!w->backdrop) {
			w->backdrop = std::make_unique<Backdrop>();
		}
		Backdrop &b = *w->backdrop;
		b.bg_file = godot_string(child.get("bgFile", std_string(b.bg_file)));
		b.edge_file = godot_string(child.get("edgeFile", std_string(b.edge_file)));
		if (child.attribute("tile")) {
			b.tile = child.flag("tile");
		}
		for (const auto &part : child.children) {
			if (part->tag == "EdgeSize") {
				b.edge_size = abs_value(*part);
			} else if (part->tag == "TileSize") {
				b.tile_size = abs_value(*part);
			} else if (part->tag == "BackgroundInsets") {
				inset(*part, b.inset_left, b.inset_right, b.inset_top, b.inset_bottom);
			} else if (part->tag == "Color") {
				b.color = xml_color(*part);
			} else if (part->tag == "BorderColor") {
				b.border_color = xml_color(*part);
			}
		}
	} else if (tag == "HitRectInsets") {
		inset(child, w->hit_left, w->hit_right, w->hit_top, w->hit_bottom);
	} else if (tag == "NormalTexture") {
		w->normal_texture = button_texture(id, child, LAYER_ARTWORK);
	} else if (tag == "PushedTexture") {
		w->pushed_texture = button_texture(id, child, LAYER_ARTWORK);
	} else if (tag == "DisabledTexture") {
		w->disabled_texture = button_texture(id, child, LAYER_ARTWORK);
	} else if (tag == "HighlightTexture") {
		w->highlight_texture = button_texture(id, child, LAYER_HIGHLIGHT);
	} else if (tag == "CheckedTexture") {
		w->checked_texture = button_texture(id, child, LAYER_OVERLAY);
	} else if (tag == "DisabledCheckedTexture") {
		w->disabled_checked_texture = button_texture(id, child, LAYER_OVERLAY);
	} else if (tag == "ButtonText" || tag == "NormalText") {
		int text = w->font_string;
		if (text < 0) {
			String name = resolve_name(godot_string(child.get("name")), id);
			text = create_widget(KIND_FONT_STRING, "FontString", name, id, true);
			widgets[text]->layer = LAYER_OVERLAY;
			widgets[id]->font_string = text;
		} else if (widgets[text]->name.is_empty()) {
			// The button's text attribute made the string first; the element still names it.
			String name = resolve_name(godot_string(child.get("name")), id);
			if (!name.is_empty()) {
				widgets[text]->name = name;
				push_widget(text);
				CharString utf8 = name.utf8();
				lua_setglobal(L, utf8.get_data());
			}
		}
		String pending = widgets[text]->text;
		apply_inherits(text, child.get("inherits"));
		apply(text, child);
		if (widgets[text]->text.is_empty() && !pending.is_empty()) {
			widgets[text]->text = pending;
		}
		apply_button_font(*widgets[id]);
	} else if (tag == "NormalFont" || tag == "HighlightFont" || tag == "DisabledFont") {
		FontInfo &font = tag == "NormalFont" ? w->normal_font : tag == "HighlightFont" ? w->highlight_font : w->disabled_font;
		inherit_font(font, child.get("inherits"));
		apply_font(font, child);
		(tag == "NormalFont" ? w->has_normal_font : tag == "HighlightFont" ? w->has_highlight_font : w->has_disabled_font) = true;
		apply_button_font(*w);
	} else if (tag == "PushedTextOffset") {
		float x, y;
		bool has_x, has_y;
		dimension(child, x, y, has_x, has_y);
		w->pushed_offset = Vector2(x, y);
	} else if (tag == "BarTexture" || tag == "ThumbTexture") {
		int layer = tag == "ThumbTexture" ? LAYER_OVERLAY : LAYER_ARTWORK;
		int texture = button_texture(id, child, layer);
		widgets[id]->bar_texture = texture;
		widgets[texture]->bar_owner = id;
	} else if (tag == "BarColor") {
		w->bar_color = xml_color(child);
		if (Widget *bar = widget(w->bar_texture)) {
			bar->vertex_color = w->bar_color;
		}
	} else if (tag == "FontString") {
		// An EditBox's or message frame's own font.
		inherit_font(w->font, child.get("inherits"));
		apply_font(w->font, child);
		if (child.attribute("nonspacewrap")) {
			w->nonspacewrap = child.flag("nonspacewrap");
		}
	} else if (tag == "TextInsets") {
		inset(child, w->text_left, w->text_right, w->text_top, w->text_bottom);
	} else if (tag == "ScrollChild") {
		for (const auto &frame : child.children) {
			int scroll = build(*frame, id, false, true);
			if (scroll >= 0) {
				widgets[id]->scroll_child = scroll;
				widgets[scroll]->scrolled_by = id;
			}
		}
	} else if (tag == "FogColor") {
		model_call(id, "SetFogColor", Array::make(child.number("r"), child.number("g"), child.number("b")));
	} else if (tag == "ColorWheelTexture" || tag == "ColorWheelThumbTexture" || tag == "ColorValueTexture" || tag == "ColorValueThumbTexture") {
		button_texture(id, child, LAYER_ARTWORK);
	}
}

void WowUI::apply_region(int id, const WowXmlNode &child) {
	Widget &w = *widgets[id];
	const std::string &tag = child.tag;
	if (tag == "TexCoords") {
		if (const WowXmlNode *rect = child.child("Rect")) {
			const char *names[8] = { "ULx", "ULy", "LLx", "LLy", "URx", "URy", "LRx", "LRy" };
			for (int i = 0; i < 8; ++i) {
				w.coords[i] = rect->number(names[i]);
			}
		} else {
			float l = child.number("left", 0.0f), r = child.number("right", 1.0f);
			float t = child.number("top", 0.0f), b = child.number("bottom", 1.0f);
			float coords[8] = { l, t, l, b, r, t, r, b };
			std::memcpy(w.coords, coords, sizeof(coords));
		}
	} else if (tag == "Color") {
		if (w.kind == KIND_FONT_STRING) {
			w.font.color = xml_color(child);
		} else if (w.texture_file.is_empty()) {
			w.solid = true;
			w.solid_color = xml_color(child);
		} else {
			w.vertex_color = xml_color(child);
		}
	} else if (tag == "Gradient") {
		w.gradient = true;
		w.gradient_vertical = child.get("orientation") == "VERTICAL";
		for (const auto &part : child.children) {
			if (part->tag == "MinColor") {
				w.gradient_min = xml_color(*part);
			} else if (part->tag == "MaxColor") {
				w.gradient_max = xml_color(*part);
			}
		}
	} else if (tag == "FontHeight" || tag == "Shadow") {
		if (tag == "FontHeight") {
			w.font.height = abs_value(child);
		} else {
			w.font.shadow = Color(0, 0, 0, 1);
			for (const auto &part : child.children) {
				if (part->tag == "Color") {
					w.font.shadow = xml_color(*part, Color(0, 0, 0, 1));
				} else if (part->tag == "Offset") {
					float x, y;
					bool has_x, has_y;
					dimension(*part, x, y, has_x, has_y);
					w.font.shadow_offset = Vector2(x, y);
				}
			}
		}
	}
}

void WowUI::apply_button_font(Widget &button) {
	Widget *text = widget(button.font_string);
	if (!text) {
		return;
	}
	const FontInfo *font = nullptr;
	if (!button.enabled && button.has_disabled_font) {
		font = &button.disabled_font;
	} else if ((button.locked_highlight || hovered == button.id) && button.enabled && button.has_highlight_font) {
		font = &button.highlight_font;
	} else if (button.has_normal_font) {
		font = &button.normal_font;
	}
	if (font) {
		text->font = *font;
		reset_layout(*text);
		invalidate();
	}
}

int WowUI::method_create(Kind kind, const String &type, const String &name, int parent, const std::string &inherits, int layer) {
	bool region = kind == KIND_TEXTURE || kind == KIND_FONT_STRING;
	int id = create_widget(kind, type, name, parent, region);
	if (region) {
		widgets[id]->layer = layer;
	}
	apply_inherits(id, inherits);
	if (!region) {
		load_on(id);
	}
	return id;
}

// Events and scripts.

void WowUI::register_event(int id, const std::string &event, bool on) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	std::vector<int> &list = event_frames[event];
	auto at = std::find(list.begin(), list.end(), id);
	if (on) {
		if (at == list.end()) {
			list.push_back(id);
		}
		w->events.insert(event);
	} else {
		if (at != list.end()) {
			list.erase(at);
		}
		w->events.erase(event);
	}
}

void WowUI::unregister_all(int id) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	for (const std::string &event : std::vector<std::string>(w->events.begin(), w->events.end())) {
		register_event(id, event, false);
	}
}

void WowUI::fire_event(const String &event, const Array &args) {
	std::string name = std_string(event);
	auto found = event_frames.find(name);
	if (found == event_frames.end()) {
		return;
	}
	std::vector<Variant> values;
	for (int64_t i = 0; i < args.size(); ++i) {
		values.push_back(args[i]);
	}
	std::vector<int> frames = found->second;
	for (int id : frames) {
		if (widgets[id]->events.count(name)) {
			call_script(id, "OnEvent", static_cast<int>(values.size()), values.data(), name.c_str());
		}
	}
}

void WowUI::update_frame(double delta) {
	now = (static_cast<double>(Time::get_singleton()->get_ticks_usec()) - start_usec) / 1000000.0;
	caret_time += delta;
	Vector2 size = get_size();
	if (size.y > 0.0f) {
		float width = SCREEN_HEIGHT * size.x / size.y;
		if (width != screen_width) {
			screen_width = width;
			invalidate();
		}
		px = size.y / SCREEN_HEIGHT;
	}
	if (moving >= 0) {
		Widget &w = *widgets[moving];
		Vector2 at = to_screen(mouse_position);
		float scale = effective_scale(w);
		w.anchors.clear();
		Anchor a;
		a.point = TOPLEFT;
		a.relative = -2;
		a.relative_point = BOTTOMLEFT;
		a.x = (at.x - w.move_grab.x) / scale;
		a.y = (at.y - w.move_grab.y) / scale;
		w.anchors.push_back(a);
		invalidate();
	}
	Variant elapsed = delta;
	size_t count = widgets.size();
	for (size_t i = 0; i < count; ++i) {
		Widget &w = *widgets[i];
		if (w.visible && !w.scripts.empty() && w.scripts.count("OnUpdate")) {
			call_script(static_cast<int>(i), "OnUpdate", 1, &elapsed);
		}
	}
	for (size_t i = 0; i < widgets.size(); ++i) {
		Widget &w = *widgets[i];
		if (!w.visible) {
			continue;
		}
		int id = static_cast<int>(i);
		if (w.kind == KIND_GAME_TOOLTIP && w.owner >= 0) {
			anchor_tooltip(w);
		} else if (w.kind == KIND_MODEL) {
			if (w.cooldown_finishing) {
				w.cooldown_finishing = false;
				call_script(id, "OnAnimFinished");
			}
			if (widgets[i]->visible && widgets[i]->scripts.count("OnUpdateModel")) {
				call_script(id, "OnUpdateModel");
			}
		} else if (w.kind == KIND_SCROLL_FRAME) {
			update_scroll_child(id);
		}
	}
}

void WowUI::set_parent(int id, int parent) {
	Widget *w = widget(id);
	if (!w || w->parent == parent || id == parent) {
		return;
	}
	bool region = w->kind == KIND_TEXTURE || w->kind == KIND_FONT_STRING;
	if (Widget *old = widget(w->parent)) {
		std::vector<int> &list = region ? old->regions : old->children;
		list.erase(std::remove(list.begin(), list.end(), id), list.end());
	}
	w->parent = parent;
	if (Widget *p = widget(parent)) {
		(region ? p->regions : p->children).push_back(id);
		if (!region) {
			w->strata = p->strata;
			set_level(id, p->level + 1);
		}
	}
	update_visibility(id, true);
	invalidate();
}

// Addons.

namespace {

String addon_name_of(const String &toc) {
	String path = toc.replace("/", "\\");
	PackedStringArray parts = path.split("\\", false);
	if (parts.size() < 2) {
		return String();
	}
	String folder = parts[parts.size() - 2];
	return parts[parts.size() - 1].get_basename().nocasecmp_to(folder) == 0 ? folder : String();
}

} // namespace

PackedStringArray WowUI::get_addons() const {
	PackedStringArray out;
	std::unordered_set<std::string> seen;
	auto add = [&](const String &name) {
		if (!name.is_empty() && seen.insert(std_string(name.to_lower())).second) {
			out.push_back(name);
		}
	};
	if (!disk_root.is_empty()) {
		String dir = disk_root.path_join("Interface/AddOns");
		PackedStringArray folders = DirAccess::get_directories_at(dir);
		for (const String &folder : folders) {
			if (FileAccess::file_exists(dir.path_join(folder).path_join(folder + String(".toc")))) {
				add(folder);
			}
		}
	}
	if (archive.is_valid()) {
		for (const String &toc : archive->find("Interface\\AddOns\\*.toc")) {
			add(addon_name_of(toc));
		}
	}
	return out;
}

Dictionary WowUI::get_addon_info(const String &name) const {
	Dictionary info;
	String toc = "Interface\\AddOns\\" + name + "\\" + name + ".toc";
	String text = read_text(toc);
	if (text.is_empty()) {
		return info;
	}
	info["name"] = name;
	info["toc"] = toc;
	for (const String &raw : text.split("\n")) {
		String line = raw.strip_edges();
		if (!line.begins_with("##")) {
			continue;
		}
		int colon = line.find(":");
		if (colon < 0) {
			continue;
		}
		info[line.substr(2, colon - 2).strip_edges()] = line.substr(colon + 1).strip_edges();
	}
	return info;
}

bool WowUI::is_addon_loaded(const String &name) const {
	return loaded_addons.count(std_string(name.to_lower())) > 0;
}

bool WowUI::load_addon(const String &name) {
	if (is_addon_loaded(name)) {
		return true;
	}
	Dictionary info = get_addon_info(name);
	if (info.is_empty()) {
		return false;
	}
	loaded_addons.insert(std_string(name.to_lower()));
	String deps = info.get("Dependencies", info.get("RequiredDeps", String()));
	for (const String &dep : deps.split(",", false)) {
		load_addon(dep.strip_edges());
	}
	load_toc(info["toc"]);
	// The host restores the addon's SavedVariables here, before the addon hears it is loaded.
	emit_signal("addon_files_loaded", name, info);
	fire_event("ADDON_LOADED", Array::make(name));
	return true;
}

// Host access.

void WowUI::register_function(const String &name, const Callable &callable) {
	functions.push_back(callable);
	lua_pushinteger(L, static_cast<lua_Integer>(functions.size() - 1));
	lua_pushcclosure(L, [](lua_State *state) -> int {
		WowUI *self = wowui::ui(state);
		int index = static_cast<int>(lua_tointeger(state, lua_upvalueindex(1)));
		self->call_function(index, lua_gettop(state));
		return lua_gettop(state);
	}, 1);
	CharString utf8 = name.utf8();
	lua_setglobal(L, utf8.get_data());
}

void WowUI::call_function(int index, int arg_count) {
	Array args;
	for (int i = 1; i <= arg_count; ++i) {
		int found = widget_from(i);
		if (found >= 0) {
			Dictionary ref;
			ref["widget"] = found;
			ref["name"] = widgets[found]->name;
			args.push_back(ref);
		} else {
			args.push_back(wowui::to_variant(L, i));
		}
	}
	lua_settop(L, 0);
	// Lua callers pass as many arguments as they like; the host function takes a fixed count.
	const int wanted = functions[index].get_argument_count();
	if (wanted >= 0) {
		args.resize(wanted);
	}
	Variant result = functions[index].callv(args);
	if (result.get_type() == Variant::ARRAY) {
		Array values = result;
		for (int64_t i = 0; i < values.size(); ++i) {
			wowui::push_variant(L, values[i]);
		}
	} else if (result.get_type() != Variant::NIL) {
		wowui::push_variant(L, result);
	}
}

Variant WowUI::call_lua(const String &function, const Array &args) {
	CharString utf8 = function.utf8();
	lua_getfield(L, LUA_REGISTRYINDEX, "wowui.traceback");
	int handler = lua_gettop(L);
	lua_getglobal(L, utf8.get_data());
	if (!lua_isfunction(L, -1)) {
		lua_settop(L, handler - 1);
		return Variant();
	}
	for (int64_t i = 0; i < args.size(); ++i) {
		wowui::push_variant(L, args[i]);
	}
	int top = handler;
	if (lua_pcall(L, static_cast<int>(args.size()), LUA_MULTRET, handler) != 0) {
		report(wowui::to_string(L, -1));
		lua_settop(L, handler - 1);
		return Variant();
	}
	Array results;
	for (int i = top + 1; i <= lua_gettop(L); ++i) {
		results.push_back(wowui::to_variant(L, i));
	}
	lua_settop(L, handler - 1);
	if (results.size() == 1) {
		return results[0];
	}
	return results.is_empty() ? Variant() : Variant(results);
}

Variant WowUI::get_lua_global(const String &name) {
	CharString utf8 = name.utf8();
	lua_getglobal(L, utf8.get_data());
	Variant value = wowui::to_variant(L, -1);
	lua_pop(L, 1);
	return value;
}

void WowUI::set_lua_global(const String &name, const Variant &value) {
	CharString utf8 = name.utf8();
	wowui::push_variant(L, value);
	lua_setglobal(L, utf8.get_data());
}

bool WowUI::run_lua(const String &code, const String &chunk_name) {
	return run_chunk(std_string(code), "=" + std_string(chunk_name));
}

Rect2 WowUI::get_widget_rect(const String &name) {
	Rect2 rect;
	int id = find(name);
	if (id >= 0) {
		screen_rect(id, rect);
	}
	return rect;
}

bool WowUI::is_widget_visible(const String &name) const {
	int id = find(name);
	return id >= 0 && widgets[id]->visible;
}

void WowUI::set_widget_texture(const String &name, const Ref<Texture2D> &texture) {
	set_widget_texture_id(find(name), texture);
}

void WowUI::set_widget_texture_id(int id, const Ref<Texture2D> &texture) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	if (w->kind == KIND_TEXTURE) {
		w->texture = texture;
		w->texture_file = String();
		w->solid = false;
	} else {
		w->external = texture;
	}
}

Array WowUI::get_model_frames() {
	Array out;
	for (const auto &w : widgets) {
		if (w->kind != KIND_MODEL && w->kind != KIND_MINIMAP) {
			continue;
		}
		Dictionary entry;
		entry["id"] = w->id;
		entry["name"] = w->name;
		entry["type"] = w->type;
		entry["visible"] = w->visible;
		entry["cooldown"] = w->cooldown_model;
		Rect2 rect;
		entry["has_rect"] = screen_rect(w->id, rect);
		entry["rect"] = rect;
		out.push_back(entry);
	}
	return out;
}

void WowUI::model_call(int id, const String &method, const Array &args) {
	emit_signal("model_called", id, method, args);
}

void WowUI::cvar_set(const String &name, const String &value) {
	cvars[name] = value;
	emit_signal("cvar_changed", name, value);
}

void WowUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_archive", "archive"), &WowUI::set_archive);
	ClassDB::bind_method(D_METHOD("get_archive"), &WowUI::get_archive);
	ClassDB::bind_method(D_METHOD("set_disk_root", "root"), &WowUI::set_disk_root);
	ClassDB::bind_method(D_METHOD("get_disk_root"), &WowUI::get_disk_root);
	ClassDB::bind_method(D_METHOD("has_file", "path"), &WowUI::has_file);
	ClassDB::bind_method(D_METHOD("load_toc", "path"), &WowUI::load_toc);
	ClassDB::bind_method(D_METHOD("load_xml", "path"), &WowUI::load_xml);
	ClassDB::bind_method(D_METHOD("get_addons"), &WowUI::get_addons);
	ClassDB::bind_method(D_METHOD("get_addon_info", "name"), &WowUI::get_addon_info);
	ClassDB::bind_method(D_METHOD("load_addon", "name"), &WowUI::load_addon);
	ClassDB::bind_method(D_METHOD("is_addon_loaded", "name"), &WowUI::is_addon_loaded);
	ClassDB::bind_method(D_METHOD("load_bindings", "path"), &WowUI::load_bindings);
	ClassDB::bind_method(D_METHOD("get_binding_names"), &WowUI::get_binding_names);
	ClassDB::bind_method(D_METHOD("run_binding", "name", "down"), &WowUI::run_binding);
	ClassDB::bind_method(D_METHOD("set_key_bindings", "values"), &WowUI::set_key_bindings);
	ClassDB::bind_method(D_METHOD("get_key_bindings"), &WowUI::get_key_bindings);
	ClassDB::bind_method(D_METHOD("run_key", "key", "down"), &WowUI::run_key);
	ClassDB::bind_method(D_METHOD("run_lua", "code", "chunk_name"), &WowUI::run_lua, DEFVAL("script"));
	ClassDB::bind_method(D_METHOD("register_function", "name", "callable"), &WowUI::register_function);
	ClassDB::bind_method(D_METHOD("fire_event", "event", "args"), &WowUI::fire_event, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("call_lua", "function", "args"), &WowUI::call_lua, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("get_lua_global", "name"), &WowUI::get_lua_global);
	ClassDB::bind_method(D_METHOD("set_lua_global", "name", "value"), &WowUI::set_lua_global);
	ClassDB::bind_method(D_METHOD("has_widget", "name"), &WowUI::has_widget);
	ClassDB::bind_method(D_METHOD("get_widget_rect", "name"), &WowUI::get_widget_rect);
	ClassDB::bind_method(D_METHOD("is_widget_visible", "name"), &WowUI::is_widget_visible);
	ClassDB::bind_method(D_METHOD("set_widget_texture", "name", "texture"), &WowUI::set_widget_texture);
	ClassDB::bind_method(D_METHOD("set_widget_texture_id", "id", "texture"), &WowUI::set_widget_texture_id);
	ClassDB::bind_method(D_METHOD("get_widget_name", "id"), &WowUI::get_widget_name);
	ClassDB::bind_method(D_METHOD("get_widget_at", "position"), &WowUI::get_widget_at);
	ClassDB::bind_method(D_METHOD("run_widget_script", "id", "handler"), &WowUI::run_widget_script);
	ClassDB::bind_method(D_METHOD("get_widget_id", "name"), &WowUI::get_widget_id);
	ClassDB::bind_method(D_METHOD("get_model_frames"), &WowUI::get_model_frames);
	ClassDB::bind_method(D_METHOD("set_cursor_busy", "busy"), &WowUI::set_cursor_busy);
	ClassDB::bind_method(D_METHOD("has_text_focus"), &WowUI::has_text_focus);
	ClassDB::bind_method(D_METHOD("clear_text_focus"), &WowUI::clear_text_focus);
	ClassDB::bind_method(D_METHOD("set_cvars", "values"), &WowUI::set_cvars);
	ClassDB::bind_method(D_METHOD("get_cvars"), &WowUI::get_cvars);
	ClassDB::bind_method(D_METHOD("get_errors"), &WowUI::get_errors);
	ClassDB::bind_method(D_METHOD("get_missing"), &WowUI::get_missing);
	ClassDB::bind_method(D_METHOD("widget_count"), &WowUI::widget_count);
	ClassDB::bind_method(D_METHOD("key_event", "event"), &WowUI::key_event);
	ClassDB::bind_static_method("WowUI", D_METHOD("event_key_name", "event"), &WowUI::event_key_name);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "archive", PROPERTY_HINT_RESOURCE_TYPE, "WowArchive", PROPERTY_USAGE_NONE), "set_archive", "get_archive");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "disk_root"), "set_disk_root", "get_disk_root");
	ADD_SIGNAL(MethodInfo("model_called", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "method"), PropertyInfo(Variant::ARRAY, "args")));
	ADD_SIGNAL(MethodInfo("addon_files_loaded", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::DICTIONARY, "info")));
	ADD_SIGNAL(MethodInfo("cvar_changed", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::STRING, "value")));
	ADD_SIGNAL(MethodInfo("binding_key", PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::BOOL, "down")));
	BIND_ENUM_CONSTANT(KIND_FRAME);
	BIND_ENUM_CONSTANT(KIND_MODEL);
	BIND_ENUM_CONSTANT(KIND_MINIMAP);
}

} // namespace godot
