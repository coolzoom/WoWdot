#include "wow_ui.h"
#include "wow_ui_lua.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

namespace godot {

namespace wowui {

WowUI *ui(lua_State *L) {
	lua_getfield(L, LUA_REGISTRYINDEX, "wowui");
	WowUI *self = static_cast<WowUI *>(lua_touserdata(L, -1));
	lua_pop(L, 1);
	return self;
}

void push_string(lua_State *L, const String &text) {
	CharString utf8 = text.utf8();
	lua_pushlstring(L, utf8.get_data(), static_cast<size_t>(utf8.length()));
}

String to_string(lua_State *L, int index) {
	size_t length = 0;
	const char *text = lua_tolstring(L, index, &length);
	return text ? String::utf8(text, static_cast<int64_t>(length)) : String();
}

void push_variant(lua_State *L, const Variant &value) {
	switch (value.get_type()) {
		case Variant::NIL:
			lua_pushnil(L);
			break;
		case Variant::BOOL:
			// The interface's flags are 1 or nil.
			if (static_cast<bool>(value)) {
				lua_pushnumber(L, 1);
			} else {
				lua_pushnil(L);
			}
			break;
		case Variant::INT:
			lua_pushnumber(L, static_cast<lua_Number>(static_cast<int64_t>(value)));
			break;
		case Variant::FLOAT:
			lua_pushnumber(L, static_cast<double>(value));
			break;
		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::NODE_PATH:
			push_string(L, value);
			break;
		case Variant::COLOR: {
			Color c = value;
			lua_createtable(L, 0, 4);
			lua_pushnumber(L, c.r);
			lua_setfield(L, -2, "r");
			lua_pushnumber(L, c.g);
			lua_setfield(L, -2, "g");
			lua_pushnumber(L, c.b);
			lua_setfield(L, -2, "b");
			lua_pushnumber(L, c.a);
			lua_setfield(L, -2, "a");
			break;
		}
		case Variant::ARRAY:
		case Variant::PACKED_STRING_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY: {
			Array items = value;
			lua_createtable(L, static_cast<int>(items.size()), 0);
			for (int64_t i = 0; i < items.size(); ++i) {
				push_variant(L, items[i]);
				lua_rawseti(L, -2, static_cast<int>(i + 1));
			}
			break;
		}
		case Variant::DICTIONARY: {
			Dictionary dict = value;
			if (dict.has("widget") && dict.size() <= 2) {
				WowUI *self = ui(L);
				int id = dict["widget"];
				if (self->widget(id)) {
					self->push(id);
					break;
				}
			}
			lua_createtable(L, 0, static_cast<int>(dict.size()));
			Array keys = dict.keys();
			for (int64_t i = 0; i < keys.size(); ++i) {
				push_variant(L, keys[i]);
				push_variant(L, dict[keys[i]]);
				lua_rawset(L, -3);
			}
			break;
		}
		default:
			lua_pushnil(L);
			break;
	}
}

Variant to_variant(lua_State *L, int index, int depth) {
	switch (lua_type(L, index)) {
		case LUA_TBOOLEAN:
			return static_cast<bool>(lua_toboolean(L, index));
		case LUA_TNUMBER: {
			double number = lua_tonumber(L, index);
			if (std::floor(number) == number && std::fabs(number) < 9007199254740992.0) {
				return static_cast<int64_t>(number);
			}
			return number;
		}
		case LUA_TSTRING:
			return to_string(L, index);
		case LUA_TTABLE: {
			if (index < 0) {
				index = lua_gettop(L) + index + 1;
			}
			WowUI *self = ui(L);
			int id = self->widget_from(index);
			if (id >= 0) {
				Dictionary ref;
				ref["widget"] = id;
				ref["name"] = self->get_widget_name(id);
				return ref;
			}
			if (depth > 4) {
				return Variant();
			}
			int length = static_cast<int>(lua_objlen(L, index));
			if (length > 0) {
				Array items;
				for (int i = 1; i <= length; ++i) {
					lua_rawgeti(L, index, i);
					items.push_back(to_variant(L, -1, depth + 1));
					lua_pop(L, 1);
				}
				return items;
			}
			Dictionary dict;
			lua_pushnil(L);
			while (lua_next(L, index)) {
				if (lua_type(L, -2) == LUA_TSTRING || lua_type(L, -2) == LUA_TNUMBER) {
					lua_pushvalue(L, -2);
					Variant key = to_variant(L, -1, depth + 1);
					lua_pop(L, 1);
					dict[key] = to_variant(L, -1, depth + 1);
				}
				lua_pop(L, 1);
			}
			return dict;
		}
		default:
			return Variant();
	}
}

namespace {

const char *POINTS[] = { "TOPLEFT", "TOP", "TOPRIGHT", "LEFT", "CENTER", "RIGHT", "BOTTOMLEFT", "BOTTOM", "BOTTOMRIGHT" };
const char *STRATA[] = { "BACKGROUND", "LOW", "MEDIUM", "HIGH", "DIALOG", "FULLSCREEN", "FULLSCREEN_DIALOG", "TOOLTIP" };
const char *LAYERS[] = { "BACKGROUND", "BORDER", "ARTWORK", "OVERLAY", "HIGHLIGHT" };

int index_of(const char *const *names, int count, const String &name, int fallback) {
	String upper = name.to_upper();
	for (int i = 0; i < count; ++i) {
		if (upper == names[i]) {
			return i;
		}
	}
	return fallback;
}

} // namespace

int point_from(const String &name) {
	return index_of(POINTS, 9, name, WowUI::TOPLEFT);
}

const char *point_name(int point) {
	return point >= 0 && point < 9 ? POINTS[point] : "TOPLEFT";
}

int strata_from(const String &name) {
	return index_of(STRATA, 8, name, 2);
}

const char *strata_name(int strata) {
	return strata >= 0 && strata < 8 ? STRATA[strata] : "MEDIUM";
}

int layer_from(const String &name) {
	return index_of(LAYERS, 5, name, WowUI::LAYER_ARTWORK);
}

const char *layer_name(int layer) {
	return layer >= 0 && layer < 5 ? LAYERS[layer] : "ARTWORK";
}

Color color_args(lua_State *L, int first, const Color &fallback) {
	auto part = [&](int offset, float value) {
		return lua_isnumber(L, first + offset) ? static_cast<float>(lua_tonumber(L, first + offset)) : value;
	};
	return Color(part(0, fallback.r), part(1, fallback.g), part(2, fallback.b), part(3, fallback.a));
}

} // namespace wowui

using namespace wowui;

struct Method {
	const char *name;
	lua_CFunction fn;
};

namespace {

const char *PRELUDE = R"LUA(
local string, table, math, os = string, table, math, os
strlen = string.len; strsub = string.sub; strfind = string.find; strlower = string.lower
strupper = string.upper; strrep = string.rep; strbyte = string.byte; strchar = string.char
strrev = string.reverse; format = string.format; gsub = string.gsub; gfind = string.gfind
strmatch = string.match
tinsert = table.insert; tremove = table.remove; getn = table.getn; setn = table.setn
sort = table.sort; foreach = table.foreach; foreachi = table.foreachi
floor = math.floor; ceil = math.ceil; abs = math.abs; mod = math.mod; max = math.max
min = math.min; sqrt = math.sqrt; exp = math.exp; log = math.log; log10 = math.log10
frexp = math.frexp; ldexp = math.ldexp; random = math.random; deg = math.deg; rad = math.rad
PI = math.pi
-- The interface's trigonometry is in degrees.
function sin(x) return math.sin(math.rad(x)) end
function cos(x) return math.cos(math.rad(x)) end
function tan(x) return math.tan(math.rad(x)) end
function asin(x) return math.deg(math.asin(x)) end
function acos(x) return math.deg(math.acos(x)) end
function atan(x) return math.deg(math.atan(x)) end
function atan2(y, x) return math.deg(math.atan2(y, x)) end
date = os.date; time = os.time
os = { date = os.date, time = os.time, clock = os.clock, difftime = os.difftime }
dofile = nil; loadfile = nil
function getglobal(name) return rawget(_G, name) end
function setglobal(name, value) rawset(_G, name, value) end
function strtrim(s) return (string.gsub(s or "", "^%s*(.-)%s*$", "%1")) end
function TEXT(text) return text end
function GetText(token, gender, ordinal)
	if gender == 3 and rawget(_G, token .. "_FEMALE") then return rawget(_G, token .. "_FEMALE") end
	return rawget(_G, token) or token
end
function GetBindingText(key, prefix)
	if not key then return "" end
	return rawget(_G, (prefix or "") .. key) or key
end
function debugstack(start) return debug.traceback("", (start or 1) + 1) end
local errorhandler = function(text) message(text) end
function seterrorhandler(handler) errorhandler = handler end
function geterrorhandler() return errorhandler end
function GetBuildInfo() return "Release", "Release", "1.12.1", "5875", "Sep 19 2006" end
function GetLocale() return "enUS" end
)LUA";

// Unknown engine functions answer nothing rather than stop the interface; frames are never guessed.
const char *const NOT_FUNCTIONS[] = { "Frame", "Button", "Text", "Bar", "Texture", "Box", "Icon", "Tab", "Slider", "Model", "Tooltip", "Menu", "List", "Panel", "Window", "Portrait", "String", "Border", "Background", "Header", "Highlight", "Label", "Title", "Arrow", "Scroll", "Child", "Parent", "DropDown", "Dropdown", "Cluster", "Template" };

bool looks_like_api(const char *name) {
	if (!name || name[0] < 'A' || name[0] > 'Z') {
		return false;
	}
	bool lower = false;
	for (const char *c = name; *c; ++c) {
		if (*c == '_' || (*c >= '0' && *c <= '9')) {
			return false;
		}
		lower = lower || (*c >= 'a' && *c <= 'z');
	}
	if (!lower) {
		return false;
	}
	size_t length = std::strlen(name);
	for (const char *suffix : NOT_FUNCTIONS) {
		size_t n = std::strlen(suffix);
		if (length >= n && std::strcmp(name + length - n, suffix) == 0) {
			return false;
		}
	}
	return true;
}

int missing_function(lua_State *L) {
	const char *name = lua_tostring(L, lua_upvalueindex(1));
	ui(L)->note_missing(name);
	if (std::strncmp(name, "GetNum", 6) == 0) {
		lua_pushnumber(L, 0);
		return 1;
	}
	return 0;
}

int global_index(lua_State *L) {
	if (lua_type(L, 2) != LUA_TSTRING || !looks_like_api(lua_tostring(L, 2))) {
		return 0;
	}
	lua_pushvalue(L, 2);
	lua_pushcclosure(L, missing_function, 1);
	return 1;
}

int missing_method(lua_State *L) {
	const char *name = lua_tostring(L, lua_upvalueindex(1));
	ui(L)->note_missing(std::string(":") + name);
	return 0;
}

int method_index(lua_State *L) {
	if (lua_type(L, 2) != LUA_TSTRING) {
		return 0;
	}
	// Only what reads as a method call; a script's own capitalised fields stay nil.
	static const char *const VERBS[] = { "Set", "Get", "Is", "Enable", "Disable", "Can", "Has", "Clear", "Add", "Register", "Unregister", "Update", "Start", "Stop", "Refresh" };
	const char *name = lua_tostring(L, 2);
	bool verb = false;
	for (const char *prefix : VERBS) {
		size_t n = std::strlen(prefix);
		if (std::strncmp(name, prefix, n) == 0 && name[n] >= 'A' && name[n] <= 'Z') {
			verb = true;
			break;
		}
	}
	if (!verb) {
		return 0;
	}
	lua_pushvalue(L, 2);
	lua_pushcclosure(L, missing_method, 1);
	return 1;
}

int traceback(lua_State *L) {
	const char *message = lua_tostring(L, 1);
	lua_getglobal(L, "debug");
	lua_getfield(L, -1, "traceback");
	lua_pushstring(L, message ? message : "(error object is not a string)");
	lua_pushinteger(L, 2);
	lua_call(L, 2, 1);
	return 1;
}

const std::pair<const char *, const char *> TYPE_PARENTS[] = {
	{ "Region", "UIObject" },
	{ "LayeredRegion", "Region" },
	{ "Texture", "LayeredRegion" },
	{ "FontString", "LayeredRegion" },
	{ "Font", "UIObject" },
	{ "Frame", "Region" },
	{ "Button", "Frame" },
	{ "CheckButton", "Button" },
	{ "LootButton", "Button" },
	{ "EditBox", "Frame" },
	{ "ScrollFrame", "Frame" },
	{ "ScrollingMessageFrame", "Frame" },
	{ "MessageFrame", "Frame" },
	{ "Slider", "Frame" },
	{ "StatusBar", "Frame" },
	{ "ColorSelect", "Frame" },
	{ "Cooldown", "Frame" },
	{ "MovieFrame", "Frame" },
	{ "WorldFrame", "Frame" },
	{ "TaxiRouteFrame", "Frame" },
	{ "SimpleHTML", "Frame" },
	{ "Minimap", "Frame" },
	{ "GameTooltip", "Frame" },
	{ "Model", "Frame" },
	{ "PlayerModel", "Model" },
	{ "DressUpModel", "PlayerModel" },
	{ "TabardModel", "PlayerModel" },
	{ "ModelFFX", "Model" },
};

const char *type_parent(const String &type) {
	for (const auto &entry : TYPE_PARENTS) {
		if (type == entry.first) {
			return entry.second;
		}
	}
	return nullptr;
}

void push_flag(lua_State *L, bool flag) {
	if (flag) {
		lua_pushnumber(L, 1);
	} else {
		lua_pushnil(L);
	}
}

float num(lua_State *L, int index, float fallback = 0.0f) {
	return lua_isnumber(L, index) ? static_cast<float>(lua_tonumber(L, index)) : fallback;
}

// Flags from the interface: nil, false, 0 and "0" are off.
bool flag(lua_State *L, int index) {
	if (lua_type(L, index) == LUA_TNUMBER) {
		return lua_tonumber(L, index) != 0;
	}
	if (lua_type(L, index) == LUA_TSTRING) {
		return std::strcmp(lua_tostring(L, index), "0") != 0;
	}
	return lua_toboolean(L, index);
}

void push_color(lua_State *L, const Color &c) {
	lua_pushnumber(L, c.r);
	lua_pushnumber(L, c.g);
	lua_pushnumber(L, c.b);
	lua_pushnumber(L, c.a);
}

std::string std_str(const String &text) {
	CharString utf8 = text.utf8();
	return std::string(utf8.get_data(), utf8.length());
}

} // namespace

