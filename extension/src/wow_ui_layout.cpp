#include "wow_ui.h"
#include "wow_ui_lua.h"

#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/system_font.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace godot {

namespace {

constexpr int NO_REL = -9;
// The narrowest a wrapping tooltip line runs before it breaks, in screen units.
constexpr float TOOLTIP_WRAP_WIDTH = 250.0f;

Vector2 point_at(int point, float left, float bottom, float right, float top) {
	float x = point == WowUI::TOPLEFT || point == WowUI::LEFT || point == WowUI::BOTTOMLEFT ? left : point == WowUI::TOPRIGHT || point == WowUI::RIGHT || point == WowUI::BOTTOMRIGHT ? right : (left + right) * 0.5f;
	float y = point == WowUI::TOPLEFT || point == WowUI::TOP || point == WowUI::TOPRIGHT ? top : point == WowUI::BOTTOMLEFT || point == WowUI::BOTTOM || point == WowUI::BOTTOMRIGHT ? bottom : (top + bottom) * 0.5f;
	return Vector2(x, y);
}

int horizontal_of(int point) {
	switch (point) {
		case WowUI::TOPLEFT:
		case WowUI::LEFT:
		case WowUI::BOTTOMLEFT:
			return 0;
		case WowUI::TOPRIGHT:
		case WowUI::RIGHT:
		case WowUI::BOTTOMRIGHT:
			return 2;
		default:
			return 1;
	}
}

int vertical_of(int point) {
	switch (point) {
		case WowUI::BOTTOMLEFT:
		case WowUI::BOTTOM:
		case WowUI::BOTTOMRIGHT:
			return 0;
		case WowUI::TOPLEFT:
		case WowUI::TOP:
		case WowUI::TOPRIGHT:
			return 2;
		default:
			return 1;
	}
}

// One axis: a start edge, a middle and an end edge, any of which anchors may pin.
bool solve_axis(bool has[3], const float at[3], float size, float &r_start, float &r_end) {
	if (has[0] && has[2]) {
		r_start = at[0];
		r_end = at[2];
	} else if (has[0]) {
		r_start = at[0];
		r_end = has[1] ? at[0] + (at[1] - at[0]) * 2.0f : at[0] + size;
	} else if (has[2]) {
		r_end = at[2];
		r_start = has[1] ? at[2] - (at[2] - at[1]) * 2.0f : at[2] - size;
	} else if (has[1]) {
		r_start = at[1] - size * 0.5f;
		r_end = at[1] + size * 0.5f;
	} else {
		return false;
	}
	return true;
}

} // namespace

float WowUI::effective_scale(const Widget &w) const {
	float scale = 1.0f;
	for (const Widget *each = &w; each; each = widget(each->parent)) {
		if (each->kind != KIND_TEXTURE && each->kind != KIND_FONT_STRING) {
			scale *= each->scale;
		}
	}
	return scale;
}

