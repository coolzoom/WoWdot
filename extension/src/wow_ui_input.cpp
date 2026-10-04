#include "wow_ui.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>

#include <algorithm>

namespace godot {

namespace {

int button_bit(MouseButton button) {
	switch (button) {
		case MOUSE_BUTTON_LEFT:
			return 1;
		case MOUSE_BUTTON_RIGHT:
			return 2;
		case MOUSE_BUTTON_MIDDLE:
			return 4;
		case MOUSE_BUTTON_XBUTTON1:
			return 8;
		case MOUSE_BUTTON_XBUTTON2:
			return 16;
		default:
			return 0;
	}
}

} // namespace

String WowUI::button_name(int bit) {
	switch (bit) {
		case 1:
			return "LeftButton";
		case 2:
			return "RightButton";
		case 4:
			return "MiddleButton";
		case 8:
			return "Button4";
		case 16:
			return "Button5";
		default:
			return "LeftButton";
	}
}

Vector2 WowUI::to_screen(const Vector2 &pixel) const {
	return Vector2(pixel.x / px, SCREEN_HEIGHT - pixel.y / px);
}

int WowUI::hit_test(const Vector2 &position) const {
	WowUI *self = const_cast<WowUI *>(this);
	std::vector<int> order;
	collect_draw(order);
	for (auto it = order.rbegin(); it != order.rend(); ++it) {
		Widget &w = *widgets[*it];
		if (!w.mouse) {
			continue;
		}
		Rect2 rect;
		if (!self->screen_rect(w.id, rect)) {
			continue;
		}
		float s = effective_scale(w) * px;
		rect.position += Vector2(w.hit_left, w.hit_top) * s;
		rect.size -= Vector2(w.hit_left + w.hit_right, w.hit_top + w.hit_bottom) * s;
		if (!rect.has_point(position)) {
			continue;
		}
		Rect2 clip;
		if (self->clip_of(w, clip) && !clip.has_point(position)) {
			continue;
		}
		return w.id;
	}
	return -1;
}

void WowUI::set_hovered(int id) {
	if (hovered == id) {
		return;
	}
	int old = hovered;
	hovered = id;
	if (Widget *w = widget(old)) {
		apply_button_font(*w);
		call_script(old, "OnLeave");
	}
	if (Widget *w = widget(id)) {
		apply_button_font(*w);
		call_script(id, "OnEnter");
	}
}

void WowUI::set_focus(int id) {
	if (focus == id) {
		return;
	}
	int old = focus;
	focus = id;
	caret_time = 0.0;
	if (Widget *w = widget(old)) {
		w->select_all = false;
		call_script(old, "OnEditFocusLost");
	}
	if (Widget *w = widget(id)) {
		w->cursor = static_cast<int>(w->text.length());
		call_script(id, "OnEditFocusGained");
	}
}

void WowUI::edit_changed(Widget &w) {
	reset_layout(w);
	caret_time = 0.0;
	call_script(w.id, "OnTextChanged");
}

bool WowUI::edit_key(Widget &w, const Ref<InputEvent> &event) {
	Ref<InputEventKey> key = event;
	if (key.is_null()) {
		return false;
	}
	if (!key->is_pressed()) {
		return true;
	}
	int id = w.id;
	Key code = key->get_keycode();
	bool ctrl = key->is_ctrl_pressed() || key->is_meta_pressed();
	int length = static_cast<int>(w.text.length());
	w.cursor = std::clamp(w.cursor, 0, length);
	auto replace = [&](const String &text, int cursor) {
		String value = text;
		if (w.max_letters > 0 && value.length() > w.max_letters) {
			value = value.substr(0, w.max_letters);
		}
		w.text = value;
		w.cursor = std::clamp(cursor, 0, static_cast<int>(value.length()));
		w.select_all = false;
		edit_changed(w);
	};
	switch (code) {
		case KEY_ENTER:
		case KEY_KP_ENTER:
			if (w.multi_line && !w.scripts.count("OnEnterPressed")) {
				replace(w.text.substr(0, w.cursor) + "\n" + w.text.substr(w.cursor), w.cursor + 1);
			} else {
				call_script(id, "OnEnterPressed");
			}
			return true;
		case KEY_ESCAPE:
			if (w.scripts.count("OnEscapePressed")) {
				call_script(id, "OnEscapePressed");
			} else {
				set_focus(-1);
			}
			return true;
		case KEY_TAB:
			call_script(id, "OnTabPressed");
			return true;
		case KEY_BACKSPACE:
			if (w.select_all) {
				replace(String(), 0);
			} else if (w.cursor > 0) {
				replace(w.text.substr(0, w.cursor - 1) + w.text.substr(w.cursor), w.cursor - 1);
			}
			return true;
		case KEY_DELETE:
			if (w.select_all) {
				replace(String(), 0);
			} else if (w.cursor < length) {
				replace(w.text.substr(0, w.cursor) + w.text.substr(w.cursor + 1), w.cursor);
			}
			return true;
		case KEY_LEFT:
		case KEY_RIGHT: {
			Variant arg = String(code == KEY_LEFT ? "LEFT" : "RIGHT");
			call_script(id, "OnArrowPressed", 1, &arg);
			w.select_all = false;
			w.cursor = std::clamp(w.cursor + (code == KEY_LEFT ? -1 : 1), 0, length);
			caret_time = 0.0;
			return true;
		}
		case KEY_HOME:
			w.cursor = 0;
			w.select_all = false;
			return true;
		case KEY_END:
			w.cursor = length;
			w.select_all = false;
			return true;
		case KEY_UP:
		case KEY_DOWN: {
			Variant arg = String(code == KEY_UP ? "UP" : "DOWN");
			call_script(id, "OnArrowPressed", 1, &arg);
			bool history = !w.history.empty() && (!w.ignore_arrows || key->is_alt_pressed());
			if (history) {
				int count = static_cast<int>(w.history.size());
				if (code == KEY_UP) {
					w.history_index = w.history_index < 0 ? count - 1 : std::max(0, w.history_index - 1);
				} else {
					w.history_index = w.history_index < 0 ? -1 : w.history_index + 1;
					if (w.history_index >= count) {
						w.history_index = -1;
					}
				}
				String line = w.history_index >= 0 ? w.history[w.history_index] : String();
				replace(line, static_cast<int>(line.length()));
			}
			return true;
		}
		default:
			break;
	}
	if (ctrl && code == KEY_A) {
		w.select_all = !w.text.is_empty();
		return true;
	}
	if (ctrl && code == KEY_C) {
		DisplayServer::get_singleton()->clipboard_set(w.password ? String() : w.text);
		return true;
	}
	if (ctrl && code == KEY_V) {
		String paste = DisplayServer::get_singleton()->clipboard_get();
		if (!w.multi_line) {
			paste = paste.replace("\r", "").replace("\n", " ");
		}
		String base = w.select_all ? String() : w.text;
		int at = w.select_all ? 0 : w.cursor;
		replace(base.substr(0, at) + paste + base.substr(at), at + static_cast<int>(paste.length()));
		return true;
	}
	char32_t c = static_cast<char32_t>(key->get_unicode());
	if (c >= 32 && !ctrl) {
		if (w.numeric && (c < '0' || c > '9')) {
			return true;
		}
		String base = w.select_all ? String() : w.text;
		int at = w.select_all ? 0 : w.cursor;
		if (w.max_letters > 0 && base.length() >= w.max_letters) {
			return true;
		}
		if (c == ' ') {
			call_script(id, "OnSpacePressed");
		}
		replace(base.substr(0, at) + String::chr(c) + base.substr(at), at + 1);
		Variant arg = String::chr(c);
		call_script(id, "OnChar", 1, &arg);
		return true;
	}
	return true;
}

bool WowUI::hyperlink_at(Widget &w, const Vector2 &position, String &r_link, String &r_text) {
	Rect2 rect;
	if (!screen_rect(w.id, rect) || w.insert_top) {
		return false;
	}
	float bottom = rect.position.y + rect.size.y;
	int skip = w.scroll_offset;
	for (auto it = w.messages.rbegin(); it != w.messages.rend(); ++it) {
		if (skip > 0) {
			--skip;
			continue;
		}
		Message &m = *it;
		if (m.layout.key.is_empty()) {
			break;
		}
		float height = static_cast<float>(m.layout.lines.size()) * m.layout.line_height;
		bottom -= height;
		if (position.y >= bottom && position.y < bottom + height) {
			int line_index = static_cast<int>((position.y - bottom) / m.layout.line_height);
			if (line_index < 0 || line_index >= static_cast<int>(m.layout.lines.size())) {
				return false;
			}
			const TextLine &line = m.layout.lines[line_index];
			Ref<Font> f = font_file(w.font.file);
			float x = rect.position.x;
			if (w.font.justify_h == JUSTIFY_MIDDLE) {
				x += (rect.size.x - line.width) * 0.5f;
			} else if (w.font.justify_h == JUSTIFY_END) {
				x += rect.size.x - line.width;
			}
			for (const TextRun &run : line.runs) {
				float width = static_cast<float>(f->get_string_size(run.text, HORIZONTAL_ALIGNMENT_LEFT, -1, m.layout.font_px).x);
				if (position.x >= x && position.x < x + width && run.link >= 0) {
					r_link = m.layout.links[run.link];
					r_text = run.text;
					return true;
				}
				x += width;
			}
			return false;
		}
		if (bottom < rect.position.y) {
			break;
		}
	}
	return false;
}

void WowUI::click(int id, const String &button, bool down) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	bool is_button = w->kind == KIND_BUTTON || w->kind == KIND_CHECK_BUTTON;
	if (is_button && !w->enabled) {
		return;
	}
	if (w->kind == KIND_CHECK_BUTTON) {
		w->checked = !w->checked;
	}
	Variant args[2] = { button, down };
	call_script(id, "OnClick", 2, args);
}

