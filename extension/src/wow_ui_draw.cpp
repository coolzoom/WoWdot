#include "wow_ui.h"

#include "pipeline/blp_loader.hpp"

#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace godot {

namespace {

const char *DESATURATE = R"(
shader_type canvas_item;
void fragment() {
	float grey = dot(COLOR.rgb, vec3(0.299, 0.587, 0.114));
	COLOR.rgb = vec3(grey);
}
)";

} // namespace

void WowUI::begin_draw() {
	RenderingServer *rs = RenderingServer::get_singleton();
	for (int i = 0; i < item_count; ++i) {
		rs->canvas_item_clear(items[i]);
	}
	item_count = 0;
	item_open = false;
	if (materials[1].is_null()) {
		Ref<CanvasItemMaterial> add;
		add.instantiate();
		add->set_blend_mode(CanvasItemMaterial::BLEND_MODE_ADD);
		materials[1] = add;
		Ref<CanvasItemMaterial> mul;
		mul.instantiate();
		mul->set_blend_mode(CanvasItemMaterial::BLEND_MODE_MUL);
		materials[2] = mul;
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code(DESATURATE);
		Ref<ShaderMaterial> grey;
		grey.instantiate();
		grey->set_shader(shader);
		materials[3] = grey;
	}
}

void WowUI::end_draw() {
	item_open = false;
}