bool WowUI::resolve(Widget &w) {
	if (w.rect_version == layout_version) {
		return w.rect_valid;
	}
	if (w.resolving || w.kind == KIND_FONT) {
		return false;
	}
	w.resolving = true;
	bool valid = false;
	float s = effective_scale(w);

	Widget *owner = widget(w.bar_owner);
	if (owner && resolve(*owner)) {
		valid = true;
		float span = owner->max_value - owner->min_value;
		float fraction = span > 0.0f ? std::clamp((owner->value - owner->min_value) / span, 0.0f, 1.0f) : 0.0f;
		if (owner->kind == KIND_STATUS_BAR) {
			w.left = owner->left;
			w.bottom = owner->bottom;
			w.right = owner->vertical ? owner->right : owner->left + (owner->right - owner->left) * fraction;
			w.top = owner->vertical ? owner->bottom + (owner->top - owner->bottom) * fraction : owner->top;
		} else {
			float width = (w.has_width ? w.width : 16.0f) * s;
			float height = (w.has_height ? w.height : 16.0f) * s;
			Vector2 center;
			if (owner->vertical) {
				center = Vector2((owner->left + owner->right) * 0.5f, owner->top - height * 0.5f - (owner->top - owner->bottom - height) * fraction);
			} else {
				center = Vector2(owner->left + width * 0.5f + (owner->right - owner->left - width) * fraction, (owner->top + owner->bottom) * 0.5f);
			}
			w.left = center.x - width * 0.5f;
			w.right = center.x + width * 0.5f;
			w.bottom = center.y - height * 0.5f;
			w.top = center.y + height * 0.5f;
		}
	} else if (!owner) {
		std::vector<Anchor> anchors = w.anchors;
		bool region = w.kind == KIND_TEXTURE || w.kind == KIND_FONT_STRING;
		if (anchors.empty()) {
			Anchor a;
			if (w.scrolled_by >= 0) {
				a.relative = w.scrolled_by;
				anchors.push_back(a);
			} else if (!w.has_width && !w.has_height && (region || w.parent < 0)) {
				anchors.push_back(a);
				a.point = a.relative_point = BOTTOMRIGHT;
				anchors.push_back(a);
			} else if (region) {
				anchors.push_back(a);
			}
		}
		bool has_x[3] = { false, false, false }, has_y[3] = { false, false, false };
		float at_x[3] = { 0, 0, 0 }, at_y[3] = { 0, 0, 0 };
		bool ok = !anchors.empty();
		for (Anchor &a : anchors) {
			int relative = a.relative;
			if (relative == -3) {
				relative = find(a.relative_name);
				if (relative < 0) {
					relative = NO_REL;
				}
			}
			if (relative == -1) {
				relative = w.parent >= 0 ? w.parent : -2;
			}
			float rl = 0.0f, rb = 0.0f, rr = screen_width, rt = SCREEN_HEIGHT;
			if (relative == NO_REL) {
				relative = w.parent >= 0 ? w.parent : -2;
			}
			if (relative >= 0) {
				Widget *rel = widget(relative);
				if (!rel || !resolve(*rel)) {
					ok = false;
					break;
				}
				rl = rel->left;
				rb = rel->bottom;
				rr = rel->right;
				rt = rel->top;
			}
			Vector2 p = point_at(a.relative_point, rl, rb, rr, rt) + Vector2(a.x, a.y) * s;
			int h = horizontal_of(a.point), v = vertical_of(a.point);
			has_x[h] = true;
			at_x[h] = p.x;
			has_y[v] = true;
			at_y[v] = p.y;
		}
		if (ok) {
			bool text = w.kind == KIND_FONT_STRING;
			float width = w.width * s;
			if (text && !w.has_width && !(has_x[0] && has_x[2])) {
				width = text_width(w) * s;
			}
			ok = solve_axis(has_x, at_x, width, w.left, w.right);
			if (ok) {
				float height = w.height * s;
				if (text && !w.has_height && !(has_y[0] && has_y[2])) {
					bool wrapped = w.has_width || (has_x[0] && has_x[2]);
					TextLayout measure;
					int size = font_px(w.font, s);
					layout_text(measure, w.text, w.font, size, wrapped ? (w.right - w.left) * px : 0.0f, w.nonspacewrap, w.max_lines);
					height = static_cast<float>(measure.lines.size()) * measure.line_height / px;
				}
				ok = solve_axis(has_y, at_y, height, w.bottom, w.top);
			}
		}
		if (ok && w.scrolled_by >= 0) {
			if (Widget *scroll = widget(w.scrolled_by)) {
				float scroll_scale = effective_scale(*scroll);
				w.left -= scroll->scroll_h * scroll_scale;
				w.right -= scroll->scroll_h * scroll_scale;
				w.top += scroll->scroll_v * scroll_scale;
				w.bottom += scroll->scroll_v * scroll_scale;
			}
		}
		if (ok && w.clamped) {
			float dx = 0.0f, dy = 0.0f;
			if (w.right > screen_width) {
				dx = screen_width - w.right;
			}
			if (w.left + dx < 0.0f) {
				dx = -w.left;
			}
			if (w.top > SCREEN_HEIGHT) {
				dy = SCREEN_HEIGHT - w.top;
			}
			if (w.bottom + dy < 0.0f) {
				dy = -w.bottom;
			}
			w.left += dx;
			w.right += dx;
			w.top += dy;
			w.bottom += dy;
		}
		valid = ok;
	}
	w.resolving = false;
	w.rect_version = layout_version;
	w.rect_valid = valid;
	return valid;
}

