#pragma once

#include "wow_archive.h"
#include "wow_xml.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/rect2.hpp>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct lua_State;

namespace godot {

// The stock interface: Blizzard's GlueXML or FrameXML and addons, read from the archive and run in Lua 5.1.
// Coordinates follow the interface's: a 768 unit tall screen, origin bottom left, y up.
class WowUI : public Control {
	GDCLASS(WowUI, Control)
	friend struct WowUILua;

public:
	static constexpr float SCREEN_HEIGHT = 768.0f;

	enum Kind {
		KIND_FRAME,
		KIND_BUTTON,
		KIND_CHECK_BUTTON,
		KIND_EDIT_BOX,
		KIND_SCROLL_FRAME,
		KIND_SCROLLING_MESSAGE_FRAME,
		KIND_MESSAGE_FRAME,
		KIND_SLIDER,
		KIND_STATUS_BAR,
		KIND_COLOR_SELECT,
		KIND_MODEL,
		KIND_MINIMAP,
		KIND_GAME_TOOLTIP,
		KIND_SIMPLE_HTML,
		KIND_COOLDOWN,
		KIND_MOVIE_FRAME,
		KIND_WORLD_FRAME,
		KIND_TEXTURE,
		KIND_FONT_STRING,
		KIND_FONT,
	};

	enum Point { TOPLEFT, TOP, TOPRIGHT, LEFT, CENTER, RIGHT, BOTTOMLEFT, BOTTOM, BOTTOMRIGHT };
	enum Layer { LAYER_BACKGROUND, LAYER_BORDER, LAYER_ARTWORK, LAYER_OVERLAY, LAYER_HIGHLIGHT, LAYER_COUNT };
	enum Blend { BLEND_BLEND, BLEND_ADD, BLEND_MOD, BLEND_ALPHAKEY, BLEND_DISABLE };
	enum Justify { JUSTIFY_START, JUSTIFY_MIDDLE, JUSTIFY_END };

	struct FontInfo {
		String file;
		float height = 12.0f;
		bool outline = false;
		bool thick = false;
		Color color = Color(1, 1, 1, 1);
		Color shadow = Color(0, 0, 0, 0);
		Vector2 shadow_offset;
		int justify_h = JUSTIFY_MIDDLE;
		int justify_v = JUSTIFY_MIDDLE;
		float spacing = 0.0f;
	};

	struct Anchor {
		int point = TOPLEFT;
		int relative = -1; // A widget id; -1 is the parent, -2 the screen, -3 named but not built yet.
		String relative_name;
		int relative_point = TOPLEFT;
		float x = 0.0f;
		float y = 0.0f;
	};

	struct Backdrop {
		String bg_file;
		String edge_file;
		bool tile = false;
		float tile_size = 0.0f;
		float edge_size = 0.0f;
		float inset_left = 0.0f, inset_right = 0.0f, inset_top = 0.0f, inset_bottom = 0.0f;
		Color color = Color(1, 1, 1, 1);
		Color border_color = Color(1, 1, 1, 1);
	};

	struct TextRun {
		String text;
		Color color;
		bool colored = false;
		int link = -1;
	};

	struct TextLine {
		std::vector<TextRun> runs;
		float width = 0.0f; // Pixels.
	};

	struct TextLayout {
		String key;
		std::vector<TextLine> lines;
		std::vector<String> links;
		float line_height = 0.0f; // Pixels.
		int font_px = 0;
		float max_width = 0.0f; // Pixels.
	};

	struct Message {
		String text;
		Color color;
		double time = 0.0;
		int id = 0;
		TextLayout layout;
	};

	struct Widget {
		int id = 0;
		Kind kind = KIND_FRAME;
		String type; // The object type the interface asks about, such as CheckButton or PlayerModel.
		String name;
		int parent = -1;
		std::vector<int> children;
		std::vector<int> regions;
		int lua_ref = -2;

		bool shown = true;
		bool visible = false;
		float alpha = 1.0f;
		float scale = 1.0f;
		int strata = 2;
		int level = 0;
		int layer = LAYER_ARTWORK;
		bool toplevel = false;