#define SELF                                     \
	WowUI *ui_ = ui(L);                          \
	int id_ = ui_->check_widget(L, 1);           \
	WowUI::Widget &w = *ui_->widgets[id_];       \
	(void)w

struct WowUILua {
	static WowUI::FontInfo *font_of(WowUI *self, WowUI::Widget &w) {
		switch (w.kind) {
			case WowUI::KIND_FONT_STRING:
			case WowUI::KIND_FONT:
			case WowUI::KIND_EDIT_BOX:
			case WowUI::KIND_SCROLLING_MESSAGE_FRAME:
			case WowUI::KIND_MESSAGE_FRAME:
			case WowUI::KIND_SIMPLE_HTML:
				return &w.font;
			default:
				if (WowUI::Widget *text = self->widget(w.font_string)) {
					return &text->font;
				}
				return nullptr;
		}
	}

	static void font_touched(WowUI *self, WowUI::Widget &w) {
		self->reset_layout(w);
		if (WowUI::Widget *text = self->widget(w.font_string)) {
			self->reset_layout(*text);
		}
		for (WowUI::Message &m : w.messages) {
			m.layout.key = String();
		}
		self->invalidate();
	}

	static int widget_arg(WowUI *self, lua_State *L, int index) {
		if (lua_type(L, index) == LUA_TSTRING) {
			return self->find(to_string(L, index));
		}
		return self->widget_from(index);
	}

	static int ensure_text(WowUI *self, int id) {
		WowUI::Widget &w = *self->widgets[id];
		if (w.font_string >= 0) {
			return w.font_string;
		}
		int text = self->create_widget(WowUI::KIND_FONT_STRING, "FontString", String(), id, true);
		WowUI::Widget &t = *self->widgets[text];
		t.layer = WowUI::LAYER_OVERLAY;
		WowUI::Anchor center;
		center.point = center.relative_point = WowUI::CENTER;
		t.anchors.push_back(center);
		self->inherit_font(t.font, "GameFontNormal");
		self->widgets[id]->font_string = text;
		self->apply_button_font(*self->widgets[id]);
		return text;
	}

	static int button_slot(lua_State *L, int WowUI::Widget::*slot, int layer) {
		SELF;
		int given = ui_->widget_from(2);
		if (given >= 0) {
			w.*slot = given;
			ui_->set_parent(given, id_);
			ui_->widgets[given]->layer = layer;
			return 0;
		}
		if (w.*slot < 0) {
			if (lua_isnil(L, 2)) {
				return 0;
			}
			int texture = ui_->create_widget(WowUI::KIND_TEXTURE, "Texture", String(), id_, true);
			WowUI::Widget &t = *ui_->widgets[texture];
			t.layer = layer;
			WowUI::Anchor a;
			a.point = a.relative_point = WowUI::TOPLEFT;
			t.anchors.push_back(a);
			a.point = a.relative_point = WowUI::BOTTOMRIGHT;
			t.anchors.push_back(a);
			ui_->widgets[id_].get()->*slot = texture;
		}
		int texture = ui_->widgets[id_].get()->*slot;
		ui_->set_texture(texture, lua_isstring(L, 2) ? to_string(L, 2) : String());
		if (lua_type(L, 3) == LUA_TSTRING) {
			ui_->widgets[texture]->blend = std::strcmp(lua_tostring(L, 3), "ADD") == 0 ? WowUI::BLEND_ADD : WowUI::BLEND_BLEND;
		}
		return 0;
	}

	static int push_slot(lua_State *L, int id) {
		WowUI *self = ui(L);
		if (self->widget(id)) {
			self->push_widget(id);
		} else {
			lua_pushnil(L);
		}
		return 1;
	}

	static void set_strata(WowUI *self, int id, int strata) {
		std::vector<int> stack = { id };
		while (!stack.empty()) {
			WowUI::Widget &each = *self->widgets[stack.back()];
			stack.pop_back();
			each.strata = strata;
			stack.insert(stack.end(), each.children.begin(), each.children.end());
		}
		self->invalidate();
	}

	static int mouse_bits(lua_State *L, int first, int &r_up, int &r_down) {
		r_up = 0;
		r_down = 0;
		const char *names[] = { "LeftButton", "RightButton", "MiddleButton", "Button4", "Button5" };
		for (int i = first; i <= lua_gettop(L); ++i) {
			if (lua_type(L, i) != LUA_TSTRING) {
				continue;
			}
			std::string arg = lua_tostring(L, i);
			if (arg == "AnyUp") {
				r_up = 31;
			} else if (arg == "AnyDown") {
				r_down = 31;
			}
			for (int b = 0; b < 5; ++b) {
				std::string name = names[b];
				if (arg == name || arg == name + "Up") {
					r_up |= 1 << b;
				} else if (arg == name + "Down") {
					r_down |= 1 << b;
				}
			}
		}
		return 0;
	}

	static int edge(lua_State *L, int which) {
		SELF;
		float e[4];
		if (!ui_->widget_rect(id_, e[0], e[1], e[2], e[3])) {
			return 0;
		}
		lua_pushnumber(L, e[which] / ui_->effective_scale(w));
		return 1;
	}

	static int create_region(lua_State *L, WowUI::Kind kind, const char *type) {
		SELF;
		String name = lua_isstring(L, 2) ? ui_->resolve_name(to_string(L, 2), id_) : String();
		int layer = lua_isstring(L, 3) ? layer_from(to_string(L, 3)) : WowUI::LAYER_ARTWORK;
		std::string inherits = lua_isstring(L, 4) ? lua_tostring(L, 4) : "";
		int region = ui_->method_create(kind, type, name, id_, inherits, layer);
		ui_->push_widget(region);
		return 1;
	}

	static int button_font(lua_State *L, WowUI::FontInfo WowUI::Widget::*slot, bool WowUI::Widget::*has) {
		SELF;
		int source = widget_arg(ui_, L, 2);
		if (source < 0) {
			return 0;
		}
		w.*slot = ui_->widgets[source]->font;
		w.*has = true;
		ui_->apply_button_font(w);
		return 0;
	}

	static int button_color(lua_State *L, WowUI::FontInfo WowUI::Widget::*slot, bool WowUI::Widget::*has) {
		SELF;
		Color color = color_args(L, 2);
		if (!(w.*has)) {
			if (WowUI::Widget *text = ui_->widget(w.font_string)) {
				w.*slot = text->font;
			}
		}
		(w.*slot).color = color;
		w.*has = true;
		if (slot == &WowUI::Widget::normal_font) {
			if (WowUI::Widget *text = ui_->widget(w.font_string)) {
				text->font.color = color;
			}
		}
		ui_->apply_button_font(w);
		return 0;
	}

	static int visible_lines(WowUI *self, WowUI::Widget &w) {
		float l, b, r, t;
		if (!self->widget_rect(w.id, l, b, r, t)) {
			return 1;
		}
		float line = std::max(1.0f, w.font.height + w.font.spacing);
		return std::max(1, static_cast<int>((t - b) / self->effective_scale(w) / line));
	}

	// Methods.