bool WowUI::widget_rect(int id, float &r_left, float &r_bottom, float &r_right, float &r_top) {
	Widget *w = widget(id);
	if (!w || !resolve(*w)) {
		return false;
	}
	r_left = w->left;
	r_bottom = w->bottom;
	r_right = w->right;
	r_top = w->top;
	return true;
}

Rect2 WowUI::to_pixels(float left, float bottom, float right, float top) const {
	return Rect2(left * px, (SCREEN_HEIGHT - top) * px, (right - left) * px, (top - bottom) * px);
}

bool WowUI::screen_rect(int id, Rect2 &r_rect) {
	float l, b, r, t;
	if (!widget_rect(id, l, b, r, t)) {
		return false;
	}
	r_rect = to_pixels(l, b, r, t);
	return true;
}

// Visibility.

bool WowUI::parent_visible(const Widget &w) const {
	const Widget *parent = widget(w.parent);
	return !parent || parent->visible;
}

void WowUI::update_visibility(int id, bool fire) {
	std::vector<int> changed;
	std::vector<int> stack = { id };
	while (!stack.empty()) {
		Widget &w = *widgets[stack.back()];
		stack.pop_back();
		bool visible = w.kind != KIND_FONT && w.shown && parent_visible(w);
		if (visible == w.visible) {
			continue;
		}
		w.visible = visible;
		if (w.kind != KIND_TEXTURE && w.kind != KIND_FONT_STRING) {
			changed.push_back(w.id);
		}
		for (auto it = w.regions.rbegin(); it != w.regions.rend(); ++it) {
			stack.push_back(*it);
		}
		for (auto it = w.children.rbegin(); it != w.children.rend(); ++it) {
			stack.push_back(*it);
		}
	}
	for (int each : changed) {
		Widget &w = *widgets[each];
		if (!w.visible) {
			if (focus == each) {
				set_focus(-1);
			}
			if (moving == each) {
				stop_moving(each);
			}
			if (pressed == each) {
				pressed = -1;
			}
		}
		if (fire && w.visible == (w.shown && parent_visible(w))) {
			call_script(each, w.visible ? "OnShow" : "OnHide");
		}
		if (widgets[each]->visible && widgets[each]->kind == KIND_EDIT_BOX && widgets[each]->auto_focus) {
			set_focus(each);
		}
	}
}

void WowUI::set_shown(int id, bool shown) {
	Widget *w = widget(id);
	if (!w || w->shown == shown) {
		return;
	}
	w->shown = shown;
	update_visibility(id, true);
}

int WowUI::set_level(int id, int level) {
	Widget *w = widget(id);
	if (!w) {
		return 0;
	}
	level = std::max(0, level);
	int delta = level - w->level;
	if (delta == 0) {
		return level;
	}
	std::vector<int> stack = { id };
	while (!stack.empty()) {
		Widget &each = *widgets[stack.back()];
		stack.pop_back();
		each.level = std::max(0, each.level + delta);
		stack.insert(stack.end(), each.children.begin(), each.children.end());
	}
	return level;
}