		std::vector<Anchor> anchors;
		float width = 0.0f;
		float height = 0.0f;
		bool has_width = false;
		bool has_height = false;
		// Resolved in screen units, bottom left origin.
		float left = 0.0f, bottom = 0.0f, right = 0.0f, top = 0.0f;
		uint64_t rect_version = 0;
		bool rect_valid = false;
		bool resolving = false;

		bool mouse = false;
		bool wheel = false;
		bool keyboard = false;
		bool movable = false;
		bool resizable = false;
		bool user_placed = false;
		bool clamped = false;
		int frame_id = 0;
		float hit_left = 0.0f, hit_right = 0.0f, hit_top = 0.0f, hit_bottom = 0.0f;
		std::unordered_map<std::string, int> scripts;
		std::unordered_set<std::string> events;
		std::unique_ptr<Backdrop> backdrop;
		int drag_buttons = 0;
		int click_up = 1; // Mouse button bits a click fires on, released or pressed.
		int click_down = 0;

		// Texture.
		String texture_file;
		Ref<Texture2D> texture;
		bool solid = false;
		Color solid_color = Color(1, 1, 1, 1);
		float coords[8] = { 0, 0, 0, 1, 1, 0, 1, 1 }; // UL, LL, UR, LR as u, v pairs.
		Color vertex_color = Color(1, 1, 1, 1);
		int blend = BLEND_BLEND;
		bool desaturated = false;
		bool gradient = false;
		bool gradient_vertical = false;
		Color gradient_min = Color(1, 1, 1, 1);
		Color gradient_max = Color(1, 1, 1, 1);
		int bar_owner = -1; // The status bar or slider this texture fills or thumbs.

		// FontString, and the fonts a Font object, Button, EditBox or message frame keeps.
		FontInfo font;
		String text;
		bool nonspacewrap = false;
		int max_lines = 0;
		TextLayout layout;

		// Button.
		int normal_texture = -1, pushed_texture = -1, disabled_texture = -1, highlight_texture = -1;
		int checked_texture = -1, disabled_checked_texture = -1;
		int font_string = -1;
		FontInfo normal_font, highlight_font, disabled_font;
		bool has_normal_font = false, has_highlight_font = false, has_disabled_font = false;
		Vector2 pushed_offset;
		bool enabled = true;
		bool pushed = false;
		bool locked_highlight = false;
		bool checked = false;

		// StatusBar and Slider.
		float min_value = 0.0f, max_value = 0.0f, value = 0.0f, value_step = 0.0f;
		int bar_texture = -1;
		Color bar_color = Color(1, 1, 1, 1);
		bool vertical = false;

		// EditBox.
		int max_letters = 0;
		bool numeric = false, password = false, multi_line = false, auto_focus = true, ignore_arrows = false;
		int cursor = 0;
		bool select_all = false;
		std::vector<String> history;
		int history_lines = 0;
		int history_index = -1;
		float text_left = 0.0f, text_right = 0.0f, text_top = 0.0f, text_bottom = 0.0f;

		// ScrollFrame.
		int scroll_child = -1;
		float scroll_h = 0.0f, scroll_v = 0.0f;
		int scrolled_by = -1; // The scroll frame this widget is the child of.

		// ScrollingMessageFrame and MessageFrame.
		std::deque<Message> messages;
		int max_messages = 128;
		bool fading = true;
		float time_visible = 10.0f;
		float fade_duration = 3.0f;
		int scroll_offset = 0;
		bool insert_top = false;

		// GameTooltip.
		int owner = -1;
		String owner_anchor;
		Vector2 owner_offset;
		int line_count = 0;
		float tooltip_padding = 0.0f;

		// Cooldown, a Model on the cooldown indicator the interface steps with SetSequenceTime.
		bool cooldown_model = false;
		float cooldown_progress = 0.0f;
		bool cooldown_finishing = false;