	static constexpr Method UIOBJECT[] = {
		{ "GetName", [](lua_State *L) -> int {
			 SELF;
			 if (w.name.is_empty()) {
				 lua_pushnil(L);
			 } else {
				 push_string(L, w.name);
			 }
			 return 1;
		 } },
		{ "GetObjectType", [](lua_State *L) -> int {
			 SELF;
			 push_string(L, w.type == "LootButton" ? String("Button") : w.type);
			 return 1;
		 } },
		{ "IsObjectType", [](lua_State *L) -> int {
			 SELF;
			 String wanted = to_string(L, 2);
			 bool found = false;
			 for (String type = w.type; !type.is_empty();) {
				 if (type.nocasecmp_to(wanted) == 0) {
					 found = true;
					 break;
				 }
				 const char *parent = type_parent(type);
				 type = parent ? String(parent) : String();
			 }
			 push_flag(L, found);
			 return 1;
		 } },
		{ "GetParent", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.parent);
		 } },
		{ "GetAlpha", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.alpha);
			 return 1;
		 } },
		{ "SetAlpha", [](lua_State *L) -> int {
			 SELF;
			 w.alpha = std::clamp(num(L, 2, 1.0f), 0.0f, 1.0f);
			 return 0;
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method REGION[] = {
		{ "SetParent", [](lua_State *L) -> int {
			 SELF;
			 ui_->set_parent(id_, widget_arg(ui_, L, 2));
			 return 0;
		 } },
		{ "Show", [](lua_State *L) -> int {
			 SELF;
			 if (w.kind == WowUI::KIND_GAME_TOOLTIP) {
				 ui_->tooltip_show(id_);
			 } else {
				 ui_->set_shown(id_, true);
			 }
			 return 0;
		 } },
		{ "Hide", [](lua_State *L) -> int {
			 SELF;
			 ui_->set_shown(id_, false);
			 return 0;
		 } },
		{ "IsShown", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.shown);
			 return 1;
		 } },
		{ "IsVisible", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.visible);
			 return 1;
		 } },
		{ "SetPoint", [](lua_State *L) -> int {
			 SELF;
			 ui_->method_set_point(id_, 2);
			 return 0;
		 } },
		{ "SetAllPoints", [](lua_State *L) -> int {
			 SELF;
			 int relative = lua_isnoneornil(L, 2) ? -1 : widget_arg(ui_, L, 2);
			 w.anchors.clear();
			 WowUI::Anchor a;
			 a.relative = relative == w.parent ? -1 : relative;
			 a.point = a.relative_point = WowUI::TOPLEFT;
			 w.anchors.push_back(a);
			 a.point = a.relative_point = WowUI::BOTTOMRIGHT;
			 w.anchors.push_back(a);
			 ui_->invalidate();
			 return 0;
		 } },
		{ "ClearAllPoints", [](lua_State *L) -> int {
			 SELF;
			 w.anchors.clear();
			 ui_->invalidate();
			 return 0;
		 } },
		{ "GetNumPoints", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, static_cast<double>(w.anchors.size()));
			 return 1;
		 } },
		{ "GetPoint", [](lua_State *L) -> int {
			 SELF;
			 int index = static_cast<int>(num(L, 2, 1.0f)) - 1;
			 if (index < 0 || index >= static_cast<int>(w.anchors.size())) {
				 return 0;
			 }
			 const WowUI::Anchor &a = w.anchors[index];
			 lua_pushstring(L, point_name(a.point));
			 int relative = a.relative == -1 ? w.parent : a.relative == -3 ? ui_->find(a.relative_name) : a.relative;
			 push_slot(L, relative);
			 lua_pushstring(L, point_name(a.relative_point));
			 lua_pushnumber(L, a.x);
			 lua_pushnumber(L, a.y);
			 return 5;
		 } },
		{ "SetWidth", [](lua_State *L) -> int {
			 SELF;
			 w.width = num(L, 2);
			 w.has_width = w.width != 0.0f;
			 ui_->reset_layout(w);
			 ui_->invalidate();
			 return 0;
		 } },
		{ "SetHeight", [](lua_State *L) -> int {
			 SELF;
			 w.height = num(L, 2);
			 w.has_height = w.height != 0.0f;
			 ui_->invalidate();
			 return 0;
		 } },
		{ "SetSize", [](lua_State *L) -> int {
			 SELF;
			 w.width = num(L, 2);
			 w.height = num(L, 3, w.width);
			 w.has_width = w.width != 0.0f;
			 w.has_height = w.height != 0.0f;
			 ui_->reset_layout(w);
			 ui_->invalidate();
			 return 0;
		 } },
		{ "GetWidth", [](lua_State *L) -> int {
			 SELF;
			 float l, b, r, t;
			 if (ui_->widget_rect(id_, l, b, r, t)) {
				 lua_pushnumber(L, (r - l) / ui_->effective_scale(w));
			 } else if (w.kind == WowUI::KIND_FONT_STRING && !w.has_width) {
				 lua_pushnumber(L, ui_->text_width(w));
			 } else {
				 lua_pushnumber(L, w.width);
			 }
			 return 1;
		 } },
		{ "GetHeight", [](lua_State *L) -> int {
			 SELF;
			 float l, b, r, t;
			 if (ui_->widget_rect(id_, l, b, r, t)) {
				 lua_pushnumber(L, (t - b) / ui_->effective_scale(w));
			 } else if (w.kind == WowUI::KIND_FONT_STRING && !w.has_height) {
				 lua_pushnumber(L, ui_->text_height(id_));
			 } else {
				 lua_pushnumber(L, w.height);
			 }
			 return 1;
		 } },
		{ "GetLeft", [](lua_State *L) -> int { return edge(L, 0); } },
		{ "GetBottom", [](lua_State *L) -> int { return edge(L, 1); } },
		{ "GetRight", [](lua_State *L) -> int { return edge(L, 2); } },
		{ "GetTop", [](lua_State *L) -> int { return edge(L, 3); } },
		{ "GetCenter", [](lua_State *L) -> int {
			 SELF;
			 float l, b, r, t;
			 if (!ui_->widget_rect(id_, l, b, r, t)) {
				 return 0;
			 }
			 float s = ui_->effective_scale(w);
			 lua_pushnumber(L, (l + r) * 0.5f / s);
			 lua_pushnumber(L, (b + t) * 0.5f / s);
			 return 2;
		 } },
		{ "GetRect", [](lua_State *L) -> int {
			 SELF;
			 float l, b, r, t;
			 if (!ui_->widget_rect(id_, l, b, r, t)) {
				 return 0;
			 }
			 float s = ui_->effective_scale(w);
			 lua_pushnumber(L, l / s);
			 lua_pushnumber(L, b / s);
			 lua_pushnumber(L, (r - l) / s);
			 lua_pushnumber(L, (t - b) / s);
			 return 4;
		 } },
		{ "IsProtected", [](lua_State *) -> int { return 0; } },
		{ nullptr, nullptr },
	};

	static constexpr Method LAYERED[] = {
		{ "SetDrawLayer", [](lua_State *L) -> int {
			 SELF;
			 w.layer = layer_from(to_string(L, 2));
			 return 0;
		 } },
		{ "GetDrawLayer", [](lua_State *L) -> int {
			 SELF;
			 lua_pushstring(L, layer_name(w.layer));
			 return 1;
		 } },
		{ "SetVertexColor", [](lua_State *L) -> int {
			 SELF;
			 w.vertex_color = color_args(L, 2);
			 return 0;
		 } },
		{ "GetVertexColor", [](lua_State *L) -> int {
			 SELF;
			 push_color(L, w.vertex_color);
			 return 4;
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method TEXTURE[] = {
		{ "SetTexture", [](lua_State *L) -> int {
			 SELF;
			 if (lua_isnumber(L, 2)) {
				 w.texture_file = String();
				 w.texture = Ref<Texture2D>();
				 w.solid = true;
				 w.solid_color = color_args(L, 2);
			 } else {
				 ui_->set_texture(id_, lua_isstring(L, 2) ? to_string(L, 2) : String());
			 }
			 lua_pushnumber(L, 1);
			 return 1;
		 } },
		{ "GetTexture", [](lua_State *L) -> int {
			 SELF;
			 if (w.solid) {
				 lua_pushstring(L, "SolidTexture");
			 } else if (w.texture_file.is_empty()) {
				 lua_pushnil(L);
			 } else {
				 push_string(L, w.texture_file);
			 }
			 return 1;
		 } },
		{ "SetTexCoord", [](lua_State *L) -> int {
			 SELF;
			 if (lua_isnumber(L, 9)) {
				 for (int i = 0; i < 8; ++i) {
					 w.coords[i] = num(L, 2 + i);
				 }
			 } else {
				 float l = num(L, 2), r = num(L, 3, 1.0f), t = num(L, 4), b = num(L, 5, 1.0f);
				 float coords[8] = { l, t, l, b, r, t, r, b };
				 std::memcpy(w.coords, coords, sizeof(coords));
			 }
			 return 0;
		 } },
		{ "GetTexCoord", [](lua_State *L) -> int {
			 SELF;
			 for (float c : w.coords) {
				 lua_pushnumber(L, c);
			 }
			 return 8;
		 } },
		{ "SetDesaturated", [](lua_State *L) -> int {
			 SELF;
			 w.desaturated = flag(L, 2);
			 lua_pushnumber(L, 1);
			 return 1;
		 } },
		{ "IsDesaturated", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.desaturated);
			 return 1;
		 } },
		{ "SetBlendMode", [](lua_State *L) -> int {
			 SELF;
			 String mode = to_string(L, 2);
			 w.blend = mode == "ADD" ? WowUI::BLEND_ADD : mode == "MOD" ? WowUI::BLEND_MOD : mode == "ALPHAKEY" ? WowUI::BLEND_ALPHAKEY : mode == "DISABLE" ? WowUI::BLEND_DISABLE : WowUI::BLEND_BLEND;
			 return 0;
		 } },
		{ "GetBlendMode", [](lua_State *L) -> int {
			 SELF;
			 const char *names[] = { "BLEND", "ADD", "MOD", "ALPHAKEY", "DISABLE" };
			 lua_pushstring(L, names[w.blend]);
			 return 1;
		 } },
		{ "SetGradient", [](lua_State *L) -> int {
			 SELF;
			 w.gradient = true;
			 w.gradient_vertical = to_string(L, 2) == "VERTICAL";
			 w.gradient_min = Color(num(L, 3, 1), num(L, 4, 1), num(L, 5, 1), 1);
			 w.gradient_max = Color(num(L, 6, 1), num(L, 7, 1), num(L, 8, 1), 1);
			 return 0;
		 } },
		{ "SetGradientAlpha", [](lua_State *L) -> int {
			 SELF;
			 w.gradient = true;
			 w.gradient_vertical = to_string(L, 2) == "VERTICAL";
			 w.gradient_min = Color(num(L, 3, 1), num(L, 4, 1), num(L, 5, 1), num(L, 6, 1));
			 w.gradient_max = Color(num(L, 7, 1), num(L, 8, 1), num(L, 9, 1), num(L, 10, 1));
			 return 0;
		 } },
		{ "SetTexCoordModifiesRect", [](lua_State *) -> int { return 0; } },
		{ nullptr, nullptr },
	};

	static constexpr Method FONT_INSTANCE[] = {
		{ "SetFont", [](lua_State *L) -> int {
			 SELF;
			 WowUI::FontInfo *font = font_of(ui_, w);
			 if (!font) {
				 return 0;
			 }
			 font->file = to_string(L, 2);
			 font->height = num(L, 3, font->height);
			 String flags = lua_isstring(L, 4) ? to_string(L, 4) : String();
			 font->outline = flags.contains("OUTLINE");
			 font->thick = flags.contains("THICKOUTLINE");
			 font_touched(ui_, w);
			 lua_pushnumber(L, 1);
			 return 1;
		 } },
		{ "GetFont", [](lua_State *L) -> int {
			 SELF;
			 WowUI::FontInfo *font = font_of(ui_, w);
			 if (!font) {
				 return 0;
			 }
			 push_string(L, font->file);
			 lua_pushnumber(L, font->height);
			 lua_pushstring(L, font->thick ? "THICKOUTLINE" : font->outline ? "OUTLINE" : "");
			 return 3;
		 } },
		{ "SetFontObject", [](lua_State *L) -> int {
			 SELF;
			 WowUI::FontInfo *font = font_of(ui_, w);
			 int source = widget_arg(ui_, L, 2);
			 if (!font || source < 0) {
				 return 0;
			 }
			 *font = ui_->widgets[source]->font;
			 font_touched(ui_, w);
			 return 0;
		 } },
		{ "GetFontObject", [](lua_State *) -> int { return 0; } },
		{ "SetTextColor", [](lua_State *L) -> int {
			 SELF;
			 if (WowUI::FontInfo *font = font_of(ui_, w)) {
				 font->color = color_args(L, 2);
			 }
			 return 0;
		 } },
		{ "GetTextColor", [](lua_State *L) -> int {
			 SELF;
			 WowUI::FontInfo *font = font_of(ui_, w);
			 push_color(L, font ? font->color : Color(1, 1, 1, 1));
			 return 4;
		 } },
		{ "SetShadowColor", [](lua_State *L) -> int {
			 SELF;
			 if (WowUI::FontInfo *font = font_of(ui_, w)) {
				 font->shadow = color_args(L, 2, Color(0, 0, 0, 1));
			 }
			 return 0;
		 } },
		{ "GetShadowColor", [](lua_State *L) -> int {
			 SELF;
			 WowUI::FontInfo *font = font_of(ui_, w);
			 push_color(L, font ? font->shadow : Color(0, 0, 0, 0));
			 return 4;
		 } },
		{ "SetShadowOffset", [](lua_State *L) -> int {
			 SELF;
			 if (WowUI::FontInfo *font = font_of(ui_, w)) {
				 font->shadow_offset = Vector2(num(L, 2), num(L, 3));
			 }
			 return 0;
		 } },
		{ "GetShadowOffset", [](lua_State *L) -> int {
			 SELF;
			 WowUI::FontInfo *font = font_of(ui_, w);
			 lua_pushnumber(L, font ? font->shadow_offset.x : 0.0f);
			 lua_pushnumber(L, font ? font->shadow_offset.y : 0.0f);
			 return 2;
		 } },
		{ "SetJustifyH", [](lua_State *L) -> int {
			 SELF;
			 if (WowUI::FontInfo *font = font_of(ui_, w)) {
				 String v = to_string(L, 2).to_upper();
				 font->justify_h = v == "LEFT" ? WowUI::JUSTIFY_START : v == "RIGHT" ? WowUI::JUSTIFY_END : WowUI::JUSTIFY_MIDDLE;
				 font_touched(ui_, w);
			 }
			 return 0;
		 } },
		{ "SetJustifyV", [](lua_State *L) -> int {
			 SELF;
			 if (WowUI::FontInfo *font = font_of(ui_, w)) {
				 String v = to_string(L, 2).to_upper();
				 font->justify_v = v == "TOP" ? WowUI::JUSTIFY_START : v == "BOTTOM" ? WowUI::JUSTIFY_END : WowUI::JUSTIFY_MIDDLE;
			 }
			 return 0;
		 } },
		{ "GetJustifyH", [](lua_State *L) -> int {
			 SELF;
			 WowUI::FontInfo *font = font_of(ui_, w);
			 int j = font ? font->justify_h : WowUI::JUSTIFY_MIDDLE;
			 lua_pushstring(L, j == WowUI::JUSTIFY_START ? "LEFT" : j == WowUI::JUSTIFY_END ? "RIGHT" : "CENTER");
			 return 1;
		 } },
		{ "GetJustifyV", [](lua_State *L) -> int {
			 SELF;
			 WowUI::FontInfo *font = font_of(ui_, w);
			 int j = font ? font->justify_v : WowUI::JUSTIFY_MIDDLE;
			 lua_pushstring(L, j == WowUI::JUSTIFY_START ? "TOP" : j == WowUI::JUSTIFY_END ? "BOTTOM" : "MIDDLE");
			 return 1;
		 } },
		{ "SetSpacing", [](lua_State *L) -> int {
			 SELF;
			 if (WowUI::FontInfo *font = font_of(ui_, w)) {
				 font->spacing = num(L, 2);
				 font_touched(ui_, w);
			 }
			 return 0;
		 } },
		{ "GetSpacing", [](lua_State *L) -> int {
			 SELF;
			 WowUI::FontInfo *font = font_of(ui_, w);
			 lua_pushnumber(L, font ? font->spacing : 0.0f);
			 return 1;
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method FONT_STRING[] = {
		{ "SetText", [](lua_State *L) -> int {
			 SELF;
			 ui_->set_text(id_, lua_isstring(L, 2) ? to_string(L, 2) : String());
			 return 0;
		 } },
		{ "GetText", [](lua_State *L) -> int {
			 SELF;
			 if (w.text.is_empty() && w.kind != WowUI::KIND_EDIT_BOX) {
				 lua_pushnil(L);
			 } else {
				 push_string(L, w.text);
			 }
			 return 1;
		 } },
		{ "SetFormattedText", [](lua_State *L) -> int {
			 SELF;
			 lua_getglobal(L, "string");
			 lua_getfield(L, -1, "format");
			 int top = lua_gettop(L);
			 for (int i = 2; i < top - 1; ++i) {
				 lua_pushvalue(L, i);
			 }
			 lua_call(L, top - 3, 1);
			 ui_->set_text(id_, to_string(L, -1));
			 return 0;
		 } },
		{ "GetStringWidth", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, ui_->text_width(w));
			 return 1;
		 } },
		{ "GetStringHeight", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, ui_->text_height(id_));
			 return 1;
		 } },
		{ "SetTextHeight", [](lua_State *L) -> int {
			 SELF;
			 w.font.height = num(L, 2, w.font.height);
			 font_touched(ui_, w);
			 return 0;
		 } },
		{ "SetNonSpaceWrap", [](lua_State *L) -> int {
			 SELF;
			 w.nonspacewrap = flag(L, 2);
			 ui_->reset_layout(w);
			 return 0;
		 } },
		{ "CanNonSpaceWrap", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.nonspacewrap);
			 return 1;
		 } },
		{ "SetAlphaGradient", [](lua_State *L) -> int {
			 lua_pushnumber(L, 1);
			 return 1;
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method FRAME[] = {
		{ "RegisterEvent", [](lua_State *L) -> int {
			 SELF;
			 ui_->register_event(id_, luaL_checkstring(L, 2), true);
			 return 0;
		 } },
		{ "UnregisterEvent", [](lua_State *L) -> int {
			 SELF;
			 ui_->register_event(id_, luaL_checkstring(L, 2), false);
			 return 0;
		 } },
		{ "UnregisterAllEvents", [](lua_State *L) -> int {
			 SELF;
			 ui_->unregister_all(id_);
			 return 0;
		 } },
		{ "IsEventRegistered", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.events.count(luaL_checkstring(L, 2)) > 0);
			 return 1;
		 } },
		{ "SetScript", [](lua_State *L) -> int {
			 SELF;
			 std::string handler = luaL_checkstring(L, 2);
			 int ref = LUA_NOREF;
			 if (lua_isfunction(L, 3)) {
				 lua_pushvalue(L, 3);
				 ref = luaL_ref(L, LUA_REGISTRYINDEX);
			 }
			 ui_->set_script(id_, handler, ref);
			 return 0;
		 } },
		{ "GetScript", [](lua_State *L) -> int {
			 SELF;
			 auto found = w.scripts.find(luaL_checkstring(L, 2));
			 if (found == w.scripts.end()) {
				 return 0;
			 }
			 lua_rawgeti(L, LUA_REGISTRYINDEX, found->second);
			 return 1;
		 } },
		{ "HasScript", [](lua_State *L) -> int {
			 lua_pushnumber(L, 1);
			 return 1;
		 } },
		{ "SetFrameStrata", [](lua_State *L) -> int {
			 SELF;
			 set_strata(ui_, id_, strata_from(to_string(L, 2)));
			 return 0;
		 } },
		{ "GetFrameStrata", [](lua_State *L) -> int {
			 SELF;
			 lua_pushstring(L, strata_name(w.strata));
			 return 1;
		 } },
		{ "SetFrameLevel", [](lua_State *L) -> int {
			 SELF;
			 ui_->set_level(id_, static_cast<int>(num(L, 2)));
			 return 0;
		 } },
		{ "GetFrameLevel", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.level);
			 return 1;
		 } },
		{ "SetID", [](lua_State *L) -> int {
			 SELF;
			 w.frame_id = static_cast<int>(num(L, 2));
			 return 0;
		 } },
		{ "GetID", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.frame_id);
			 return 1;
		 } },
		{ "EnableMouse", [](lua_State *L) -> int {
			 SELF;
			 w.mouse = flag(L, 2);
			 return 0;
		 } },
		{ "IsMouseEnabled", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.mouse);
			 return 1;
		 } },
		{ "EnableMouseWheel", [](lua_State *L) -> int {
			 SELF;
			 w.wheel = flag(L, 2);
			 return 0;
		 } },
		{ "IsMouseWheelEnabled", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.wheel);
			 return 1;
		 } },
		{ "EnableKeyboard", [](lua_State *L) -> int {
			 SELF;
			 w.keyboard = flag(L, 2);
			 return 0;
		 } },
		{ "IsKeyboardEnabled", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.keyboard);
			 return 1;
		 } },
		{ "SetMovable", [](lua_State *L) -> int {
			 SELF;
			 w.movable = flag(L, 2);
			 return 0;
		 } },
		{ "IsMovable", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.movable);
			 return 1;
		 } },
		{ "SetResizable", [](lua_State *L) -> int {
			 SELF;
			 w.resizable = flag(L, 2);
			 return 0;
		 } },
		{ "IsResizable", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.resizable);
			 return 1;
		 } },
		{ "StartMoving", [](lua_State *L) -> int {
			 SELF;
			 ui_->start_moving(id_);
			 return 0;
		 } },
		{ "StartSizing", [](lua_State *) -> int { return 0; } },
		{ "StopMovingOrSizing", [](lua_State *L) -> int {
			 SELF;
			 ui_->stop_moving(id_);
			 return 0;
		 } },
		{ "SetUserPlaced", [](lua_State *L) -> int {
			 SELF;
			 w.user_placed = flag(L, 2);
			 return 0;
		 } },
		{ "IsUserPlaced", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.user_placed);
			 return 1;
		 } },
		{ "RegisterForDrag", [](lua_State *L) -> int {
			 SELF;
			 int up, down;
			 mouse_bits(L, 2, up, down);
			 w.drag_buttons = up | down;
			 return 0;
		 } },
		{ "SetBackdrop", [](lua_State *L) -> int {
			 SELF;
			 if (!lua_istable(L, 2)) {
				 w.backdrop.reset();
				 return 0;
			 }
			 if (!w.backdrop) {
				 w.backdrop = std::make_unique<WowUI::Backdrop>();
			 }
			 WowUI::Backdrop &b = *w.backdrop;
			 lua_getfield(L, 2, "bgFile");
			 b.bg_file = lua_isstring(L, -1) ? to_string(L, -1) : String();
			 lua_getfield(L, 2, "edgeFile");
			 b.edge_file = lua_isstring(L, -1) ? to_string(L, -1) : String();
			 lua_getfield(L, 2, "tile");
			 b.tile = lua_toboolean(L, -1);
			 lua_getfield(L, 2, "tileSize");
			 b.tile_size = num(L, -1);
			 lua_getfield(L, 2, "edgeSize");
			 b.edge_size = num(L, -1);
			 lua_pop(L, 5);
			 b.inset_left = b.inset_right = b.inset_top = b.inset_bottom = 0.0f;
			 lua_getfield(L, 2, "insets");
			 if (lua_istable(L, -1)) {
				 lua_getfield(L, -1, "left");
				 b.inset_left = num(L, -1);
				 lua_getfield(L, -2, "right");
				 b.inset_right = num(L, -1);
				 lua_getfield(L, -3, "top");
				 b.inset_top = num(L, -1);
				 lua_getfield(L, -4, "bottom");
				 b.inset_bottom = num(L, -1);
				 lua_pop(L, 4);
			 }
			 lua_pop(L, 1);
			 return 0;
		 } },
		{ "SetBackdropColor", [](lua_State *L) -> int {
			 SELF;
			 if (w.backdrop) {
				 w.backdrop->color = color_args(L, 2);
			 }
			 return 0;
		 } },
		{ "SetBackdropBorderColor", [](lua_State *L) -> int {
			 SELF;
			 if (w.backdrop) {
				 w.backdrop->border_color = color_args(L, 2);
			 }
			 return 0;
		 } },
		{ "GetBackdropColor", [](lua_State *L) -> int {
			 SELF;
			 push_color(L, w.backdrop ? w.backdrop->color : Color(1, 1, 1, 1));
			 return 4;
		 } },
		{ "GetBackdropBorderColor", [](lua_State *L) -> int {
			 SELF;
			 push_color(L, w.backdrop ? w.backdrop->border_color : Color(1, 1, 1, 1));
			 return 4;
		 } },
		{ "SetScale", [](lua_State *L) -> int {
			 SELF;
			 float scale = num(L, 2, 1.0f);
			 w.scale = scale > 0.0f ? scale : 1.0f;
			 ui_->invalidate();
			 return 0;
		 } },
		{ "GetScale", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.scale);
			 return 1;
		 } },
		{ "GetEffectiveScale", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, ui_->effective_scale(w));
			 return 1;
		 } },
		{ "CreateTexture", [](lua_State *L) -> int { return create_region(L, WowUI::KIND_TEXTURE, "Texture"); } },
		{ "CreateFontString", [](lua_State *L) -> int { return create_region(L, WowUI::KIND_FONT_STRING, "FontString"); } },
		{ "GetChildren", [](lua_State *L) -> int {
			 SELF;
			 std::vector<int> list = w.children;
			 luaL_checkstack(L, static_cast<int>(list.size()) + 1, "too many children");
			 for (int child : list) {
				 ui_->push_widget(child);
			 }
			 return static_cast<int>(list.size());
		 } },
		{ "GetRegions", [](lua_State *L) -> int {
			 SELF;
			 std::vector<int> list = w.regions;
			 luaL_checkstack(L, static_cast<int>(list.size()) + 1, "too many regions");
			 for (int child : list) {
				 ui_->push_widget(child);
			 }
			 return static_cast<int>(list.size());
		 } },
		{ "GetNumChildren", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, static_cast<double>(w.children.size()));
			 return 1;
		 } },
		{ "GetNumRegions", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, static_cast<double>(w.regions.size()));
			 return 1;
		 } },
		{ "Raise", [](lua_State *L) -> int {
			 SELF;
			 ui_->raise(id_);
			 return 0;
		 } },
		{ "Lower", [](lua_State *L) -> int {
			 SELF;
			 int base = 0;
			 if (WowUI::Widget *p = ui_->widget(w.parent)) {
				 base = p->level + 1;
			 }
			 ui_->set_level(id_, base);
			 return 0;
		 } },
		{ "SetToplevel", [](lua_State *L) -> int {
			 SELF;
			 w.toplevel = flag(L, 2);
			 return 0;
		 } },
		{ "IsToplevel", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.toplevel);
			 return 1;
		 } },
		{ "SetHitRectInsets", [](lua_State *L) -> int {
			 SELF;
			 w.hit_left = num(L, 2);
			 w.hit_right = num(L, 3);
			 w.hit_top = num(L, 4);
			 w.hit_bottom = num(L, 5);
			 return 0;
		 } },
		{ "GetHitRectInsets", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.hit_left);
			 lua_pushnumber(L, w.hit_right);
			 lua_pushnumber(L, w.hit_top);
			 lua_pushnumber(L, w.hit_bottom);
			 return 4;
		 } },
		{ "SetClampedToScreen", [](lua_State *L) -> int {
			 SELF;
			 w.clamped = flag(L, 2);
			 ui_->invalidate();
			 return 0;
		 } },
		{ "IsClampedToScreen", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.clamped);
			 return 1;
		 } },
		{ "GetFrameType", [](lua_State *L) -> int {
			 SELF;
			 push_string(L, w.type);
			 return 1;
		 } },
		{ "SetMinResize", [](lua_State *) -> int { return 0; } },
		{ "SetMaxResize", [](lua_State *) -> int { return 0; } },
		{ "EnableDrawLayer", [](lua_State *) -> int { return 0; } },
		{ "DisableDrawLayer", [](lua_State *) -> int { return 0; } },
		{ "GetTitleRegion", [](lua_State *) -> int { return 0; } },
		{ nullptr, nullptr },
	};

	static constexpr Method BUTTON[] = {
		{ "SetText", [](lua_State *L) -> int {
			 SELF;
			 ui_->set_text(id_, lua_isstring(L, 2) ? to_string(L, 2) : String());
			 return 0;
		 } },
		{ "GetText", [](lua_State *L) -> int {
			 SELF;
			 WowUI::Widget *text = ui_->widget(w.font_string);
			 if (!text || text->text.is_empty()) {
				 lua_pushnil(L);
			 } else {
				 push_string(L, text->text);
			 }
			 return 1;
		 } },
		{ "GetTextWidth", [](lua_State *L) -> int {
			 SELF;
			 WowUI::Widget *text = ui_->widget(w.font_string);
			 lua_pushnumber(L, text ? ui_->text_width(*text) : 0.0f);
			 return 1;
		 } },
		{ "GetTextHeight", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.font_string >= 0 ? ui_->text_height(w.font_string) : 0.0f);
			 return 1;
		 } },
		{ "GetFontString", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.font_string);
		 } },
		{ "SetFontString", [](lua_State *L) -> int {
			 SELF;
			 int text = ui_->widget_from(2);
			 if (text >= 0) {
				 w.font_string = text;
				 ui_->set_parent(text, id_);
			 }
			 return 0;
		 } },
		{ "SetTextFontObject", [](lua_State *L) -> int { return button_font(L, &WowUI::Widget::normal_font, &WowUI::Widget::has_normal_font); } },
		{ "SetHighlightFontObject", [](lua_State *L) -> int { return button_font(L, &WowUI::Widget::highlight_font, &WowUI::Widget::has_highlight_font); } },
		{ "SetDisabledFontObject", [](lua_State *L) -> int { return button_font(L, &WowUI::Widget::disabled_font, &WowUI::Widget::has_disabled_font); } },
		{ "SetTextColor", [](lua_State *L) -> int { return button_color(L, &WowUI::Widget::normal_font, &WowUI::Widget::has_normal_font); } },
		{ "SetHighlightTextColor", [](lua_State *L) -> int { return button_color(L, &WowUI::Widget::highlight_font, &WowUI::Widget::has_highlight_font); } },
		{ "SetDisabledTextColor", [](lua_State *L) -> int { return button_color(L, &WowUI::Widget::disabled_font, &WowUI::Widget::has_disabled_font); } },
		{ "GetTextColor", [](lua_State *L) -> int {
			 SELF;
			 WowUI::Widget *text = ui_->widget(w.font_string);
			 push_color(L, text ? text->font.color : Color(1, 1, 1, 1));
			 return 4;
		 } },
		{ "SetNormalTexture", [](lua_State *L) -> int { return button_slot(L, &WowUI::Widget::normal_texture, WowUI::LAYER_ARTWORK); } },
		{ "SetPushedTexture", [](lua_State *L) -> int { return button_slot(L, &WowUI::Widget::pushed_texture, WowUI::LAYER_ARTWORK); } },
		{ "SetDisabledTexture", [](lua_State *L) -> int { return button_slot(L, &WowUI::Widget::disabled_texture, WowUI::LAYER_ARTWORK); } },
		{ "SetHighlightTexture", [](lua_State *L) -> int { return button_slot(L, &WowUI::Widget::highlight_texture, WowUI::LAYER_HIGHLIGHT); } },
		{ "GetNormalTexture", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.normal_texture);
		 } },
		{ "GetPushedTexture", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.pushed_texture);
		 } },
		{ "GetDisabledTexture", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.disabled_texture);
		 } },
		{ "GetHighlightTexture", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.highlight_texture);
		 } },
		{ "LockHighlight", [](lua_State *L) -> int {
			 SELF;
			 w.locked_highlight = true;
			 ui_->apply_button_font(w);
			 return 0;
		 } },
		{ "UnlockHighlight", [](lua_State *L) -> int {
			 SELF;
			 w.locked_highlight = false;
			 ui_->apply_button_font(w);
			 return 0;
		 } },
		{ "Enable", [](lua_State *L) -> int {
			 SELF;
			 w.enabled = true;
			 ui_->apply_button_font(w);
			 return 0;
		 } },
		{ "Disable", [](lua_State *L) -> int {
			 SELF;
			 w.enabled = false;
			 w.pushed = false;
			 ui_->apply_button_font(w);
			 return 0;
		 } },
		{ "IsEnabled", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.enabled);
			 return 1;
		 } },
		{ "SetButtonState", [](lua_State *L) -> int {
			 SELF;
			 w.pushed = to_string(L, 2) == "PUSHED";
			 return 0;
		 } },
		{ "GetButtonState", [](lua_State *L) -> int {
			 SELF;
			 lua_pushstring(L, !w.enabled ? "DISABLED" : w.pushed ? "PUSHED" : "NORMAL");
			 return 1;
		 } },
		{ "RegisterForClicks", [](lua_State *L) -> int {
			 SELF;
			 mouse_bits(L, 2, w.click_up, w.click_down);
			 return 0;
		 } },
		{ "Click", [](lua_State *L) -> int {
			 SELF;
			 ui_->click_widget(id_, lua_isstring(L, 2) ? to_string(L, 2) : String("LeftButton"));
			 return 0;
		 } },
		{ "SetPushedTextOffset", [](lua_State *L) -> int {
			 SELF;
			 w.pushed_offset = Vector2(num(L, 2), num(L, 3));
			 return 0;
		 } },
		{ "GetPushedTextOffset", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.pushed_offset.x);
			 lua_pushnumber(L, w.pushed_offset.y);
			 return 2;
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method CHECK_BUTTON[] = {
		{ "SetChecked", [](lua_State *L) -> int {
			 SELF;
			 w.checked = flag(L, 2);
			 return 0;
		 } },
		{ "GetChecked", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.checked);
			 return 1;
		 } },
		{ "SetCheckedTexture", [](lua_State *L) -> int { return button_slot(L, &WowUI::Widget::checked_texture, WowUI::LAYER_OVERLAY); } },
		{ "SetDisabledCheckedTexture", [](lua_State *L) -> int { return button_slot(L, &WowUI::Widget::disabled_checked_texture, WowUI::LAYER_OVERLAY); } },
		{ "GetCheckedTexture", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.checked_texture);
		 } },
		{ "GetDisabledCheckedTexture", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.disabled_checked_texture);
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method VALUE[] = {
		{ "SetMinMaxValues", [](lua_State *L) -> int {
			 SELF;
			 w.min_value = num(L, 2);
			 w.max_value = num(L, 3);
			 ui_->set_value(id_, w.value, false);
			 ui_->invalidate();
			 return 0;
		 } },
		{ "GetMinMaxValues", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.min_value);
			 lua_pushnumber(L, w.max_value);
			 return 2;
		 } },
		{ "SetValue", [](lua_State *L) -> int {
			 SELF;
			 ui_->set_value(id_, num(L, 2), true);
			 return 0;
		 } },
		{ "GetValue", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.value);
			 return 1;
		 } },
		{ "SetOrientation", [](lua_State *L) -> int {
			 SELF;
			 w.vertical = to_string(L, 2) == "VERTICAL";
			 ui_->invalidate();
			 return 0;
		 } },
		{ "GetOrientation", [](lua_State *L) -> int {
			 SELF;
			 lua_pushstring(L, w.vertical ? "VERTICAL" : "HORIZONTAL");
			 return 1;
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method STATUS_BAR[] = {
		{ "SetStatusBarTexture", [](lua_State *L) -> int {
			 button_slot(L, &WowUI::Widget::bar_texture, lua_isstring(L, 3) ? layer_from(to_string(L, 3)) : WowUI::LAYER_ARTWORK);
			 SELF;
			 if (WowUI::Widget *bar = ui_->widget(w.bar_texture)) {
				 bar->bar_owner = id_;
				 bar->anchors.clear();
				 bar->vertex_color = w.bar_color;
			 }
			 ui_->invalidate();
			 return 0;
		 } },
		{ "GetStatusBarTexture", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.bar_texture);
		 } },
		{ "SetStatusBarColor", [](lua_State *L) -> int {
			 SELF;
			 w.bar_color = color_args(L, 2);
			 if (WowUI::Widget *bar = ui_->widget(w.bar_texture)) {
				 bar->vertex_color = w.bar_color;
			 }
			 return 0;
		 } },
		{ "GetStatusBarColor", [](lua_State *L) -> int {
			 SELF;
			 push_color(L, w.bar_color);
			 return 4;
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method SLIDER[] = {
		{ "SetValueStep", [](lua_State *L) -> int {
			 SELF;
			 w.value_step = num(L, 2);
			 return 0;
		 } },
		{ "GetValueStep", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.value_step);
			 return 1;
		 } },
		{ "SetThumbTexture", [](lua_State *L) -> int {
			 button_slot(L, &WowUI::Widget::bar_texture, WowUI::LAYER_OVERLAY);
			 SELF;
			 if (WowUI::Widget *thumb = ui_->widget(w.bar_texture)) {
				 thumb->bar_owner = id_;
				 thumb->anchors.clear();
			 }
			 ui_->invalidate();
			 return 0;
		 } },
		{ "GetThumbTexture", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.bar_texture);
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method EDIT_BOX[] = {
		{ "SetText", [](lua_State *L) -> int {
			 SELF;
			 ui_->edit_set_text(id_, lua_isstring(L, 2) ? to_string(L, 2) : String(), true);
			 return 0;
		 } },
		{ "GetText", [](lua_State *L) -> int {
			 SELF;
			 push_string(L, w.text);
			 return 1;
		 } },
		{ "Insert", [](lua_State *L) -> int {
			 SELF;
			 String insert = to_string(L, 2);
			 int at = std::clamp(w.cursor, 0, static_cast<int>(w.text.length()));
			 String text = w.select_all ? insert : w.text.substr(0, at) + insert + w.text.substr(at);
			 w.select_all = false;
			 ui_->edit_set_text(id_, text, true);
			 w.cursor = std::min(at + static_cast<int>(insert.length()), static_cast<int>(w.text.length()));
			 return 0;
		 } },
		{ "SetFocus", [](lua_State *L) -> int {
			 SELF;
			 ui_->focus_widget(id_, true);
			 return 0;
		 } },
		{ "ClearFocus", [](lua_State *L) -> int {
			 SELF;
			 ui_->focus_widget(id_, false);
			 return 0;
		 } },
		{ "HighlightText", [](lua_State *L) -> int {
			 SELF;
			 int from = static_cast<int>(num(L, 2, 0.0f));
			 int to = static_cast<int>(num(L, 3, -1.0f));
			 w.select_all = !w.text.is_empty() && from <= 0 && (to < 0 || to >= static_cast<int>(w.text.length()));
			 return 0;
		 } },
		{ "SetMaxLetters", [](lua_State *L) -> int {
			 SELF;
			 w.max_letters = static_cast<int>(num(L, 2));
			 return 0;
		 } },
		{ "GetMaxLetters", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.max_letters);
			 return 1;
		 } },
		{ "GetNumLetters", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, static_cast<double>(w.text.length()));
			 return 1;
		 } },
		{ "SetNumber", [](lua_State *L) -> int {
			 SELF;
			 ui_->edit_set_text(id_, String::num(num(L, 2)), true);
			 return 0;
		 } },
		{ "GetNumber", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.text.to_float());
			 return 1;
		 } },
		{ "SetNumeric", [](lua_State *L) -> int {
			 SELF;
			 w.numeric = flag(L, 2);
			 return 0;
		 } },
		{ "IsNumeric", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.numeric);
			 return 1;
		 } },
		{ "SetPassword", [](lua_State *L) -> int {
			 SELF;
			 w.password = flag(L, 2);
			 ui_->reset_layout(w);
			 return 0;
		 } },
		{ "IsPassword", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.password);
			 return 1;
		 } },
		{ "SetAutoFocus", [](lua_State *L) -> int {
			 SELF;
			 w.auto_focus = flag(L, 2);
			 return 0;
		 } },
		{ "IsAutoFocus", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.auto_focus);
			 return 1;
		 } },
		{ "SetMultiLine", [](lua_State *L) -> int {
			 SELF;
			 w.multi_line = flag(L, 2);
			 return 0;
		 } },
		{ "IsMultiLine", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.multi_line);
			 return 1;
		 } },
		{ "AddHistoryLine", [](lua_State *L) -> int {
			 SELF;
			 String line = to_string(L, 2);
			 if (!line.is_empty()) {
				 w.history.erase(std::remove(w.history.begin(), w.history.end(), line), w.history.end());
				 w.history.push_back(line);
				 int limit = w.history_lines > 0 ? w.history_lines : 32;
				 while (static_cast<int>(w.history.size()) > limit) {
					 w.history.erase(w.history.begin());
				 }
			 }
			 w.history_index = -1;
			 return 0;
		 } },
		{ "SetHistoryLines", [](lua_State *L) -> int {
			 SELF;
			 w.history_lines = static_cast<int>(num(L, 2));
			 return 0;
		 } },
		{ "GetHistoryLines", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.history_lines);
			 return 1;
		 } },
		{ "SetTextInsets", [](lua_State *L) -> int {
			 SELF;
			 w.text_left = num(L, 2);
			 w.text_right = num(L, 3);
			 w.text_top = num(L, 4);
			 w.text_bottom = num(L, 5);
			 return 0;
		 } },
		{ "GetTextInsets", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.text_left);
			 lua_pushnumber(L, w.text_right);
			 lua_pushnumber(L, w.text_top);
			 lua_pushnumber(L, w.text_bottom);
			 return 4;
		 } },
		{ "GetInputLanguage", [](lua_State *L) -> int {
			 lua_pushstring(L, "ROMAN");
			 return 1;
		 } },
		{ "ToggleInputLanguage", [](lua_State *) -> int { return 0; } },
		{ "SetBlinkSpeed", [](lua_State *) -> int { return 0; } },
		{ "SetAltArrowKeyMode", [](lua_State *L) -> int {
			 SELF;
			 w.ignore_arrows = flag(L, 2);
			 return 0;
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method SCROLL_FRAME[] = {
		{ "SetScrollChild", [](lua_State *L) -> int {
			 SELF;
			 int child = widget_arg(ui_, L, 2);
			 if (WowUI::Widget *old = ui_->widget(w.scroll_child)) {
				 old->scrolled_by = -1;
			 }
			 w.scroll_child = child;
			 if (WowUI::Widget *c = ui_->widget(child)) {
				 c->scrolled_by = id_;
				 ui_->set_parent(child, id_);
			 }
			 ui_->invalidate();
			 return 0;
		 } },
		{ "GetScrollChild", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.scroll_child);
		 } },
		{ "SetVerticalScroll", [](lua_State *L) -> int {
			 SELF;
			 w.scroll_v = num(L, 2);
			 ui_->invalidate();
			 return 0;
		 } },
		{ "SetHorizontalScroll", [](lua_State *L) -> int {
			 SELF;
			 w.scroll_h = num(L, 2);
			 ui_->invalidate();
			 return 0;
		 } },
		{ "GetVerticalScroll", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.scroll_v);
			 return 1;
		 } },
		{ "GetHorizontalScroll", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.scroll_h);
			 return 1;
		 } },
		{ "GetVerticalScrollRange", [](lua_State *L) -> int {
			 SELF;
			 ui_->update_scroll_child(id_);
			 lua_pushnumber(L, std::max(0.0f, w.range_v));
			 return 1;
		 } },
		{ "GetHorizontalScrollRange", [](lua_State *L) -> int {
			 SELF;
			 ui_->update_scroll_child(id_);
			 lua_pushnumber(L, std::max(0.0f, w.range_h));
			 return 1;
		 } },
		{ "UpdateScrollChildRect", [](lua_State *L) -> int {
			 SELF;
			 ui_->invalidate();
			 ui_->update_scroll_child(id_);
			 return 0;
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method MESSAGES[] = {
		{ "AddMessage", [](lua_State *L) -> int {
			 SELF;
			 Color color(num(L, 3, 1.0f), num(L, 4, 1.0f), num(L, 5, 1.0f), 1.0f);
			 ui_->message_add(id_, lua_isstring(L, 2) ? to_string(L, 2) : String(), color, static_cast<int>(num(L, 6)));
			 return 0;
		 } },
		{ "Clear", [](lua_State *L) -> int {
			 SELF;
			 w.messages.clear();
			 w.scroll_offset = 0;
			 return 0;
		 } },
		{ "ScrollUp", [](lua_State *L) -> int {
			 SELF;
			 w.scroll_offset = std::min(w.scroll_offset + 1, std::max(0, static_cast<int>(w.messages.size()) - 1));
			 return 0;
		 } },
		{ "ScrollDown", [](lua_State *L) -> int {
			 SELF;
			 w.scroll_offset = std::max(0, w.scroll_offset - 1);
			 return 0;
		 } },
		{ "PageUp", [](lua_State *L) -> int {
			 SELF;
			 w.scroll_offset = std::min(w.scroll_offset + visible_lines(ui_, w), std::max(0, static_cast<int>(w.messages.size()) - 1));
			 return 0;
		 } },
		{ "PageDown", [](lua_State *L) -> int {
			 SELF;
			 w.scroll_offset = std::max(0, w.scroll_offset - visible_lines(ui_, w));
			 return 0;
		 } },
		{ "ScrollToTop", [](lua_State *L) -> int {
			 SELF;
			 w.scroll_offset = std::max(0, static_cast<int>(w.messages.size()) - visible_lines(ui_, w));
			 return 0;
		 } },
		{ "ScrollToBottom", [](lua_State *L) -> int {
			 SELF;
			 w.scroll_offset = 0;
			 return 0;
		 } },
		{ "AtTop", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.scroll_offset >= static_cast<int>(w.messages.size()) - visible_lines(ui_, w));
			 return 1;
		 } },
		{ "AtBottom", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.scroll_offset == 0);
			 return 1;
		 } },
		{ "GetNumMessages", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, static_cast<double>(w.messages.size()));
			 return 1;
		 } },
		{ "GetNumLinesDisplayed", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, visible_lines(ui_, w));
			 return 1;
		 } },
		{ "GetCurrentScroll", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.scroll_offset);
			 return 1;
		 } },
		{ "SetScrollOffset", [](lua_State *L) -> int {
			 SELF;
			 w.scroll_offset = std::max(0, static_cast<int>(num(L, 2)));
			 return 0;
		 } },
		{ "SetMaxLines", [](lua_State *L) -> int {
			 SELF;
			 w.max_messages = std::max(1, static_cast<int>(num(L, 2, 128.0f)));
			 while (static_cast<int>(w.messages.size()) > w.max_messages) {
				 w.messages.pop_front();
			 }
			 return 0;
		 } },
		{ "GetMaxLines", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.max_messages);
			 return 1;
		 } },
		{ "SetFading", [](lua_State *L) -> int {
			 SELF;
			 w.fading = flag(L, 2);
			 return 0;
		 } },
		{ "GetFading", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.fading);
			 return 1;
		 } },
		{ "SetTimeVisible", [](lua_State *L) -> int {
			 SELF;
			 w.time_visible = num(L, 2, 10.0f);
			 return 0;
		 } },
		{ "GetTimeVisible", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.time_visible);
			 return 1;
		 } },
		{ "SetFadeDuration", [](lua_State *L) -> int {
			 SELF;
			 w.fade_duration = num(L, 2, 3.0f);
			 return 0;
		 } },
		{ "GetFadeDuration", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.fade_duration);
			 return 1;
		 } },
		{ "SetInsertMode", [](lua_State *L) -> int {
			 SELF;
			 w.insert_top = to_string(L, 2) == "TOP";
			 return 0;
		 } },
		{ "UpdateColorByID", [](lua_State *L) -> int {
			 SELF;
			 int message_id = static_cast<int>(num(L, 2));
			 Color color(num(L, 3, 1.0f), num(L, 4, 1.0f), num(L, 5, 1.0f), 1.0f);
			 for (WowUI::Message &m : w.messages) {
				 if (m.id == message_id && message_id != 0) {
					 m.color = color;
					 m.layout.key = String();
				 }
			 }
			 return 0;
		 } },
		{ "SetScrollFromBottom", [](lua_State *) -> int { return 0; } },
		{ nullptr, nullptr },
	};

	static constexpr Method TOOLTIP[] = {
		{ "SetOwner", [](lua_State *L) -> int {
			 SELF;
			 w.owner = widget_arg(ui_, L, 2);
			 w.owner_anchor = lua_isstring(L, 3) ? to_string(L, 3) : String("ANCHOR_LEFT");
			 w.owner_offset = Vector2(num(L, 4), num(L, 5));
			 w.min_width = 0.0f;
			 ui_->tooltip_clear(id_);
			 ui_->set_shown(id_, false);
			 ui_->anchor_tooltip(w);
			 return 0;
		 } },
		{ "IsOwned", [](lua_State *L) -> int {
			 SELF;
			 push_flag(L, w.owner >= 0 && w.owner == widget_arg(ui_, L, 2));
			 return 1;
		 } },
		{ "GetOwner", [](lua_State *L) -> int {
			 SELF;
			 return push_slot(L, w.owner);
		 } },
		{ "AddLine", [](lua_State *L) -> int {
			 SELF;
			 Color color(num(L, 3, 1.0f), num(L, 4, 0.82f), num(L, 5, 0.0f), 1.0f);
			 ui_->tooltip_add_line(id_, lua_isstring(L, 2) ? to_string(L, 2) : String(), String(), color, color, flag(L, 6));
			 return 0;
		 } },
		{ "AddDoubleLine", [](lua_State *L) -> int {
			 SELF;
			 Color left(num(L, 4, 1.0f), num(L, 5, 0.82f), num(L, 6, 0.0f), 1.0f);
			 Color right(num(L, 7, 1.0f), num(L, 8, 0.82f), num(L, 9, 0.0f), 1.0f);
			 ui_->tooltip_add_line(id_, lua_isstring(L, 2) ? to_string(L, 2) : String(), lua_isstring(L, 3) ? to_string(L, 3) : String(), left, right);
			 return 0;
		 } },
		{ "SetText", [](lua_State *L) -> int {
			 SELF;
			 ui_->tooltip_clear(id_);
			 Color color(num(L, 3, 1.0f), num(L, 4, 0.82f), num(L, 5, 0.0f), 1.0f);
			 ui_->tooltip_add_line(id_, lua_isstring(L, 2) ? to_string(L, 2) : String(), String(), color, color, flag(L, 7));
			 ui_->tooltip_show(id_);
			 return 0;
		 } },
		{ "AppendText", [](lua_State *L) -> int {
			 SELF;
			 if (WowUI::Widget *first = ui_->widget(ui_->find(w.name + "TextLeft1"))) {
				 ui_->set_text(first->id, first->text + to_string(L, 2));
				 ui_->tooltip_show(id_);
			 }
			 return 0;
		 } },
		{ "ClearLines", [](lua_State *L) -> int {
			 SELF;
			 ui_->tooltip_clear(id_);
			 return 0;
		 } },
		{ "NumLines", [](lua_State *L) -> int {
			 SELF;
			 lua_pushnumber(L, w.line_count);
			 return 1;
		 } },
		{ "SetMinimumWidth", [](lua_State *L) -> int {
			 SELF;
			 w.min_width = num(L, 2);
			 return 0;
		 } },
		{ "FadeOut", [](lua_State *L) -> int {
			 SELF;
			 ui_->set_shown(id_, false);
			 return 0;
		 } },
		{ nullptr, nullptr },
	};

	// Tooltip contents come from the game: the host's WowdotTooltip(tooltip, method, ...) returns its lines.
	static int tooltip_query(lua_State *L) {
		SELF;
		const char *method = lua_tostring(L, lua_upvalueindex(1));
		int args = lua_gettop(L);
		ui_->tooltip_clear(id_);
		lua_getglobal(L, "WowdotTooltip");
		if (!lua_isfunction(L, -1)) {
			lua_pop(L, 1);
			ui_->note_missing(std::string("GameTooltip:") + method);
			return 0;
		}
		lua_pushvalue(L, 1);
		lua_pushstring(L, method);
		for (int i = 2; i <= args; ++i) {
			lua_pushvalue(L, i);
		}
		lua_call(L, args + 1, 1);
		int lines = lua_gettop(L);
		bool any = false;
		if (lua_istable(L, lines)) {
			int count = static_cast<int>(lua_objlen(L, lines));
			for (int i = 1; i <= count; ++i) {
				lua_rawgeti(L, lines, i);
				int line = lua_gettop(L);
				if (lua_istable(L, line)) {
					lua_rawgeti(L, line, 1);
					String left = lua_isstring(L, -1) ? to_string(L, -1) : String();
					lua_rawgeti(L, line, 2);
					lua_rawgeti(L, line, 3);
					lua_rawgeti(L, line, 4);
					Color left_color(num(L, -3, 1.0f), num(L, -2, 1.0f), num(L, -1, 1.0f), 1.0f);
					lua_rawgeti(L, line, 5);
					String right = lua_isstring(L, -1) ? to_string(L, -1) : String();
					lua_rawgeti(L, line, 6);
					lua_rawgeti(L, line, 7);
					lua_rawgeti(L, line, 8);
					Color right_color(num(L, -3, 1.0f), num(L, -2, 1.0f), num(L, -1, 1.0f), 1.0f);
					ui_->tooltip_add_line(id_, left, right, left_color, right_color);
					any = true;
				}
				lua_settop(L, lines);
			}
		}
		if (any) {
			ui_->tooltip_show(id_);
		} else {
			ui_->set_shown(id_, false);
		}
		push_flag(L, any);
		return 1;
	}

	static constexpr const char *TOOLTIP_QUERIES[] = { "SetUnit", "SetAction", "SetBagItem", "SetInventoryItem", "SetSpell", "SetHyperlink", "SetLootItem", "SetQuestItem", "SetQuestLogItem", "SetMerchantItem", "SetTrainerService", "SetPetAction", "SetShapeshift", "SetTalent", "SetTradePlayerItem", "SetTradeTargetItem", "SetCraftItem", "SetCraftSpell", "SetTradeSkillItem", "SetAuctionItem", "SetAuctionSellItem", "SetInboxItem", "SetSendMailItem", "SetLootRollItem", "SetPlayerBuff", "SetUnitBuff", "SetUnitDebuff", "SetTrackingSpell", "SetBuybackItem", "SetQuestRewardSpell", "SetQuestLogRewardSpell", "SetTracking" };

	// Model methods go to the host, which draws the scene; getters answer what was last set.
	static int model_method(lua_State *L) {
		SELF;
		String method = to_string(L, lua_upvalueindex(1));
		Array args;
		for (int i = 2; i <= lua_gettop(L); ++i) {
			args.push_back(to_variant(L, i));
		}
		if (method.begins_with("Get")) {
			String key = method.substr(3);
			if (w.state.has(key)) {
				Array values = w.state[key];
				for (int64_t i = 0; i < values.size(); ++i) {
					push_variant(L, values[i]);
				}
				return static_cast<int>(values.size());
			}
			if (key == "Facing" || key == "Zoom") {
				lua_pushnumber(L, 0);
				return 1;
			}
			if (key == "ModelScale") {
				lua_pushnumber(L, 1);
				return 1;
			}
			if (key == "Position" || key == "PingPosition") {
				lua_pushnumber(L, 0);
				lua_pushnumber(L, 0);
				lua_pushnumber(L, 0);
				return key == "Position" ? 3 : 2;
			}
			if (key == "ZoomLevels") {
				lua_pushnumber(L, 6);
				return 1;
			}
			return 0;
		}
		if (method.begins_with("Set")) {
			w.state[method.substr(3)] = args;
		}
		if (method == "SetModel") {
			w.cooldown_model = args.size() > 0 && String(args[0]).to_lower().contains("cooldown");
		}
		if (w.cooldown_model) {
			if (method == "SetSequenceTime" && args.size() >= 2) {
				w.cooldown_progress = static_cast<float>(args[0]) == 0.0f ? static_cast<float>(args[1]) / 1000.0f : 1.0f;
			} else if (method == "SetSequence" && args.size() >= 1) {
				w.cooldown_finishing = static_cast<int>(args[0]) == 1;
				if (!w.cooldown_finishing) {
					w.cooldown_progress = 0.0f;
				}
			}
			return 0;
		}
		ui_->model_call(id_, method, args);
		return 0;
	}

	static constexpr const char *MODEL_METHODS[] = { "SetModel", "GetModel", "SetUnit", "SetCamera", "SetSequence", "SetSequenceTime", "SetPosition", "GetPosition", "SetFacing", "GetFacing", "SetLight", "SetFogColor", "GetFogColor", "SetFogNear", "GetFogNear", "SetFogFar", "GetFogFar", "ClearFog", "SetRotation", "RefreshUnit", "SetModelScale", "GetModelScale", "ClearModel", "SetCreature", "Dress", "Undress", "TryOn", "AdvanceTime", "ReplaceIconTexture", "SetGlow", "SetDisplayInfo", "InitializeTabardColors", "CanSaveTabardNow", "CycleVariation", "GetUpperBackgroundFileName", "GetLowerBackgroundFileName", "GetUpperEmblemFileName", "GetLowerEmblemFileName", "Save" };
	static constexpr const char *MINIMAP_METHODS[] = { "SetZoom", "GetZoom", "GetZoomLevels", "SetMaskTexture", "SetIconTexture", "SetBlipTexture", "SetPlayerModel", "SetArrowModel", "PingLocation", "GetPingPosition", "SetPlayerTexture", "SetPlayerTextureHeight", "SetPlayerTextureWidth" };

	static constexpr Method COLOR_SELECT[] = {
		{ "SetColorRGB", [](lua_State *L) -> int {
			 SELF;
			 w.state["color"] = Color(num(L, 2), num(L, 3), num(L, 4));
			 return 0;
		 } },
		{ "GetColorRGB", [](lua_State *L) -> int {
			 SELF;
			 Color c = w.state.get("color", Color(1, 1, 1));
			 lua_pushnumber(L, c.r);
			 lua_pushnumber(L, c.g);
			 lua_pushnumber(L, c.b);
			 return 3;
		 } },
		{ "SetColorHSV", [](lua_State *L) -> int {
			 SELF;
			 w.state["color"] = Color::from_hsv(num(L, 2) / 360.0f, num(L, 3), num(L, 4));
			 return 0;
		 } },
		{ "GetColorHSV", [](lua_State *L) -> int {
			 SELF;
			 Color c = w.state.get("color", Color(1, 1, 1));
			 lua_pushnumber(L, c.get_h() * 360.0f);
			 lua_pushnumber(L, c.get_s());
			 lua_pushnumber(L, c.get_v());
			 return 3;
		 } },
		{ nullptr, nullptr },
	};

	static constexpr Method SIMPLE_HTML[] = {
		{ "SetText", [](lua_State *L) -> int {
			 SELF;
			 ui_->set_text(id_, lua_isstring(L, 2) ? to_string(L, 2) : String());
			 return 0;
		 } },
		{ "SetHyperlinkFormat", [](lua_State *) -> int { return 0; } },
		{ nullptr, nullptr },
	};

	static constexpr Method MOVIE[] = {
		{ "StartMovie", [](lua_State *) -> int { return 0; } },
		{ "StopMovie", [](lua_State *) -> int { return 0; } },
		{ "EnableSubtitles", [](lua_State *) -> int { return 0; } },
		{ nullptr, nullptr },
	};

	static constexpr Method COOLDOWN[] = {
		{ "SetCooldown", [](lua_State *L) -> int {
			 SELF;
			 w.state["start"] = num(L, 2);
			 w.state["duration"] = num(L, 3);
			 return 0;
		 } },
		{ nullptr, nullptr },
	};

	// Method tables.

	static void add_methods(lua_State *L, int table, const Method *methods) {
		for (const Method *m = methods; m->name; ++m) {
			lua_pushcfunction(L, m->fn);
			lua_setfield(L, table, m->name);
		}
	}

	static void add_closures(lua_State *L, int table, const char *const *names, size_t count, lua_CFunction fn) {
		for (size_t i = 0; i < count; ++i) {
			lua_pushstring(L, names[i]);
			lua_pushcclosure(L, fn, 1);
			lua_setfield(L, table, names[i]);
		}
	}

	// Makes the method table of a type from its base's and its own.
	static int make_type(lua_State *L, const char *base, std::initializer_list<const Method *> sets) {
		lua_newtable(L);
		int table = lua_gettop(L);
		if (base) {
			lua_getfield(L, LUA_REGISTRYINDEX, (std::string("wowui.methods.") + base).c_str());
			lua_pushnil(L);
			while (lua_next(L, -2)) {
				lua_pushvalue(L, -2);
				lua_insert(L, -2);
				lua_rawset(L, table);
			}
			lua_pop(L, 1);
		}
		for (const Method *set : sets) {
			add_methods(L, table, set);
		}
		return table;
	}

	static void finish_type(lua_State *L, int table, const char *type) {
		lua_newtable(L);
		lua_pushcfunction(L, method_index);
		lua_setfield(L, -2, "__index");
		lua_setmetatable(L, table);
		lua_pushvalue(L, table);
		lua_setfield(L, LUA_REGISTRYINDEX, (std::string("wowui.methods.") + type).c_str());
		lua_newtable(L);
		lua_pushvalue(L, table);
		lua_setfield(L, -2, "__index");
		lua_setfield(L, LUA_REGISTRYINDEX, (std::string("wowui.meta.") + type).c_str());
		lua_settop(L, table - 1);
	}

	static void define_type(lua_State *L, const char *name, const char *base, std::initializer_list<const Method *> sets) {
		finish_type(L, make_type(L, base, sets), name);
	}

	static void register_methods(lua_State *L) {
		define_type(L, "UIObject", nullptr, { UIOBJECT });
		define_type(L, "Region", "UIObject", { REGION });
		define_type(L, "LayeredRegion", "Region", { LAYERED });
		define_type(L, "Texture", "LayeredRegion", { TEXTURE });
		define_type(L, "FontString", "LayeredRegion", { FONT_INSTANCE, FONT_STRING });
		define_type(L, "Font", "UIObject", { FONT_INSTANCE });
		define_type(L, "Frame", "Region", { FRAME });
		// The button's own SetText and text color calls win over the font instance's.
		define_type(L, "Button", "Frame", { FONT_INSTANCE, BUTTON });
		define_type(L, "CheckButton", "Button", { CHECK_BUTTON });
		define_type(L, "LootButton", "Button", {});
		define_type(L, "EditBox", "Frame", { FONT_INSTANCE, EDIT_BOX });
		define_type(L, "ScrollFrame", "Frame", { SCROLL_FRAME });
		define_type(L, "ScrollingMessageFrame", "Frame", { FONT_INSTANCE, MESSAGES });
		define_type(L, "MessageFrame", "Frame", { FONT_INSTANCE, MESSAGES });
		define_type(L, "Slider", "Frame", { VALUE, SLIDER });
		define_type(L, "StatusBar", "Frame", { VALUE, STATUS_BAR });
		define_type(L, "ColorSelect", "Frame", { COLOR_SELECT });
		define_type(L, "Cooldown", "Frame", { COOLDOWN });
		define_type(L, "MovieFrame", "Frame", { MOVIE });
		define_type(L, "WorldFrame", "Frame", {});
		define_type(L, "TaxiRouteFrame", "Frame", {});
		define_type(L, "SimpleHTML", "Frame", { FONT_INSTANCE, SIMPLE_HTML });
		{
			int table = make_type(L, "Frame", { TOOLTIP });
			add_closures(L, table, TOOLTIP_QUERIES, std::size(TOOLTIP_QUERIES), tooltip_query);
			finish_type(L, table, "GameTooltip");
		}
		{
			int table = make_type(L, "Frame", {});
			add_closures(L, table, MODEL_METHODS, std::size(MODEL_METHODS), model_method);
			finish_type(L, table, "Model");
		}
		define_type(L, "PlayerModel", "Model", {});
		define_type(L, "DressUpModel", "PlayerModel", {});
		define_type(L, "TabardModel", "PlayerModel", {});
		define_type(L, "ModelFFX", "Model", {});
		{
			int table = make_type(L, "Frame", {});
			add_closures(L, table, MINIMAP_METHODS, std::size(MINIMAP_METHODS), model_method);
			finish_type(L, table, "Minimap");
		}
	}

	// Globals.

	static int print(lua_State *L) {
		String line;
		for (int i = 1; i <= lua_gettop(L); ++i) {
			lua_getglobal(L, "tostring");
			lua_pushvalue(L, i);
			lua_call(L, 1, 1);
			line += (i > 1 ? "\t" : "") + to_string(L, -1);
			lua_pop(L, 1);
		}
		UtilityFunctions::print("[UI] ", line);
		return 0;
	}

	static int message(lua_State *L) {
		ui(L)->report(to_string(L, 1));
		return 0;
	}

	static int create_frame(lua_State *L) {
		WowUI *self = ui(L);
		WowUI::Kind kind;
		String type;
		if (!WowUI::kind_for_type(luaL_checkstring(L, 1), kind, type) || kind == WowUI::KIND_TEXTURE || kind == WowUI::KIND_FONT_STRING || kind == WowUI::KIND_FONT) {
			return luaL_error(L, "CreateFrame: unknown frame type '%s'", lua_tostring(L, 1));
		}
		int parent = lua_isnoneornil(L, 3) ? -1 : widget_arg(self, L, 3);
		String name = lua_isstring(L, 2) ? self->resolve_name(to_string(L, 2), parent) : String();
		std::string inherits = lua_isstring(L, 4) ? lua_tostring(L, 4) : "";
		int id = self->method_create(kind, type, name, parent, inherits, WowUI::LAYER_ARTWORK);
		self->push_widget(id);
		return 1;
	}

	static int get_time(lua_State *L) {
		WowUI *self = ui(L);
		lua_pushnumber(L, (static_cast<double>(Time::get_singleton()->get_ticks_usec()) - self->start_usec) / 1000000.0);
		return 1;
	}

	static int get_cvar(lua_State *L) {
		WowUI *self = ui(L);
		String name = to_string(L, 1);
		if (!self->cvars.has(name)) {
			return 0;
		}
		push_string(L, String(self->cvars[name]));
		return 1;
	}

	static int set_cvar(lua_State *L) {
		WowUI *self = ui(L);
		lua_getglobal(L, "tostring");
		lua_pushvalue(L, 2);
		lua_call(L, 1, 1);
		self->cvar_set(to_string(L, 1), to_string(L, -1));
		return 0;
	}

	static int register_cvar(lua_State *L) {
		WowUI *self = ui(L);
		String name = to_string(L, 1);
		if (!self->cvars.has(name)) {
			self->cvars[name] = lua_isstring(L, 2) ? to_string(L, 2) : String();
		}
		return 0;
	}

	static int cursor_position(lua_State *L) {
		Vector2 at = ui(L)->cursor_position();
		lua_pushnumber(L, at.x);
		lua_pushnumber(L, at.y);
		return 2;
	}

	static int screen_width(lua_State *L) {
		lua_pushnumber(L, ui(L)->get_screen_width());
		return 1;
	}

	static int screen_height(lua_State *L) {
		lua_pushnumber(L, WowUI::SCREEN_HEIGHT);
		return 1;
	}

	static int key_down(lua_State *L, Key a, Key b) {
		Input *input = Input::get_singleton();
		push_flag(L, input->is_key_pressed(a) || input->is_key_pressed(b));
		return 1;
	}

	static int mouse_focus(lua_State *L) {
		return push_slot(L, ui(L)->hovered_widget());
	}

	static int framerate(lua_State *L) {
		lua_pushnumber(L, Engine::get_singleton()->get_frames_per_second());
		return 1;
	}

	static int run_script(lua_State *L) {
		ui(L)->run_chunk(luaL_checkstring(L, 1), "=RunScript");
		return 0;
	}

	static int load_addon(lua_State *L) {
		WowUI *self = ui(L);
		String name = to_string(L, 1);
		if (self->load_addon(name)) {
			lua_pushnumber(L, 1);
			return 1;
		}
		lua_pushnil(L);
		lua_pushstring(L, "MISSING");
		return 2;
	}

	static int addon_loaded(lua_State *L) {
		push_flag(L, ui(L)->is_addon_loaded(to_string(L, 1)));
		return 1;
	}

	static String addon_at(WowUI *self, lua_State *L, int index) {
		if (lua_type(L, index) == LUA_TNUMBER) {
			PackedStringArray all = self->get_addons();
			int i = static_cast<int>(lua_tonumber(L, index)) - 1;
			return i >= 0 && i < all.size() ? all[i] : String();
		}
		return to_string(L, index);
	}

	static int num_addons(lua_State *L) {
		lua_pushnumber(L, ui(L)->get_addons().size());
		return 1;
	}

	static int addon_info(lua_State *L) {
		WowUI *self = ui(L);
		String name = addon_at(self, L, 1);
		Dictionary info = self->get_addon_info(name);
		if (info.is_empty()) {
			return 0;
		}
		push_string(L, name);
		push_string(L, info.get("Title", name));
		push_string(L, info.get("Notes", String()));
		lua_pushnumber(L, 1); // enabled
		lua_pushnumber(L, 1); // loadable
		lua_pushnil(L); // reason
		lua_pushstring(L, "INSECURE");
		return 7;
	}

	static int addon_metadata(lua_State *L) {
		WowUI *self = ui(L);
		Dictionary info = self->get_addon_info(addon_at(self, L, 1));
		String field = to_string(L, 2);
		if (!info.has(field)) {
			return 0;
		}
		push_string(L, info[field]);
		return 1;
	}

	static int addon_on_demand(lua_State *L) {
		WowUI *self = ui(L);
		Dictionary info = self->get_addon_info(addon_at(self, L, 1));
		push_flag(L, String(info.get("LoadOnDemand", "0")) == "1");
		return 1;
	}

	static int binding_key(lua_State *L) {
		WowUI *self = ui(L);
		String action = to_string(L, 1);
		Array keys = self->key_bindings.keys();
		int count = 0;
		for (int64_t i = 0; i < keys.size(); ++i) {
			if (String(self->key_bindings[keys[i]]) == action) {
				push_string(L, keys[i]);
				++count;
			}
		}
		return count;
	}

	static int binding_action(lua_State *L) {
		WowUI *self = ui(L);
		push_string(L, String(self->key_bindings.get(to_string(L, 1), String())));
		return 1;
	}

	static int set_binding(lua_State *L) {
		WowUI *self = ui(L);
		String key = to_string(L, 1);
		if (lua_isstring(L, 2) && lua_objlen(L, 2) > 0) {
			self->key_bindings[key] = to_string(L, 2);
		} else {
			self->key_bindings.erase(key);
		}
		lua_pushnumber(L, 1);
		return 1;
	}

	static int num_bindings(lua_State *L) {
		lua_pushnumber(L, static_cast<double>(ui(L)->binding_order.size()));
		return 1;
	}

	static int get_binding(lua_State *L) {
		WowUI *self = ui(L);
		int index = static_cast<int>(num(L, 1)) - 1;
		if (index < 0 || index >= static_cast<int>(self->binding_order.size())) {
			return 0;
		}
		const std::string &name = self->binding_order[index];
		lua_pushstring(L, name.c_str());
		String action = String::utf8(name.c_str());
		Array keys = self->key_bindings.keys();
		int count = 1;
		for (int64_t i = 0; i < keys.size() && count < 3; ++i) {
			if (String(self->key_bindings[keys[i]]) == action) {
				push_string(L, keys[i]);
				++count;
			}
		}
		return count;
	}

	static int noop(lua_State *) {
		return 0;
	}

	static void register_core(WowUI *self, lua_State *L) {
		const Method core[] = {
			{ "print", print },
			{ "message", message },
			{ "CreateFrame", create_frame },
			{ "GetTime", get_time },
			{ "GetCVar", get_cvar },
			{ "SetCVar", set_cvar },
			{ "RegisterCVar", register_cvar },
			{ "GetCursorPosition", cursor_position },
			{ "GetScreenWidth", screen_width },
			{ "GetScreenHeight", screen_height },
			{ "IsShiftKeyDown", [](lua_State *state) -> int { return key_down(state, KEY_SHIFT, KEY_SHIFT); } },
			{ "IsControlKeyDown", [](lua_State *state) -> int { return key_down(state, KEY_CTRL, KEY_META); } },
			{ "IsAltKeyDown", [](lua_State *state) -> int { return key_down(state, KEY_ALT, KEY_ALT); } },
			{ "GetMouseFocus", mouse_focus },
			{ "GetFramerate", framerate },
			{ "RunScript", run_script },
			{ "LoadAddOn", load_addon },
			{ "IsAddOnLoaded", addon_loaded },
			{ "GetNumAddOns", num_addons },
			{ "GetAddOnInfo", addon_info },
			{ "GetAddOnMetadata", addon_metadata },
			{ "IsAddOnLoadOnDemand", addon_on_demand },
			{ "EnableAddOn", noop },
			{ "DisableAddOn", noop },
			{ "GetBindingKey", binding_key },
			{ "GetBindingAction", binding_action },
			{ "SetBinding", set_binding },
			{ "GetNumBindings", num_bindings },
			{ "GetBinding", get_binding },
			{ "SaveBindings", noop },
			{ "LoadBindings", noop },
			{ "GetCurrentBindingSet", [](lua_State *state) -> int {
				 lua_pushnumber(state, 1);
				 return 1;
			 } },
			{ nullptr, nullptr },
		};
		for (const Method *m = core; m->name; ++m) {
			lua_register(L, m->name, m->fn);
		}
		self->run_chunk(PRELUDE, "=prelude");
		lua_pushvalue(L, LUA_GLOBALSINDEX);
		lua_newtable(L);
		lua_pushcfunction(L, global_index);
		lua_setfield(L, -2, "__index");
		lua_setmetatable(L, -2);
		lua_pop(L, 1);
	}
};