void WowUI::raise(int id) {
	Widget *w = widget(id);
	if (!w) {
		return;
	}
	int highest = -1;
	for (const auto &other : widgets) {
		if (other->strata != w->strata || other->kind == KIND_TEXTURE || other->kind == KIND_FONT_STRING || other->kind == KIND_FONT || !other->visible) {
			continue;
		}
		bool inside = false;
		for (const Widget *p = other.get(); p; p = widget(p->parent)) {
			if (p->id == id) {
				inside = true;
				break;
			}
		}
		if (!inside) {
			highest = std::max(highest, other->level);
		}
	}
	if (w->level <= highest) {
		set_level(id, highest + 1);
	}
}

void WowUI::update_scroll_child(int id) {
	Widget &frame = *widgets[id];
	Widget *child = widget(frame.scroll_child);
	float l, b, r, t;
	if (!child || !widget_rect(id, l, b, r, t)) {
		return;
	}
	float cl, cb, cr, ct;
	if (!widget_rect(child->id, cl, cb, cr, ct)) {
		return;
	}
	float scale = effective_scale(frame);
	float range_h = std::max(0.0f, ((cr - cl) - (r - l)) / scale);
	float range_v = std::max(0.0f, ((ct - cb) - (t - b)) / scale);
	if (std::fabs(range_h - frame.range_h) < 0.01f && std::fabs(range_v - frame.range_v) < 0.01f) {
		return;
	}
	frame.range_h = range_h;
	frame.range_v = range_v;
	Variant args[2] = { range_h, range_v };
	call_script(id, "OnScrollRangeChanged", 2, args);
}

// Tooltips.

void WowUI::layout_tooltip(Widget &tooltip) {
	float widest = 0.0f;
	float height = 0.0f;
	std::vector<int> rights;
	std::vector<int> lefts;
	std::vector<int> wrapped;
	for (int i = 1; i <= tooltip.line_count; ++i) {
		String index = String::num_int64(i);
		int left = find(tooltip.name + "TextLeft" + index);
		int right = find(tooltip.name + "TextRight" + index);
		if (left < 0) {
			continue;
		}
		Widget &l = *widgets[left];
		if (l.tooltip_wrap) {
			wrapped.push_back(left);
			continue;
		}
		float line = text_width(l);
		if (Widget *r = widget(right); r && r->shown && !r->text.is_empty()) {
			line += text_width(*r) + 20.0f;
			rights.push_back(right);
			lefts.push_back(left);
		}
		widest = std::max(widest, line);
	}
	// Wrapped lines fill the width the other lines set, but never narrower than the stock tips read.
	float wrap_width = std::max(widest, std::max(TOOLTIP_WRAP_WIDTH, tooltip.min_width - 20.0f));
	for (int left : wrapped) {
		Widget &l = *widgets[left];
		float natural = text_width(l);
		l.has_width = natural > wrap_width;
		l.width = l.has_width ? wrap_width : 0.0f;
		reset_layout(l);
		widest = std::max(widest, std::min(natural, wrap_width));
	}
	for (int i = 1; i <= tooltip.line_count; ++i) {
		int left = find(tooltip.name + "TextLeft" + String::num_int64(i));
		if (left >= 0) {
			height += text_height(left) + (i > 1 ? 2.0f : 0.0f);
		}
	}
	float width = std::max(widest + 20.0f, tooltip.min_width);
	tooltip.width = width;
	tooltip.height = height + 20.0f;
	tooltip.has_width = tooltip.has_height = true;
	for (size_t i = 0; i < rights.size(); ++i) {
		Widget &r = *widgets[rights[i]];
		r.anchors.clear();
		Anchor a;
		a.point = RIGHT;
		a.relative = lefts[i];
		a.relative_point = LEFT;
		a.x = width - 20.0f;
		r.anchors.push_back(a);
	}
	invalidate();
}