		// Model and Minimap: a picture drawn by the game, laid in by the host.
		Ref<Texture2D> external;
		Dictionary state; // What the interface last set, for the matching getters.
		float min_width = 0.0f;
		float range_h = -1.0f, range_v = -1.0f;

		// Moving.
		bool moving = false;
		Vector2 move_grab;
	};

private:
	struct DrawState {
		int material = 0;
		bool clip = false;
		Rect2 clip_rect;
		bool repeat = false;
		bool operator==(const DrawState &o) const {
			return material == o.material && clip == o.clip && repeat == o.repeat && (!clip || clip_rect == o.clip_rect);
		}
	};

	Ref<WowArchive> archive;
	String disk_root; // The client folder; loose Interface files there win over the archive's.
	lua_State *L = nullptr;
	std::vector<std::unique_ptr<Widget>> widgets;
	std::unordered_map<std::string, const WowXmlNode *> templates;
	std::vector<std::unique_ptr<WowXmlDocument>> documents;
	std::unordered_map<std::string, std::vector<int>> event_frames;
	std::vector<Callable> functions;
	std::unordered_map<std::string, Ref<Font>> fonts;
	std::unordered_map<std::string, Ref<Texture2D>> textures;
	std::map<std::string, std::string> bindings;
	std::vector<std::string> binding_order;
	std::unordered_set<std::string> bindings_on_up;
	Dictionary key_bindings; // Key, such as CTRL-1, to binding name.
	Dictionary cvars;
	std::unordered_set<std::string> loaded_addons;
	PackedStringArray errors;
	std::unordered_set<std::string> missing;
	uint64_t layout_version = 1;
	double start_usec = 0.0;
	double now = 0.0;
	float screen_width = 1024.0f;

	// Drawing.
	std::vector<RID> items;
	int item_count = 0;
	DrawState item_state;
	bool item_open = false;
	Ref<Material> materials[4]; // None, add, multiply, desaturate.
	float alpha_of(const Widget &w) const;
	bool clip_of(const Widget &w, Rect2 &r_clip);
	bool region_drawn(const Widget &region, const Widget &frame) const;
	float px = 1.0f; // Pixels per screen unit.

	// Input.
	int hovered = -1;
	int pressed = -1;
	int pressed_button = 0;
	Vector2 press_position;
	bool dragging = false;
	int focus = -1;
	int moving = -1;
	Vector2 mouse_position;
	bool cursor_busy = false;
	double caret_time = 0.0;

	bool read_file(const String &path, std::string &r_out) const;
	Ref<Font> fallback_font();
	Ref<Font> fallback;

	// Lua.
	void open_lua();
	void close_lua();
	void register_core();
	void register_methods();
	bool run_chunk(const std::string &code, const std::string &chunk_name);
	int compile(const std::string &code, const std::string &chunk_name);
	void report(const String &message);
	void push_widget(int id);
	void call_script(int id, const char *handler, int arg_count = 0, const Variant *args = nullptr, const char *event = nullptr);
	bool has_script(int id, const char *handler) const;
	void set_script(int id, const std::string &handler, int ref);

	// Building.
	int create_widget(Kind kind, const String &type, const String &name, int parent, bool region);
	int build(const WowXmlNode &node, int parent, bool is_region_hint, bool run_load);
	void apply(int id, const WowXmlNode &node);
	void apply_inherits(int id, const std::string &inherits);
	void apply_child(int id, const WowXmlNode &child);
	void apply_region(int id, const WowXmlNode &node);
	void apply_anchors(int id, const WowXmlNode &node);
	void apply_size(int id, const WowXmlNode &node);
	void apply_font(FontInfo &font, const WowXmlNode &node);
	void inherit_font(FontInfo &font, const std::string &names);
	int button_texture(int id, const WowXmlNode &node, int layer);
	void process_ui(const WowXmlNode &root, const std::string &dir);
	String resolve_name(const String &name, int parent) const;
	void load_on(int id);