// Lua state.

void WowUI::open_lua() {
	L = luaL_newstate();
	const luaL_Reg libs[] = {
		{ "", luaopen_base },
		{ LUA_TABLIBNAME, luaopen_table },
		{ LUA_STRLIBNAME, luaopen_string },
		{ LUA_MATHLIBNAME, luaopen_math },
		{ LUA_OSLIBNAME, luaopen_os },
		{ LUA_DBLIBNAME, luaopen_debug },
	};
	for (const luaL_Reg &lib : libs) {
		lua_pushcfunction(L, lib.func);
		lua_pushstring(L, lib.name);
		lua_call(L, 1, 0);
	}
	lua_pushlightuserdata(L, this);
	lua_setfield(L, LUA_REGISTRYINDEX, "wowui");
	lua_pushcfunction(L, traceback);
	lua_setfield(L, LUA_REGISTRYINDEX, "wowui.traceback");
	register_methods();
	register_core();
}

void WowUI::close_lua() {
	if (L) {
		lua_close(L);
		L = nullptr;
	}
}

void WowUI::register_core() {
	WowUILua::register_core(this, L);
}

void WowUI::register_methods() {
	WowUILua::register_methods(L);
}

bool WowUI::run_chunk(const std::string &code, const std::string &chunk_name) {
	int base = lua_gettop(L);
	lua_getfield(L, LUA_REGISTRYINDEX, "wowui.traceback");
	if (luaL_loadbuffer(L, code.data(), code.size(), chunk_name.c_str()) != 0 || lua_pcall(L, 0, 0, base + 1) != 0) {
		report(wowui::to_string(L, -1));
		lua_settop(L, base);
		return false;
	}
	lua_settop(L, base);
	return true;
}