void WowUI::_gui_input(const Ref<InputEvent> &event) {
	Ref<InputEventMouseMotion> motion = event;
	if (motion.is_valid()) {
		mouse_position = motion->get_position();
		set_hovered(hit_test(mouse_position));
		if (Widget *p = widget(pressed)) {
			if (!dragging && (p->drag_buttons & pressed_button) && mouse_position.distance_to(press_position) > 4.0f) {
				dragging = true;
				Variant arg = button_name(pressed_button);
				call_script(pressed, "OnDragStart", 1, &arg);
			}
			if (p->kind == KIND_SLIDER) {
				float l, b, r, t;
				if (widget_rect(pressed, l, b, r, t)) {
					Vector2 at = to_screen(mouse_position);
					float fraction = p->vertical ? (t - at.y) / std::max(1.0f, t - b) : (at.x - l) / std::max(1.0f, r - l);
					set_value(pressed, p->min_value + std::clamp(fraction, 0.0f, 1.0f) * (p->max_value - p->min_value), true);
				}
			}
		}
		if (hovered >= 0 || pressed >= 0) {
			accept_event();
		}
		return;
	}
	Ref<InputEventMouseButton> mouse = event;
	if (mouse.is_null()) {
		return;
	}
	mouse_position = mouse->get_position();
	MouseButton index = mouse->get_button_index();
	if (index == MOUSE_BUTTON_WHEEL_UP || index == MOUSE_BUTTON_WHEEL_DOWN) {
		if (!mouse->is_pressed()) {
			return;
		}
		for (int id = hit_test(mouse_position); id >= 0; id = widgets[id]->parent) {
			if (widgets[id]->wheel || widgets[id]->scripts.count("OnMouseWheel")) {
				Variant arg = index == MOUSE_BUTTON_WHEEL_UP ? 1 : -1;
				call_script(id, "OnMouseWheel", 1, &arg);
				accept_event();
				return;
			}
		}
		if (hit_test(mouse_position) >= 0) {
			accept_event();
		}
		return;
	}
	int bit = button_bit(index);
	if (bit == 0) {
		return;
	}
	String name = button_name(bit);
	if (mouse->is_pressed()) {
		int id = hit_test(mouse_position);
		if (id < 0) {
			return;
		}
		accept_event();
		for (int each = id; each >= 0; each = widgets[each]->parent) {
			if (widgets[each]->toplevel) {
				raise(each);
				break;
			}
		}
		Widget &w = *widgets[id];
		if (w.kind == KIND_EDIT_BOX) {
			set_focus(id);
		}
		pressed = id;
		pressed_button = bit;
		press_position = mouse_position;
		dragging = false;
		bool is_button = w.kind == KIND_BUTTON || w.kind == KIND_CHECK_BUTTON;
		if (is_button && w.enabled) {
			w.pushed = true;
		}
		Variant arg = name;
		call_script(id, "OnMouseDown", 1, &arg);
		if (mouse->is_double_click()) {
			call_script(id, "OnDoubleClick", 1, &arg);
		}
		if (is_button && (w.click_down & bit)) {
			click(id, name, true);
		}
		float l, b, r, t;
		if (w.kind == KIND_SLIDER && widget_rect(id, l, b, r, t)) {
			Vector2 at = to_screen(mouse_position);
			float fraction = w.vertical ? (t - at.y) / std::max(1.0f, t - b) : (at.x - l) / std::max(1.0f, r - l);
			set_value(id, w.min_value + std::clamp(fraction, 0.0f, 1.0f) * (w.max_value - w.min_value), true);
		}
		return;
	}
	if (pressed < 0) {
		return;
	}
	accept_event();
	int id = pressed;
	int was = pressed_button;
	pressed = -1;
	Widget &w = *widgets[id];
	w.pushed = false;
	Variant arg = button_name(was);
	call_script(id, "OnMouseUp", 1, &arg);
	int target = hit_test(mouse_position);
	if (dragging) {
		dragging = false;
		call_script(id, "OnDragStop");
		if (target >= 0 && target != id) {
			call_script(target, "OnReceiveDrag");
		}
	} else if (target == id && (widgets[id]->kind == KIND_BUTTON || widgets[id]->kind == KIND_CHECK_BUTTON) && (widgets[id]->click_up & was)) {
		click(id, button_name(was), false);
	}
	set_hovered(hit_test(mouse_position));
}