	// Layout and visibility.
	bool resolve(Widget &w);
	float effective_scale(const Widget &w) const;
	void invalidate() { ++layout_version; }
	void update_visibility(int id, bool fire);
	bool parent_visible(const Widget &w) const;
	void set_shown(int id, bool shown);
	float text_width(Widget &w);
	void apply_button_font(Widget &button);
	void layout_tooltip(Widget &tooltip);
	void anchor_tooltip(Widget &tooltip);
	Rect2 to_pixels(float left, float bottom, float right, float top) const;
	bool screen_rect(int id, Rect2 &r_rect);
	int set_level(int id, int level);
	void raise(int id);

	// Drawing.
	void begin_draw();
	void end_draw();
	RID item(const DrawState &state);
	void draw_frame(Widget &w, const DrawState &base);
	void draw_region(Widget &region, Widget &frame, const DrawState &base);
	void draw_texture(Widget &t, const Rect2 &rect, const DrawState &base);
	void draw_quad(RID rid, const Rect2 &rect, const float *coords, const Color colors[4], const Ref<Texture2D> &texture);
	void draw_backdrop(Widget &w, const Rect2 &rect, const DrawState &base);
	void draw_text(const TextLayout &layout, const FontInfo &font, const Rect2 &rect, float alpha, const DrawState &base, int justify_h, int justify_v, float y_offset = 0.0f);
	void draw_edit_box(Widget &w, const Rect2 &rect, const DrawState &base);
	void draw_messages(Widget &w, const Rect2 &rect, const DrawState &base);
	void draw_cooldown(Widget &w, const Rect2 &rect, const DrawState &base);
	Ref<Font> font_file(const String &path);
	Ref<Texture2D> load_texture(const String &path);
	int font_px(const FontInfo &font, float scale) const;
	void layout_text(TextLayout &layout, const String &text, const FontInfo &font, int size, float max_width, bool nonspacewrap, int max_lines);
	void collect_draw(std::vector<int> &out) const;

	// Input.
	int hit_test(const Vector2 &position) const;
	void set_hovered(int id);
	void set_focus(int id);
	bool edit_key(Widget &w, const Ref<InputEvent> &event);
	void edit_changed(Widget &w);
	bool hyperlink_at(Widget &w, const Vector2 &position, String &r_link, String &r_text);
	static String button_name(int button);
	static String key_name(int64_t keycode);
	void click(int id, const String &button, bool down);
	Vector2 to_screen(const Vector2 &pixel) const;

	void update_frame(double delta);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	WowUI();
	~WowUI() override;

	// For the Lua bindings.
	static bool kind_for_type(const std::string &type, Kind &r_kind, String &r_type);
	int hovered_widget() const { return hovered; }
	void push(int id) { push_widget(id); }
	void set_parent(int id, int parent);
	lua_State *lua() const { return L; }
	Widget *widget(int id) const { return id >= 0 && id < static_cast<int>(widgets.size()) ? widgets[id].get() : nullptr; }
	int widget_from(int index) const;
	int find(const String &name) const;
	int check_widget(lua_State *state, int index) const;
	void mark_dirty() { invalidate(); }
	void method_set_point(int id, int arg_base);
	void method_show(int id, bool shown) { set_shown(id, shown); }
	int method_create(Kind kind, const String &type, const String &name, int parent, const std::string &inherits, int layer);
	void register_event(int id, const std::string &event, bool on);
	void unregister_all(int id);
	void set_text(int id, const String &text);
	void set_texture(int id, const String &file);
	void set_font_string_font(int id, const FontInfo &font);
	void tooltip_add_line(int id, const String &left, const String &right, const Color &left_color, const Color &right_color);
	void tooltip_clear(int id);
	void tooltip_show(int id);
	void message_add(int id, const String &text, const Color &color, int message_id);
	void edit_set_text(int id, const String &text, bool fire);
	void focus_widget(int id, bool focused) { set_focus(focused ? id : (focus == id ? -1 : focus)); }
	void set_value(int id, float value, bool fire);
	void start_moving(int id);
	void stop_moving(int id);
	void click_widget(int id, const String &button) { click(id, button, false); }
	bool widget_rect(int id, float &r_left, float &r_bottom, float &r_right, float &r_top);
	float widget_scale(int id) const { const Widget *w = widget(id); return w ? effective_scale(*w) : 1.0f; }
	float get_screen_width() const { return screen_width; }
	float text_height(int id);
	float string_width(int id) { Widget *w = widget(id); return w ? text_width(*w) : 0.0f; }
	double time() const { return now; }
	Vector2 cursor_position() const { return to_screen(mouse_position); }
	void call_function(int index, int arg_count);
	void model_call(int id, const String &method, const Array &args);
	void note_missing(const std::string &name);
	Dictionary &get_cvar_store() { return cvars; }
	void cvar_set(const String &name, const String &value);
	bool load_xml_file(const String &path);
	bool load_lua_file(const String &path);
	String read_text(const String &path) const;
	void fire_lua_event(const String &event, const Array &args) { fire_event(event, args); }
	void update_scroll_child(int id);
	void layout_text_widget(Widget &w);
	void reset_layout(Widget &w) { w.layout.key = String(); }
	int focused() const { return focus; }
	bool is_cursor_busy() const { return cursor_busy; }