int WowUI::compile(const std::string &code, const std::string &chunk_name) {
	if (luaL_loadbuffer(L, code.data(), code.size(), chunk_name.c_str()) != 0) {
		report(wowui::to_string(L, -1));
		lua_pop(L, 1);
		return LUA_NOREF;
	}
	return luaL_ref(L, LUA_REGISTRYINDEX);
}

void WowUI::push_widget(int id) {
	Widget &w = *widgets[id];
	if (w.lua_ref != LUA_NOREF) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, w.lua_ref);
		return;
	}
	lua_createtable(L, 0, 1);
	lua_pushlightuserdata(L, reinterpret_cast<void *>(static_cast<intptr_t>(id + 1)));
	lua_rawseti(L, -2, 0);
	std::string meta = "wowui.meta." + std_str(w.type);
	lua_getfield(L, LUA_REGISTRYINDEX, meta.c_str());
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_getfield(L, LUA_REGISTRYINDEX, "wowui.meta.Frame");
	}
	lua_setmetatable(L, -2);
	lua_pushvalue(L, -1);
	w.lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

namespace {

const char *ARG_NAMES[] = { "arg1", "arg2", "arg3", "arg4", "arg5", "arg6", "arg7", "arg8", "arg9" };

} // namespace

void WowUI::call_script(int id, const char *handler, int arg_count, const Variant *args, const char *event) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	auto found = w->scripts.find(handler);
	if (found == w->scripts.end()) {
		return;
	}
	int ref = found->second;
	int base = lua_gettop(L);
	lua_checkstack(L, 32);
	// The interface's handlers read this, event and arg1 to arg9 as globals; keep the caller's.
	lua_getglobal(L, "this");
	lua_getglobal(L, "event");
	for (const char *name : ARG_NAMES) {
		lua_getglobal(L, name);
	}
	push_widget(id);
	lua_setglobal(L, "this");
	if (event) {
		lua_pushstring(L, event);
		lua_setglobal(L, "event");
	}
	for (int i = 0; i < 9; ++i) {
		if (i < arg_count) {
			push_variant(L, args[i]);
		} else {
			lua_pushnil(L);
		}
		lua_setglobal(L, ARG_NAMES[i]);
	}
	lua_getfield(L, LUA_REGISTRYINDEX, "wowui.traceback");
	int handler_index = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	if (lua_pcall(L, 0, 0, handler_index) != 0) {
		String owner = w->name.is_empty() ? w->type : w->name;
		report(owner + ":" + handler + " " + wowui::to_string(L, -1));
	}
	lua_settop(L, base + 11);
	for (int i = 8; i >= 0; --i) {
		lua_setglobal(L, ARG_NAMES[i]);
	}
	lua_setglobal(L, "event");
	lua_setglobal(L, "this");
	lua_settop(L, base);
}