void WowUI::anchor_tooltip(Widget &tooltip) {
	const String &mode = tooltip.owner_anchor;
	if (mode == "ANCHOR_NONE" || mode == "ANCHOR_PRESERVE") {
		return;
	}
	float l = 0.0f, b = 0.0f, r = 0.0f, t = 0.0f;
	bool cursor = mode == "ANCHOR_CURSOR";
	if (cursor) {
		Vector2 at = to_screen(mouse_position);
		l = r = at.x;
		b = t = at.y;
	} else if (!widget_rect(tooltip.owner, l, b, r, t)) {
		return;
	}
	float s = effective_scale(tooltip);
	int point = BOTTOMRIGHT;
	Vector2 at(l, t);
	if (mode == "ANCHOR_RIGHT") {
		point = BOTTOMLEFT;
		at = Vector2(r, t);
	} else if (mode == "ANCHOR_TOPLEFT") {
		point = BOTTOMLEFT;
		at = Vector2(l, t);
	} else if (mode == "ANCHOR_TOPRIGHT") {
		point = BOTTOMRIGHT;
		at = Vector2(r, t);
	} else if (mode == "ANCHOR_BOTTOMLEFT") {
		point = TOPRIGHT;
		at = Vector2(l, b);
	} else if (mode == "ANCHOR_BOTTOMRIGHT") {
		point = TOPLEFT;
		at = Vector2(r, b);
	} else if (mode == "ANCHOR_BOTTOM") {
		point = TOP;
		at = Vector2((l + r) * 0.5f, b);
	} else if (mode == "ANCHOR_TOP") {
		point = BOTTOM;
		at = Vector2((l + r) * 0.5f, t);
	} else if (cursor) {
		point = BOTTOMLEFT;
		at = Vector2(l + 8.0f, t + 8.0f);
	}
	at += tooltip.owner_offset * s;
	Anchor a;
	a.point = point;
	a.relative = -2;
	a.relative_point = BOTTOMLEFT;
	a.x = at.x / s;
	a.y = at.y / s;
	if (tooltip.anchors.size() == 1) {
		const Anchor &old = tooltip.anchors[0];
		if (old.point == a.point && old.relative == -2 && std::fabs(old.x - a.x) < 0.01f && std::fabs(old.y - a.y) < 0.01f) {
			return;
		}
	}
	tooltip.anchors.clear();
	tooltip.anchors.push_back(a);
	invalidate();
}

// Text.

Ref<Font> WowUI::fallback_font() {
	if (fallback.is_null()) {
		Ref<SystemFont> system;
		system.instantiate();
		PackedStringArray names;
		for (const char *name : { "PingFang SC", "Hiragino Sans GB", "Microsoft YaHei", "Noto Sans CJK SC", "Source Han Sans SC", "sans-serif" }) {
			names.push_back(name);
		}
		system->set_font_names(names);
		fallback = system;
	}
	return fallback;
}

Ref<Font> WowUI::font_file(const String &path) {
	std::string key = std::string(path.to_lower().utf8().get_data());
	auto found = fonts.find(key);
	if (found != fonts.end()) {
		return found->second;
	}
	Ref<Font> font;
	std::string bytes;
	if (!path.is_empty() && read_file(path, bytes) && !bytes.empty()) {
		Ref<FontFile> file;
		file.instantiate();
		PackedByteArray data;
		data.resize(static_cast<int64_t>(bytes.size()));
		std::memcpy(data.ptrw(), bytes.data(), bytes.size());
		file->set_data(data);
		TypedArray<Font> fallbacks;
		fallbacks.push_back(fallback_font());
		file->set_fallbacks(fallbacks);
		font = file;
	} else {
		if (!path.is_empty()) {
			report("no font " + path);
		}
		font = fallback_font();
	}
	fonts[key] = font;
	return font;
}

int WowUI::font_px(const FontInfo &font, float scale) const {
	return std::max(1, static_cast<int>(std::lround(font.height * scale * px)));
}