bool WowUI::_has_point(const Vector2 &point) const {
	return pressed >= 0 || moving >= 0 || hit_test(point) >= 0;
}

String WowUI::key_name(int64_t keycode) {
	Key code = static_cast<Key>(keycode);
	if (code >= KEY_A && code <= KEY_Z) {
		return String::chr(static_cast<char32_t>('A' + (code - KEY_A)));
	}
	if (code >= KEY_0 && code <= KEY_9) {
		return String::chr(static_cast<char32_t>('0' + (code - KEY_0)));
	}
	if (code >= KEY_F1 && code <= KEY_F12) {
		return "F" + String::num_int64(1 + (code - KEY_F1));
	}
	if (code >= KEY_KP_0 && code <= KEY_KP_9) {
		return "NUMPAD" + String::num_int64(code - KEY_KP_0);
	}
	switch (code) {
		case KEY_ESCAPE:
			return "ESCAPE";
		case KEY_ENTER:
		case KEY_KP_ENTER:
			return "ENTER";
		case KEY_SPACE:
			return "SPACE";
		case KEY_TAB:
			return "TAB";
		case KEY_BACKSPACE:
			return "BACKSPACE";
		case KEY_UP:
			return "UP";
		case KEY_DOWN:
			return "DOWN";
		case KEY_LEFT:
			return "LEFT";
		case KEY_RIGHT:
			return "RIGHT";
		case KEY_INSERT:
			return "INSERT";
		case KEY_DELETE:
			return "DELETE";
		case KEY_HOME:
			return "HOME";
		case KEY_END:
			return "END";
		case KEY_PAGEUP:
			return "PAGEUP";
		case KEY_PAGEDOWN:
			return "PAGEDOWN";
		case KEY_KP_ADD:
			return "NUMPADPLUS";
		case KEY_KP_SUBTRACT:
			return "NUMPADMINUS";
		case KEY_KP_MULTIPLY:
			return "NUMPADMULTIPLY";
		case KEY_KP_DIVIDE:
			return "NUMPADDIVIDE";
		case KEY_KP_PERIOD:
			return "NUMPADDECIMAL";
		case KEY_MINUS:
			return "-";
		case KEY_EQUAL:
			return "=";
		case KEY_BRACKETLEFT:
			return "[";
		case KEY_BRACKETRIGHT:
			return "]";
		case KEY_BACKSLASH:
			return "\\";
		case KEY_SEMICOLON:
			return ";";
		case KEY_APOSTROPHE:
			return "'";
		case KEY_COMMA:
			return ",";
		case KEY_PERIOD:
			return ".";
		case KEY_SLASH:
			return "/";
		case KEY_QUOTELEFT:
			return "`";
		case KEY_PRINT:
			return "PRINTSCREEN";
		case KEY_NUMLOCK:
			return "NUMLOCK";
		case KEY_SCROLLLOCK:
			return "SCROLLLOCK";
		case KEY_PAUSE:
			return "PAUSE";
		default:
			return String();
	}
}