RID WowUI::item(const DrawState &state) {
	if (item_open && item_state == state) {
		return items[item_count - 1];
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (item_count == static_cast<int>(items.size())) {
		RID rid = rs->canvas_item_create();
		rs->canvas_item_set_parent(rid, get_canvas_item());
		rs->canvas_item_set_default_texture_filter(rid, RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_LINEAR);
		items.push_back(rid);
	}
	RID rid = items[item_count];
	rs->canvas_item_set_draw_index(rid, item_count);
	++item_count;
	rs->canvas_item_set_material(rid, state.material > 0 ? materials[state.material]->get_rid() : RID());
	rs->canvas_item_set_default_texture_repeat(rid, state.repeat ? RenderingServer::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED : RenderingServer::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	rs->canvas_item_set_clip(rid, state.clip);
	rs->canvas_item_set_custom_rect(rid, state.clip, state.clip_rect);
	item_state = state;
	item_open = true;
	return rid;
}

void WowUI::collect_draw(std::vector<int> &out) const {
	out.clear();
	for (const auto &w : widgets) {
		if (w->visible && w->kind != KIND_TEXTURE && w->kind != KIND_FONT_STRING && w->kind != KIND_FONT) {
			out.push_back(w->id);
		}
	}
	std::sort(out.begin(), out.end(), [this](int a, int b) {
		const Widget &x = *widgets[a];
		const Widget &y = *widgets[b];
		if (x.strata != y.strata) {
			return x.strata < y.strata;
		}
		if (x.level != y.level) {
			return x.level < y.level;
		}
		return a < b;
	});
}

float WowUI::alpha_of(const Widget &w) const {
	float alpha = 1.0f;
	for (const Widget *each = &w; each; each = widget(each->parent)) {
		alpha *= each->alpha;
	}
	return alpha;
}

bool WowUI::clip_of(const Widget &w, Rect2 &r_clip) {
	bool clipped = false;
	for (const Widget *each = &w; each; each = widget(each->parent)) {
		if (each->scrolled_by < 0) {
			continue;
		}
		Rect2 rect;
		if (!screen_rect(each->scrolled_by, rect)) {
			continue;
		}
		r_clip = clipped ? r_clip.intersection(rect) : rect;
		clipped = true;
	}
	return clipped;
}

bool WowUI::region_drawn(const Widget &region, const Widget &frame) const {
	int id = region.id;
	bool hot = (hovered == frame.id && frame.enabled) || frame.locked_highlight;
	if (region.layer == LAYER_HIGHLIGHT && !hot) {
		return false;
	}
	if (frame.kind != KIND_BUTTON && frame.kind != KIND_CHECK_BUTTON) {
		return true;
	}
	bool pushed = frame.pushed && frame.enabled;
	if (id == frame.normal_texture) {
		return frame.enabled ? !(pushed && frame.pushed_texture >= 0) : frame.disabled_texture < 0;
	}
	if (id == frame.pushed_texture) {
		return pushed;
	}
	if (id == frame.disabled_texture) {
		return !frame.enabled;
	}
	if (id == frame.checked_texture) {
		return frame.checked && (frame.enabled || frame.disabled_checked_texture < 0);
	}
	if (id == frame.disabled_checked_texture) {
		return frame.checked && !frame.enabled;
	}
	return true;
}

void WowUI::draw_frame(Widget &w, const DrawState &base) {
	DrawState state = base;
	state.clip = clip_of(w, state.clip_rect);
	Rect2 rect;
	bool placed = screen_rect(w.id, rect);
	if (state.clip && placed && !state.clip_rect.intersects(rect)) {
		return;
	}
	if (placed && w.backdrop) {
		draw_backdrop(w, rect, state);
	}
	if (placed && w.external.is_valid()) {
		Color white(1, 1, 1, alpha_of(w));
		Color colors[4] = { white, white, white, white };
		float coords[8] = { 0, 0, 0, 1, 1, 0, 1, 1 };
		draw_quad(item(state), rect, coords, colors, w.external);
	}
	for (int layer = 0; layer < LAYER_COUNT; ++layer) {
		for (int region_id : w.regions) {
			Widget &region = *widgets[region_id];
			if (region.layer == layer && region.visible && region_drawn(region, w)) {
				draw_region(region, w, state);
			}
		}
	}
	if (!placed) {
		return;
	}
	switch (w.kind) {
		case KIND_EDIT_BOX:
			draw_edit_box(w, rect, state);
			break;
		case KIND_SCROLLING_MESSAGE_FRAME:
		case KIND_MESSAGE_FRAME:
			draw_messages(w, rect, state);
			break;
		case KIND_SIMPLE_HTML:
			if (!w.text.is_empty()) {
				layout_text_widget(w);
				draw_text(w.layout, w.font, rect, alpha_of(w), state, w.font.justify_h, JUSTIFY_START);
			}
			break;
		case KIND_MODEL:
			if (w.cooldown_model) {
				draw_cooldown(w, rect, state);
			}
			break;
		default:
			break;
	}
}

void WowUI::draw_region(Widget &region, Widget &frame, const DrawState &base) {
	Rect2 rect;
	if (!screen_rect(region.id, rect)) {
		return;
	}
	if (region.kind == KIND_TEXTURE) {
		draw_texture(region, rect, base);
		return;
	}
	if (region.kind != KIND_FONT_STRING || region.text.is_empty()) {
		return;
	}
	layout_text_widget(region);
	float alpha = alpha_of(region);
	Rect2 at = rect;
	if (region.id == frame.font_string && frame.pushed && frame.enabled) {
		float s = effective_scale(frame) * px;
		at.position += Vector2(frame.pushed_offset.x, -frame.pushed_offset.y) * s;
	}
	FontInfo font = region.font;
	font.color *= region.vertex_color;
	draw_text(region.layout, font, at, alpha, base, font.justify_h, font.justify_v);
}

void WowUI::draw_texture(Widget &t, const Rect2 &rect, const DrawState &base) {
	if (!t.solid && t.texture.is_null()) {
		return;
	}
	DrawState state = base;
	state.material = t.desaturated ? 3 : t.blend == BLEND_ADD ? 1 : t.blend == BLEND_MOD ? 2 : 0;
	float coords[8];
	std::memcpy(coords, t.coords, sizeof(coords));
	Rect2 at = rect;
	if (Widget *owner = widget(t.bar_owner); owner && owner->kind == KIND_STATUS_BAR) {
		// The bar's texture is cut, not squeezed, as it empties.
		float span = owner->max_value - owner->min_value;
		float fraction = span > 0.0f ? std::clamp((owner->value - owner->min_value) / span, 0.0f, 1.0f) : 0.0f;
		if (fraction <= 0.0f) {
			return;
		}
		if (owner->vertical) {
			float top_v = coords[3] + (coords[1] - coords[3]) * fraction;
			coords[1] = coords[5] = top_v;
		} else {
			float right_u = coords[0] + (coords[4] - coords[0]) * fraction;
			coords[4] = coords[6] = right_u;
		}
	}
	for (float c : coords) {
		if (c < -0.001f || c > 1.001f) {
			state.repeat = true;
		}
	}
	Color tint = (t.solid ? t.solid_color : Color(1, 1, 1, 1)) * t.vertex_color;
	tint.a *= alpha_of(t);
	Color colors[4] = { tint, tint, tint, tint }; // TL, TR, BR, BL
	if (t.gradient) {
		Color low = tint * t.gradient_min;
		Color high = tint * t.gradient_max;
		if (t.gradient_vertical) {
			colors[0] = colors[1] = high;
			colors[2] = colors[3] = low;
		} else {
			colors[0] = colors[3] = low;
			colors[1] = colors[2] = high;
		}
	}
	draw_quad(item(state), at, coords, colors, t.solid ? Ref<Texture2D>() : t.texture);
}

void WowUI::draw_quad(RID rid, const Rect2 &rect, const float *coords, const Color colors[4], const Ref<Texture2D> &texture) {
	PackedVector2Array points;
	points.push_back(rect.position);
	points.push_back(Vector2(rect.position.x + rect.size.x, rect.position.y));
	points.push_back(rect.position + rect.size);
	points.push_back(Vector2(rect.position.x, rect.position.y + rect.size.y));
	PackedColorArray tints;
	for (int i = 0; i < 4; ++i) {
		tints.push_back(colors[i]);
	}
	PackedVector2Array uvs;
	uvs.push_back(Vector2(coords[0], coords[1])); // UL
	uvs.push_back(Vector2(coords[4], coords[5])); // UR
	uvs.push_back(Vector2(coords[6], coords[7])); // LR
	uvs.push_back(Vector2(coords[2], coords[3])); // LL
	RenderingServer::get_singleton()->canvas_item_add_primitive(rid, points, tints, uvs, texture.is_valid() ? texture->get_rid() : RID());
}

void WowUI::draw_backdrop(Widget &w, const Rect2 &rect, const DrawState &base) {
	Backdrop &b = *w.backdrop;
	float s = effective_scale(w) * px;
	float alpha = alpha_of(w);
	if (!b.bg_file.is_empty()) {
		Ref<Texture2D> bg = load_texture(b.bg_file);
		if (bg.is_valid()) {
			Rect2 inner(rect.position + Vector2(b.inset_left, b.inset_top) * s, rect.size - Vector2(b.inset_left + b.inset_right, b.inset_top + b.inset_bottom) * s);
			float u = 1.0f, v = 1.0f;
			DrawState state = base;
			if (b.tile && b.tile_size > 0.0f) {
				u = inner.size.x / (b.tile_size * s);
				v = inner.size.y / (b.tile_size * s);
				state.repeat = true;
			}
			float coords[8] = { 0, 0, 0, v, u, 0, u, v };
			Color tint = b.color;
			tint.a *= alpha;
			Color colors[4] = { tint, tint, tint, tint };
			draw_quad(item(state), inner, coords, colors, bg);
		}
	}
	if (b.edge_file.is_empty()) {
		return;
	}
	Ref<Texture2D> edge = load_texture(b.edge_file);
	if (edge.is_null()) {
		return;
	}
	float e = (b.edge_size > 0.0f ? b.edge_size : static_cast<float>(edge->get_height())) * s;
	e = std::min(e, std::min(rect.size.x, rect.size.y) * 0.5f);
	if (e <= 0.0f) {
		return;
	}
	Color tint = b.border_color;
	tint.a *= alpha;
	Color colors[4] = { tint, tint, tint, tint };
	RID rid = item(base);
	// The edge file is eight squares in a row: left, right, top, bottom, then the four corners.
	auto piece = [&](int index, const Rect2 &at, bool rotated, float length) {
		float u0 = index / 8.0f, u1 = (index + 1) / 8.0f;
		if (rotated) {
			// The top and bottom squares are stored turned a quarter, so the run's length walks v.
			float coords[8] = { u0, 1.0f, u1, 1.0f, u0, 1.0f - length, u1, 1.0f - length };
			draw_quad(rid, at, coords, colors, edge);
		} else {
			float coords[8] = { u0, 0.0f, u0, length, u1, 0.0f, u1, length };
			draw_quad(rid, at, coords, colors, edge);
		}
	};
	auto run = [&](int index, const Rect2 &span, bool horizontal) {
		float total = horizontal ? span.size.x : span.size.y;
		for (float done = 0.0f; done < total - 0.01f; done += e) {
			float step = std::min(e, total - done);
			Rect2 at = horizontal ? Rect2(span.position.x + done, span.position.y, step, e) : Rect2(span.position.x, span.position.y + done, e, step);
			piece(index, at, horizontal, step / e);
		}
	};
	Vector2 p = rect.position;
	Vector2 size = rect.size;
	run(0, Rect2(p.x, p.y + e, e, size.y - 2 * e), false);
	run(1, Rect2(p.x + size.x - e, p.y + e, e, size.y - 2 * e), false);
	run(2, Rect2(p.x + e, p.y, size.x - 2 * e, e), true);
	run(3, Rect2(p.x + e, p.y + size.y - e, size.x - 2 * e, e), true);
	piece(4, Rect2(p.x, p.y, e, e), false, 1.0f);
	piece(5, Rect2(p.x + size.x - e, p.y, e, e), false, 1.0f);
	piece(6, Rect2(p.x, p.y + size.y - e, e, e), false, 1.0f);
	piece(7, Rect2(p.x + size.x - e, p.y + size.y - e, e, e), false, 1.0f);
}

void WowUI::draw_text(const TextLayout &layout, const FontInfo &font, const Rect2 &rect, float alpha, const DrawState &base, int justify_h, int justify_v, float y_offset) {
	if (layout.lines.empty()) {
		return;
	}
	Ref<Font> f = font_file(font.file);
	int size = layout.font_px;
	float total = static_cast<float>(layout.lines.size()) * layout.line_height;
	float y = rect.position.y + y_offset;
	if (justify_v == JUSTIFY_MIDDLE) {
		y += (rect.size.y - total) * 0.5f;
	} else if (justify_v == JUSTIFY_END) {
		y += rect.size.y - total;
	}
	float ascent = static_cast<float>(f->get_ascent(size));
	float scale = static_cast<float>(size) / std::max(1.0f, font.height);
	Vector2 shadow = Vector2(font.shadow_offset.x, -font.shadow_offset.y) * scale;
	int outline = font.outline ? std::max(1, static_cast<int>(std::lround(scale * (font.thick ? 2.0f : 1.0f)))) : 0;
	RID rid = item(base);
	for (const TextLine &line : layout.lines) {
		float x = rect.position.x;
		if (justify_h == JUSTIFY_MIDDLE) {
			x += (rect.size.x - line.width) * 0.5f;
		} else if (justify_h == JUSTIFY_END) {
			x += rect.size.x - line.width;
		}
		x = std::floor(x + 0.5f);
		float baseline = std::floor(y + ascent + 0.5f);
		for (const TextRun &run : line.runs) {
			Color color = run.colored ? Color(run.color.r, run.color.g, run.color.b, font.color.a) : font.color;
			color.a *= alpha;
			Vector2 at(x, baseline);
			if (font.shadow.a > 0.0f && shadow != Vector2()) {
				Color shade = font.shadow;
				shade.a *= color.a;
				f->draw_string(rid, at + shadow, run.text, HORIZONTAL_ALIGNMENT_LEFT, -1, size, shade);
			}
			if (outline > 0) {
				f->draw_string_outline(rid, at, run.text, HORIZONTAL_ALIGNMENT_LEFT, -1, size, outline * 2, Color(0, 0, 0, color.a));
			}
			f->draw_string(rid, at, run.text, HORIZONTAL_ALIGNMENT_LEFT, -1, size, color);
			x += static_cast<float>(f->get_string_size(run.text, HORIZONTAL_ALIGNMENT_LEFT, -1, size).x);
		}
		y += layout.line_height;
	}
}

void WowUI::draw_edit_box(Widget &w, const Rect2 &rect, const DrawState &base) {
	float s = effective_scale(w) * px;
	Rect2 area(rect.position + Vector2(w.text_left, w.text_top) * s, rect.size - Vector2(w.text_left + w.text_right, w.text_top + w.text_bottom) * s);
	DrawState state = base;
	state.clip = true;
	state.clip_rect = base.clip ? base.clip_rect.intersection(rect) : rect;
	layout_text_widget(w);
	float alpha = alpha_of(w);
	bool focused = focus == w.id;
	float width = w.layout.lines.empty() ? 0.0f : w.layout.lines[0].width;
	Ref<Font> f = font_file(w.font.file);
	int size = w.layout.font_px > 0 ? w.layout.font_px : font_px(w.font, effective_scale(w));
	String shown = w.password ? String("*").repeat(static_cast<int>(w.text.length())) : w.text;
	float caret = static_cast<float>(f->get_string_size(shown.substr(0, w.cursor), HORIZONTAL_ALIGNMENT_LEFT, -1, size).x);
	float shift = 0.0f;
	if (!w.multi_line && caret > area.size.x - 2.0f) {
		shift = caret - area.size.x + 2.0f;
	}
	Rect2 text_rect(area.position - Vector2(shift, 0.0f), Vector2(std::max(area.size.x, width), area.size.y));
	int justify_h = w.multi_line || shift > 0.0f ? JUSTIFY_START : w.font.justify_h;
	if (w.select_all && focused && !w.text.is_empty()) {
		Color mark(1, 1, 1, 0.35f * alpha);
		Color colors[4] = { mark, mark, mark, mark };
		float coords[8] = { 0, 0, 0, 1, 1, 0, 1, 1 };
		float line = static_cast<float>(f->get_height(size));
		Rect2 at(text_rect.position.x, area.position.y + (w.multi_line ? 0.0f : (area.size.y - line) * 0.5f), width, line);
		draw_quad(item(state), at, coords, colors, Ref<Texture2D>());
	}
	draw_text(w.layout, w.font, text_rect, alpha, state, justify_h, w.multi_line ? JUSTIFY_START : JUSTIFY_MIDDLE);
	if (focused && std::fmod(caret_time, 1.0) < 0.5) {
		float line = static_cast<float>(f->get_height(size));
		float x = text_rect.position.x + caret;
		if (justify_h == JUSTIFY_MIDDLE) {
			x += (area.size.x - width) * 0.5f;
		} else if (justify_h == JUSTIFY_END) {
			x += area.size.x - width;
		}
		float y = w.multi_line ? area.position.y : area.position.y + (area.size.y - line) * 0.5f;
		Color bar = w.font.color;
		bar.a *= alpha;
		Color colors[4] = { bar, bar, bar, bar };
		float coords[8] = { 0, 0, 0, 1, 1, 0, 1, 1 };
		draw_quad(item(state), Rect2(x, y, std::max(1.0f, px), line), coords, colors, Ref<Texture2D>());
	}
}

void WowUI::draw_messages(Widget &w, const Rect2 &rect, const DrawState &base) {
	if (w.messages.empty()) {
		return;
	}
	DrawState state = base;
	state.clip = true;
	state.clip_rect = base.clip ? base.clip_rect.intersection(rect) : rect;
	float scale = effective_scale(w);
	int size = font_px(w.font, scale);
	float alpha = alpha_of(w);
	float width = rect.size.x;
	auto prepare = [&](Message &m) {
		String key = String::num_int64(size) + "|" + String::num(width, 1) + "|" + w.font.file;
		if (m.layout.key != key) {
			FontInfo font = w.font;
			String text = m.text;
			layout_text(m.layout, text, font, size, width, true, 0);
			m.layout.key = key;
		}
	};
	auto fade = [&](const Message &m) {
		if (!w.fading || w.scroll_offset > 0) {
			return 1.0f;
		}
		double age = now - m.time;
		if (age < w.time_visible) {
			return 1.0f;
		}
		return std::max(0.0f, 1.0f - static_cast<float>((age - w.time_visible) / std::max(0.01f, w.fade_duration)));
	};
	auto draw_one = [&](Message &m, float top) {
		FontInfo font = w.font;
		font.color = Color(m.color.r, m.color.g, m.color.b, 1.0f);
		float height = static_cast<float>(m.layout.lines.size()) * m.layout.line_height;
		draw_text(m.layout, font, Rect2(rect.position.x, top, width, height), alpha * fade(m), state, w.font.justify_h, JUSTIFY_START);
	};
	if (w.insert_top) {
		float y = rect.position.y;
		for (Message &m : w.messages) {
			if (fade(m) <= 0.0f) {
				continue;
			}
			prepare(m);
			draw_one(m, y);
			y += static_cast<float>(m.layout.lines.size()) * m.layout.line_height;
			if (y > rect.position.y + rect.size.y) {
				break;
			}
		}
		return;
	}
	float bottom = rect.position.y + rect.size.y;
	int skip = w.scroll_offset;
	for (auto it = w.messages.rbegin(); it != w.messages.rend(); ++it) {
		if (skip > 0) {
			--skip;
			continue;
		}
		Message &m = *it;
		if (fade(m) <= 0.0f) {
			break;
		}
		prepare(m);
		float height = static_cast<float>(m.layout.lines.size()) * m.layout.line_height;
		bottom -= height;
		draw_one(m, bottom);
		if (bottom < rect.position.y) {
			break;
		}
	}
}

void WowUI::draw_cooldown(Widget &w, const Rect2 &rect, const DrawState &base) {
	float done = std::clamp(w.cooldown_progress, 0.0f, 1.0f);
	if (done >= 1.0f) {
		return;
	}
	Vector2 center = rect.get_center();
	Vector2 half = rect.size * 0.5f;
	PackedVector2Array points;
	points.push_back(center);
	constexpr float TAU = 6.2831853f;
	int steps = std::max(2, static_cast<int>((1.0f - done) * 48.0f));
	for (int i = 0; i <= steps; ++i) {
		float angle = TAU * (done + (1.0f - done) * static_cast<float>(i) / static_cast<float>(steps));
		Vector2 dir(std::sin(angle), -std::cos(angle));
		float reach = 1.0f / std::max(std::fabs(dir.x), std::fabs(dir.y));
		points.push_back(center + Vector2(dir.x * half.x, dir.y * half.y) * reach);
	}
	PackedColorArray colors;
	colors.push_back(Color(0, 0, 0, 0.65f * alpha_of(w)));
	RenderingServer::get_singleton()->canvas_item_add_polygon(item(base), points, colors);
}

Ref<Texture2D> WowUI::load_texture(const String &path) {
	String file = path.replace("/", "\\");
	if (file.get_extension().is_empty()) {
		file += ".blp";
	}
	std::string key = std::string(file.to_lower().utf8().get_data());
	auto found = textures.find(key);
	if (found != textures.end()) {
		return found->second;
	}
	std::string bytes;
	Ref<Image> image;
	bool read = read_file(file, bytes);
	if (!read && file.to_lower().ends_with(".blp")) {
		// Addons often name a .tga without its extension.
		file = file.get_basename() + ".tga";
		read = read_file(file, bytes);
	}
	if (read) {
		if (file.to_lower().ends_with(".tga")) {
			PackedByteArray data;
			data.resize(static_cast<int64_t>(bytes.size()));
			std::memcpy(data.ptrw(), bytes.data(), bytes.size());
			image.instantiate();
			if (image->load_tga_from_buffer(data) != OK) {
				image.unref();
			}
		} else {
			std::vector<uint8_t> data(bytes.begin(), bytes.end());
			wowee::pipeline::BLPImage blp = wowee::pipeline::BLPLoader::load(data);
			if (blp.isValid()) {
				PackedByteArray pixels;
				pixels.resize(static_cast<int64_t>(blp.data.size()));
				std::memcpy(pixels.ptrw(), blp.data.data(), blp.data.size());
				image = Image::create_from_data(blp.width, blp.height, false, Image::FORMAT_RGBA8, pixels);
			}
		}
	}
	Ref<Texture2D> texture;
	if (image.is_valid()) {
		texture = ImageTexture::create_from_image(image);
	} else {
		note_missing("texture " + key);
	}
	textures[key] = texture;
	return texture;
}

} // namespace godot