bool WowUI::has_script(int id, const char *handler) const {
	const Widget *w = widget(id);
	return w && w->scripts.count(handler) > 0;
}

void WowUI::set_script(int id, const std::string &handler, int ref) {
	Widget &w = *widgets[id];
	auto found = w.scripts.find(handler);
	if (found != w.scripts.end()) {
		luaL_unref(L, LUA_REGISTRYINDEX, found->second);
		w.scripts.erase(found);
	}
	if (ref != LUA_NOREF && ref != LUA_REFNIL) {
		w.scripts[handler] = ref;
	}
}

// Methods shared with the builder.

void WowUI::method_set_point(int id, int base) {
	Widget &w = *widgets[id];
	Anchor a;
	a.point = point_from(wowui::to_string(L, base));
	a.relative_point = a.point;
	int top = lua_gettop(L);
	int i = base + 1;
	if (i <= top && !lua_isnumber(L, i)) {
		if (lua_istable(L, i)) {
			int relative = widget_from(i);
			a.relative = relative < 0 || relative == w.parent ? -1 : relative;
		} else if (lua_type(L, i) == LUA_TSTRING) {
			a.relative_name = resolve_name(wowui::to_string(L, i), w.parent);
			a.relative = find(a.relative_name);
			if (a.relative < 0) {
				a.relative = -3;
			} else if (a.relative == w.parent) {
				a.relative = -1;
			}
		}
		++i;
		if (i <= top && lua_type(L, i) == LUA_TSTRING) {
			a.relative_point = point_from(wowui::to_string(L, i));
			++i;
		}
	}
	if (i <= top && lua_isnumber(L, i)) {
		a.x = static_cast<float>(lua_tonumber(L, i));
		a.y = static_cast<float>(lua_tonumber(L, i + 1));
	}
	if (a.relative == id) {
		return;
	}
	bool replaced = false;
	for (Anchor &existing : w.anchors) {
		if (existing.point == a.point) {
			existing = a;
			replaced = true;
		}
	}
	if (!replaced) {
		w.anchors.push_back(a);
	}
	invalidate();
}

