#pragma once

#include "wow_ui.h"

#include <lua.hpp>

namespace godot::wowui {

WowUI *ui(lua_State *L);
void push_string(lua_State *L, const String &text);
String to_string(lua_State *L, int index);
void push_variant(lua_State *L, const Variant &value);
Variant to_variant(lua_State *L, int index, int depth = 0);
int point_from(const String &name);
const char *point_name(int point);
int strata_from(const String &name);
const char *strata_name(int strata);
int layer_from(const String &name);
const char *layer_name(int layer);
Color color_args(lua_State *L, int first, const Color &fallback = Color(1, 1, 1, 1));

} // namespace godot::wowui