namespace {

struct Piece {
	String text;
	Color color;
	bool colored = false;
	int link = -1;
	bool newline = false;
	bool space = false;
};

bool is_wide(char32_t c) {
	return c >= 0x2E80;
}

int hex_digit(char32_t c) {
	if (c >= '0' && c <= '9') {
		return static_cast<int>(c - '0');
	}
	if (c >= 'a' && c <= 'f') {
		return static_cast<int>(c - 'a' + 10);
	}
	if (c >= 'A' && c <= 'F') {
		return static_cast<int>(c - 'A' + 10);
	}
	return -1;
}

// Splits interface markup into pieces a line may break between: words, single spaces and wide characters.
std::vector<Piece> pieces_of(const String &text, std::vector<String> &r_links) {
	std::vector<Piece> out;
	Color color;
	bool colored = false;
	int link = -1;
	auto add_char = [&](char32_t c) {
		bool space = c == ' ' || c == '\t';
		bool wide = is_wide(c);
		if (!out.empty() && !space && !wide && !out.back().newline && !out.back().space && out.back().link == link && out.back().colored == colored && (!colored || out.back().color == color) && !is_wide(out.back().text[out.back().text.length() - 1])) {
			out.back().text += String::chr(c);
			return;
		}
		Piece p;
		p.text = String::chr(space ? ' ' : c);
		p.color = color;
		p.colored = colored;
		p.link = link;
		p.space = space;
		out.push_back(p);
	};
	int64_t n = text.length();
	for (int64_t i = 0; i < n; ++i) {
		char32_t c = text[i];
		if (c == '|' && i + 1 < n) {
			char32_t d = text[i + 1];
			if ((d == 'c' || d == 'C') && i + 9 < n) {
				int v[8];
				bool ok = true;
				for (int k = 0; k < 8; ++k) {
					v[k] = hex_digit(text[i + 2 + k]);
					ok = ok && v[k] >= 0;
				}
				if (ok) {
					color = Color((v[2] * 16 + v[3]) / 255.0f, (v[4] * 16 + v[5]) / 255.0f, (v[6] * 16 + v[7]) / 255.0f, 1.0f);
					colored = true;
					i += 9;
					continue;
				}
			} else if (d == 'r' || d == 'R') {
				colored = false;
				++i;
				continue;
			} else if (d == 'H') {
				int64_t end = text.find("|h", i + 2);
				if (end >= 0) {
					r_links.push_back(text.substr(i + 2, end - i - 2));
					link = static_cast<int>(r_links.size()) - 1;
					i = end + 1;
					continue;
				}
			} else if (d == 'h') {
				link = -1;
				++i;
				continue;
			} else if (d == 'n') {
				Piece p;
				p.newline = true;
				out.push_back(p);
				++i;
				continue;
			} else if (d == '|') {
				add_char('|');
				++i;
				continue;
			} else if (d == 'T') {
				int64_t end = text.find("|t", i + 2);
				if (end >= 0) {
					i = end + 1;
					continue;
				}
			}
		}
		if (c == '\n') {
			Piece p;
			p.newline = true;
			out.push_back(p);
			continue;
		}
		if (c == '\r') {
			continue;
		}
		add_char(c);
	}
	return out;
}

} // namespace