	// For GDScript.
	void set_archive(const Ref<WowArchive> &p_archive) { archive = p_archive; }
	Ref<WowArchive> get_archive() const { return archive; }
	void set_disk_root(const String &p_root) { disk_root = p_root; }
	String get_disk_root() const { return disk_root; }
	bool has_file(const String &path) const;
	// The loose file that overrides an archive path, or empty when the archive copy is used.
	String get_disk_file(const String &path) const;
	bool load_toc(const String &path);
	bool load_xml(const String &path) { return load_xml_file(path); }
	PackedStringArray get_addons() const;
	Dictionary get_addon_info(const String &name) const;
	bool load_addon(const String &name);
	bool is_addon_loaded(const String &name) const;
	bool run_lua(const String &code, const String &chunk_name = "script");
	void load_bindings(const String &path);
	PackedStringArray get_binding_names() const;
	bool run_binding(const String &name, bool down);
	void set_key_bindings(const Dictionary &values) { key_bindings = values; }
	Dictionary get_key_bindings() const { return key_bindings; }
	bool run_key(const String &key, bool down);
	void register_function(const String &name, const Callable &callable);
	void fire_event(const String &event, const Array &args = Array());
	Variant call_lua(const String &function, const Array &args = Array());
	Variant get_lua_global(const String &name);
	void set_lua_global(const String &name, const Variant &value);
	bool has_widget(const String &name) const { return find(name) >= 0; }
	Rect2 get_widget_rect(const String &name);
	bool is_widget_visible(const String &name) const;
	void set_widget_texture(const String &name, const Ref<Texture2D> &texture);
	void set_widget_texture_id(int id, const Ref<Texture2D> &texture);
	String get_widget_name(int id) const { const Widget *w = widget(id); return w ? w->name : String(); }
	int get_widget_at(const Vector2 &position) const { return hit_test(position); }
	void run_widget_script(int id, const String &handler) { call_script(id, handler.utf8().get_data()); }
	int get_widget_id(const String &name) const { return find(name); }
	Array get_model_frames();
	void set_cursor_busy(bool busy) { cursor_busy = busy; }
	bool has_text_focus() const { return focus >= 0; }
	void clear_text_focus() { set_focus(-1); }
	void set_cvars(const Dictionary &values) { cvars = values; }
	Dictionary get_cvars() const { return cvars; }
	PackedStringArray get_errors() const { return errors; }
	PackedStringArray get_missing() const;
	int widget_count() const { return static_cast<int>(widgets.size()); }
	bool key_event(const Ref<InputEvent> &event);
	static String event_key_name(const Ref<InputEvent> &event);

	void _gui_input(const Ref<InputEvent> &event) override;
	bool _has_point(const Vector2 &point) const override;
};

} // namespace godot

VARIANT_ENUM_CAST(WowUI::Kind);
