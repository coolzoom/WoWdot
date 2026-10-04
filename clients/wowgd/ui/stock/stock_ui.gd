## The game's own interface: Blizzard's XML and Lua read from the MPQs and run by WowUI.
## Subclasses load GlueXML or FrameXML and answer the engine functions those files call.
class_name StockUI
extends WowUI

const SETTING: String = "wowdot/interface/stock_ui"
# `--godot-ui` keeps the converted scenes for one run.
const OPT_OUT: String = "--godot-ui"
const MODEL_FRAME: PackedScene = preload("res://ui/wow/wow_model_frame.tscn")
# Interface files name .mdx models; the 1.12 archives ship them as .m2.
const MDX: String = ".mdx"
const M2: String = ".m2"

var _models: Dictionary[int, WowModelFrame] = {}
var _model_holder: Control


static func enabled() -> bool:
	if OPT_OUT in OS.get_cmdline_args() or OPT_OUT in OS.get_cmdline_user_args():
		return false
	return ProjectSettings.get_setting(SETTING, false)


static func model_path(path: String) -> String:
	return path.get_basename() + M2 if path.to_lower().ends_with(MDX) else path


func _init() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	archive = WowAssets.archive
	disk_root = WowLoader.client_data_dir().simplify_path().get_base_dir()
	# The frames render here, unseen, and the interface draws their pictures at its own layer.
	_model_holder = Control.new()
	_model_holder.modulate = Color(1, 1, 1, 0)
	_model_holder.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_model_holder.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(_model_holder)
	model_called.connect(_on_model_called)
	register_function("GetLocale", func() -> String: return WowAssets.video.locale)
	register_function("PlaySound", _play_sound)
	register_function("PlayMusic", func(path: String) -> void: WowAssets.audio.play_music(path))
	register_function("StopMusic", WowAssets.audio.stop_music)
	register_function("LaunchURL", func(url: String) -> void: OS.shell_open(url))
	register_function("Screenshot", _screenshot)
	register_function("QuitGame", _quit)
	register_function("ForceQuit", _quit)
	register_function("Quit", _quit)
	register_function("ShowCursor", _nothing)
	register_function("HideCursor", _nothing)


func _process(_delta: float) -> void:
	_place_models()


# Keys reach the focused edit box, then keyboard frames, then the bindings.
func _input(event: InputEvent) -> void:
	if event is InputEventKey and is_visible_in_tree() and handle_key(event):
		get_viewport().set_input_as_handled()


func handle_key(event: InputEventKey) -> bool:
	return key_event(event)


# Errors from Blizzard's or an addon's Lua are reported once each, as the stock client does.
func report_errors() -> void:
	for error: String in get_errors():
		push_warning("Interface: " + error)


func model_frame(id: int) -> WowModelFrame:
	if not _models.has(id):
		var frame: WowModelFrame = MODEL_FRAME.instantiate()
		frame.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_model_holder.add_child(frame)
		_models[id] = frame
		set_widget_texture_id(id, frame.texture)
	return _models[id]


func model_frame_named(widget: String) -> WowModelFrame:
	var id: int = get_widget_id(widget)
	return model_frame(id) if id >= 0 else null


# Hidden frames stop rendering; shown ones follow the rect the interface lays out for them.
func _place_models() -> void:
	if _models.is_empty():
		return
	for entry: Dictionary in get_model_frames():
		var frame: WowModelFrame = _models.get(entry["id"])
		if frame == null:
			continue
		frame.visible = entry["visible"] and entry["has_rect"]
		if frame.visible:
			var rect: Rect2 = entry["rect"]
			frame.position = rect.position
			frame.size = rect.size
		set_widget_texture_id(entry["id"], frame.texture)


func _on_model_called(id: int, method: String, args: Array) -> void:
	match method:
		"SetModel":
			model_frame(id).model_file = model_path(str(args[0])) if not args.is_empty() else ""
		"ClearModel":
			if _models.has(id):
				_models[id].model_file = ""
				_models[id].show_character(null)
		"SetFogColor":
			var frame: WowModelFrame = model_frame(id)
			frame.set_fog(Color(args[0], args[1], args[2]), frame.fog_near, frame.fog_far)
		"SetFogNear":
			var frame: WowModelFrame = model_frame(id)
			frame.set_fog(frame.fog_color, args[0], frame.fog_far)
		"SetFogFar":
			var frame: WowModelFrame = model_frame(id)
			frame.set_fog(frame.fog_color, frame.fog_near, args[0])
		"ClearFog":
			var frame: WowModelFrame = model_frame(id)
			frame.set_fog(frame.fog_color, 0.0, 0.0)
		"SetFacing":
			model_frame(id).facing = rad_to_deg(float(args[0]))
		"SetGlow":
			model_frame(id).glow = args[0]
		_:
			model_method(id, method, args)


## Unit models, dressing rooms and the like; the glue only needs scenes.
func model_method(_id: int, _method: String, _args: Array) -> void:
	pass


func _play_sound(sound_name: String) -> void:
	if WowAssets.audio.has_sound(sound_name):
		WowAssets.audio.play_sound(sound_name)


func _screenshot() -> void:
	const DIRECTORY: String = "user://Screenshots"
	DirAccess.make_dir_recursive_absolute(DIRECTORY)
	var stamp: String = Time.get_datetime_string_from_system().replace(":", "").replace("-", "")
	get_viewport().get_texture().get_image().save_png(
		DIRECTORY.path_join("WoWScrnShot_%s.png" % stamp)
	)


func _quit() -> void:
	get_tree().quit()


func _nothing() -> void:
	pass