String WowUI::event_key_name(const Ref<InputEvent> &event) {
	Ref<InputEventKey> key = event;
	if (key.is_null()) {
		return String();
	}
	String name = key_name(key->get_keycode());
	if (name.is_empty()) {
		return String();
	}
	String prefix;
	if (key->is_alt_pressed()) {
		prefix += "ALT-";
	}
	if (key->is_ctrl_pressed() || key->is_meta_pressed()) {
		prefix += "CTRL-";
	}
	if (key->is_shift_pressed()) {
		prefix += "SHIFT-";
	}
	return prefix + name;
}

bool WowUI::key_event(const Ref<InputEvent> &event) {
	Ref<InputEventKey> key = event;
	if (key.is_null()) {
		return false;
	}
	if (Widget *w = widget(focus)) {
		return edit_key(*w, event);
	}
	String name = event_key_name(event);
	String bare = key_name(key->get_keycode());
	if (bare.is_empty()) {
		return false;
	}
	// The topmost frame that takes the keyboard eats every key.
	std::vector<int> order;
	collect_draw(order);
	for (auto it = order.rbegin(); it != order.rend(); ++it) {
		Widget &w = *widgets[*it];
		if (!w.keyboard) {
			continue;
		}
		if (key->is_echo() && !key->is_pressed()) {
			return true;
		}
		Variant arg = bare;
		call_script(w.id, key->is_pressed() ? "OnKeyDown" : "OnKeyUp", 1, &arg);
		return true;
	}
	if (key->is_echo()) {
		return key_bindings.has(name);
	}
	if (run_key(name, key->is_pressed())) {
		return true;
	}
	return !key->is_pressed() ? false : (name != bare && run_key(bare, true));
}

} // namespace godot