namespace {

String strip_html(const String &html) {
	String out;
	bool in_tag = false;
	String tag;
	for (int64_t i = 0; i < html.length(); ++i) {
		char32_t c = html[i];
		if (c == '<') {
			in_tag = true;
			tag = String();
		} else if (c == '>' && in_tag) {
			in_tag = false;
			String name = tag.strip_edges().to_lower();
			if (name.begins_with("br") || name.begins_with("/p") || name.begins_with("/h")) {
				out += "\n";
			}
		} else if (in_tag) {
			tag += String::chr(c);
		} else {
			out += String::chr(c);
		}
	}
	return out.replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&").replace("&quot;", "\"").strip_edges();
}

} // namespace

void WowUI::set_text(int id, const String &text) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	switch (w->kind) {
		case KIND_BUTTON:
		case KIND_CHECK_BUTTON: {
			if (w->font_string < 0 && text.is_empty()) {
				return;
			}
			int fs = WowUILua::ensure_text(this, id);
			set_text(fs, text);
			return;
		}
		case KIND_EDIT_BOX:
			edit_set_text(id, text, true);
			return;
		case KIND_SIMPLE_HTML:
			w->text = text.contains("<") ? strip_html(text) : text;
			break;
		case KIND_GAME_TOOLTIP:
			tooltip_clear(id);
			tooltip_add_line(id, text, String(), Color(1, 1, 1), Color(1, 1, 1));
			tooltip_show(id);
			return;
		default:
			if (w->text == text) {
				return;
			}
			w->text = text;
			break;
	}
	reset_layout(*w);
	invalidate();
}