void WowUI::layout_text(TextLayout &layout, const String &text, const FontInfo &font, int size, float max_width, bool nonspacewrap, int max_lines) {
	layout.lines.clear();
	layout.links.clear();
	layout.font_px = size;
	layout.max_width = max_width;
	Ref<Font> f = font_file(font.file);
	layout.line_height = static_cast<float>(f->get_height(size)) + font.spacing * px;
	std::vector<Piece> pieces = pieces_of(text, layout.links);
	layout.lines.emplace_back();
	auto measure = [&](const String &s) {
		return static_cast<float>(f->get_string_size(s, HORIZONTAL_ALIGNMENT_LEFT, -1, size).x);
	};
	auto append = [&](const Piece &p, const String &s, float width) {
		TextLine &line = layout.lines.back();
		if (!line.runs.empty()) {
			TextRun &last = line.runs.back();
			if (last.link == p.link && last.colored == p.colored && (!p.colored || last.color == p.color)) {
				last.text += s;
				line.width += width;
				return;
			}
		}
		TextRun run;
		run.text = s;
		run.color = p.color;
		run.colored = p.colored;
		run.link = p.link;
		line.runs.push_back(run);
		line.width += width;
	};
	for (const Piece &p : pieces) {
		if (p.newline) {
			layout.lines.emplace_back();
			continue;
		}
		float width = measure(p.text);
		TextLine &line = layout.lines.back();
		if (max_width > 0.0f && line.width + width > max_width + 0.5f && !line.runs.empty()) {
			if (p.space) {
				continue;
			}
			// Trailing spaces do not count toward the broken line.
			TextRun &last = line.runs.back();
			while (last.text.ends_with(" ")) {
				last.text = last.text.substr(0, last.text.length() - 1);
				line.width -= measure(" ");
			}
			layout.lines.emplace_back();
		}
		if (max_width > 0.0f && width > max_width && nonspacewrap) {
			for (int64_t i = 0; i < p.text.length(); ++i) {
				String c = p.text.substr(i, 1);
				float cw = measure(c);
				if (layout.lines.back().width + cw > max_width + 0.5f && !layout.lines.back().runs.empty()) {
					layout.lines.emplace_back();
				}
				append(p, c, cw);
			}
			continue;
		}
		if (p.space && layout.lines.back().runs.empty() && max_width > 0.0f && layout.lines.size() > 1) {
			continue;
		}
		append(p, p.text, width);
	}
	if (max_lines > 0 && static_cast<int>(layout.lines.size()) > max_lines) {
		layout.lines.resize(static_cast<size_t>(max_lines));
	}
	if (layout.lines.size() == 1 && layout.lines[0].runs.empty() && text.is_empty()) {
		layout.lines.clear();
	}
}

void WowUI::layout_text_widget(Widget &w) {
	float s = effective_scale(w);
	int size = font_px(w.font, s);
	float max_width = 0.0f;
	if (w.kind == KIND_FONT_STRING || w.kind == KIND_SIMPLE_HTML) {
		bool fixed = w.has_width;
		if (!fixed) {
			int left = 0, right = 0;
			for (const Anchor &a : w.anchors) {
				int h = horizontal_of(a.point);
				left += h == 0;
				right += h == 2;
			}
			fixed = left > 0 && right > 0;
		}
		if (fixed && resolve(w)) {
			max_width = (w.right - w.left) * px;
		}
	}
	String shown = w.text;
	if (w.kind == KIND_EDIT_BOX && w.password) {
		shown = String("*").repeat(static_cast<int>(w.text.length()));
	}
	String key = shown + "|" + String::num_int64(size) + "|" + String::num(max_width, 1) + "|" + w.font.file + "|" + String::num(w.font.spacing) + "|" + String::num_int64(w.max_lines);
	if (w.layout.key == key) {
		return;
	}
	bool wrap = w.kind != KIND_EDIT_BOX || w.multi_line;
	layout_text(w.layout, shown, w.font, size, wrap ? max_width : 0.0f, w.nonspacewrap, w.max_lines);
	w.layout.key = key;
}

float WowUI::text_width(Widget &w) {
	if (w.text.is_empty()) {
		return 0.0f;
	}
	float s = effective_scale(w);
	TextLayout measure;
	layout_text(measure, w.text, w.font, font_px(w.font, s), 0.0f, false, 0);
	float widest = 0.0f;
	for (const TextLine &line : measure.lines) {
		widest = std::max(widest, line.width);
	}
	return widest / (px * s);
}

float WowUI::text_height(int id) {
	Widget *w = widget(id);
	if (!w || w->text.is_empty()) {
		return 0.0f;
	}
	layout_text_widget(*w);
	float s = effective_scale(*w);
	return static_cast<float>(w->layout.lines.size()) * w->layout.line_height / (px * s);
}

} // namespace godot