void WowUI::set_texture(int id, const String &file) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	w->solid = false;
	w->texture_file = file;
	w->texture = file.is_empty() ? Ref<Texture2D>() : load_texture(file);
}

void WowUI::set_font_string_font(int id, const FontInfo &font) {
	if (Widget *w = widget(id)) {
		w->font = font;
		reset_layout(*w);
		invalidate();
	}
}

void WowUI::tooltip_clear(int id) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	for (int i = 1; i <= std::max(w->line_count, 1); ++i) {
		for (const char *side : { "TextLeft", "TextRight" }) {
			int line = find(w->name + side + String::num_int64(i));
			if (line >= 0) {
				widgets[line]->text = String();
				widgets[line]->tooltip_wrap = false;
				widgets[line]->has_width = false;
				reset_layout(*widgets[line]);
				set_shown(line, false);
			}
		}
	}
	w->line_count = 0;
	invalidate();
}

void WowUI::tooltip_add_line(int id, const String &left, const String &right, const Color &left_color, const Color &right_color, bool wrap) {
	Widget *w = widget(id);
	if (!w || w->name.is_empty()) {
		return;
	}
	int n = ++w->line_count;
	String index = String::num_int64(n);
	int left_id = find(w->name + "TextLeft" + index);
	if (left_id < 0) {
		int previous = find(w->name + "TextLeft" + String::num_int64(n - 1));
		left_id = create_widget(KIND_FONT_STRING, "FontString", w->name + "TextLeft" + index, id, true);
		Widget &line = *widgets[left_id];
		if (Widget *model = widget(previous)) {
			line.font = model->font;
		} else {
			inherit_font(line.font, "GameTooltipText");
		}
		Anchor a;
		a.point = TOPLEFT;
		a.relative = previous;
		a.relative_point = BOTTOMLEFT;
		a.y = -2.0f;
		line.anchors.push_back(a);
	}
	int right_id = find(w->name + "TextRight" + index);
	if (right_id < 0) {
		right_id = create_widget(KIND_FONT_STRING, "FontString", w->name + "TextRight" + index, id, true);
		Widget &line = *widgets[right_id];
		line.font = widgets[left_id]->font;
		Anchor a;
		a.point = RIGHT;
		a.relative = left_id;
		a.relative_point = LEFT;
		a.x = 40.0f;
		line.anchors.push_back(a);
	}
	Widget &l = *widgets[left_id];
	l.text = left;
	l.font.color = left_color;
	l.tooltip_wrap = wrap;
	l.has_width = false;
	reset_layout(l);
	set_shown(left_id, true);
	Widget &r = *widgets[right_id];
	r.text = right;
	r.font.color = right_color;
	reset_layout(r);
	set_shown(right_id, !right.is_empty());
	invalidate();
}

void WowUI::tooltip_show(int id) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	layout_tooltip(*w);
	anchor_tooltip(*w);
	set_shown(id, true);
	raise(id);
}

void WowUI::message_add(int id, const String &text, const Color &color, int message_id) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	Message m;
	m.text = text;
	m.color = color;
	m.time = now;
	m.id = message_id;
	if (w->insert_top) {
		w->messages.push_front(m);
		while (static_cast<int>(w->messages.size()) > w->max_messages) {
			w->messages.pop_back();
		}
	} else {
		w->messages.push_back(m);
		while (static_cast<int>(w->messages.size()) > w->max_messages) {
			w->messages.pop_front();
		}
		if (w->scroll_offset > 0) {
			w->scroll_offset = std::min(w->scroll_offset + 1, static_cast<int>(w->messages.size()) - 1);
		}
	}
}

void WowUI::edit_set_text(int id, const String &text, bool fire) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	String value = text;
	if (!w->multi_line) {
		value = value.replace("\n", "");
	}
	if (w->max_letters > 0 && value.length() > w->max_letters) {
		value = value.substr(0, w->max_letters);
	}
	bool changed = value != w->text;
	w->text = value;
	w->cursor = static_cast<int>(value.length());
	w->select_all = false;
	reset_layout(*w);
	if (fire && changed) {
		call_script(id, "OnTextChanged");
	}
}

void WowUI::set_value(int id, float value, bool fire) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	float low = std::min(w->min_value, w->max_value);
	float high = std::max(w->min_value, w->max_value);
	float v = std::clamp(value, low, high);
	if (w->value_step > 0.0f) {
		v = low + std::round((v - low) / w->value_step) * w->value_step;
		v = std::clamp(v, low, high);
	}
	if (v == w->value) {
		return;
	}
	w->value = v;
	invalidate();
	if (fire) {
		Variant arg = v;
		call_script(id, "OnValueChanged", 1, &arg);
	}
}

void WowUI::start_moving(int id) {
	float l, b, r, t;
	if (!widget_rect(id, l, b, r, t)) {
		return;
	}
	moving = id;
	widgets[id]->moving = true;
	widgets[id]->move_grab = to_screen(mouse_position) - Vector2(l, t);
}

void WowUI::stop_moving(int id) {
	if (moving == id) {
		moving = -1;
	}
	if (Widget *w = widget(id)) {
		w->moving = false;
		w->user_placed = true;
	}
}

bool WowUI::run_key(const String &key, bool down) {
	if (!key_bindings.has(key)) {
		return false;
	}
	return run_binding(key_bindings[key], down);
}

} // namespace godot
