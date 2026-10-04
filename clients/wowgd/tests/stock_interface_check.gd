class_name StockInterfaceCheck
extends Node

const HUD: PackedScene = preload("res://ui/hud.tscn")

var _failures: PackedStringArray = []
var _stock: StockInterface


# Loads the stock FrameXML under the HUD without a server and feeds it the session's signals.
func _ready() -> void:
	if not StockUI.enabled():
		print("stock_interface_check: stock interface is off")
		get_tree().quit(1)
		return
	var layer: CanvasLayer = CanvasLayer.new()
	add_child(layer)
	var hud: Hud = HUD.instantiate()
	layer.add_child(hud)
	_run.call_deferred(layer, hud)


func _run(layer: CanvasLayer, hud: Hud) -> void:
	await _frames(5)
	_stock = layer.get_node_or_null("StockInterface")
	if _stock == null:
		_failures.append("the stock interface was added")
		_finish()
		return
	_stock.enter("Stockcheck")
	await _frames(30)
	for frame: String in ["MainMenuBar", "PlayerFrame", "ChatFrame1", "CharacterMicroButton"]:
		_check(_stock.is_widget_visible(frame), frame + " shows")
	_check(not _stock.is_widget_visible("MinimapCluster"), "the stock minimap stays hidden")
	var bindings: Dictionary = _stock.get_key_bindings()
	_check(bindings.values().has("ACTIONBUTTON1"), "action keys drive the stock bar")
	_check(bindings.values().has("OPENCHAT"), "the chat key opens the stock edit box")
	_stock.run_lua("WowdotCheckSaved = { count = 3, label = 'a\\nb', nested = { true }, frame = UIParent }")
	var saved: String = str(_stock.call_lua("WowdotSerialize", ["WowdotCheckSaved"]))
	_stock.run_lua("WowdotCheckSaved = nil")
	_stock.run_lua(saved)
	_stock.run_lua("WowdotCheckRead = WowdotCheckSaved.count .. WowdotCheckSaved.label .. tostring(WowdotCheckSaved.nested[1]) .. tostring(WowdotCheckSaved.frame)")
	_check(_stock.get_lua_global("WowdotCheckRead") == "3a\nbtruenil", "saved variables round-trip without frames")
	_stock.run_lua("SetPortraitTexture(PlayerPortrait, 'player') GameTooltip:SetOwner(UIParent) GameTooltip:SetAction(1) GameTooltip:SetUnit('target') GameTooltip:Hide()")
	WowClient.session.chat_received.emit({
		"type": WowSession.CHAT_SAY, "language": 0, "text": "Lok'tar!", "sender_guid": 2,
		"sender_name": "Grunt",
	})
	_stock.add_system_line("Welcome to the stock interface.")
	_stock.show_error("Not enough rage")
	await _frames(30)
	_stock.run_lua("WowdotClicks = '' local open = WowdotPanel WowdotPanel = function(panel, arg) WowdotClicks = WowdotClicks .. panel .. ';' open(panel, arg) end")
	var micro: Dictionary[String, String] = {
		"CharacterMicroButton": "character", "SpellbookMicroButton": "spellbook",
		"TalentMicroButton": "talents", "QuestLogMicroButton": "quest_log",
		"SocialsMicroButton": "social", "WorldMapMicroButton": "world_map",
		"MainMenuMicroButton": "game_menu", "HelpMicroButton": "help",
		"MainMenuBarBackpackButton": "backpack",
	}
	for button: String in micro:
		if not _stock.is_widget_visible(button):
			continue
		var at: Vector2 = _stock.get_widget_rect(button).get_center()
		_check(_stock.get_widget_name(_stock.get_widget_at(at)) == button, button + " is not covered")
		await _click(at)
		_check(str(_stock.get_lua_global("WowdotClicks")).ends_with(micro[button] + ";"),
			button + " opens " + micro[button])
		if button == "CharacterMicroButton":
			_check(hud.get("_character").visible, "the character window opens")
			await _click(at)
	print("stock_interface_check: opened ", _stock.get_lua_global("WowdotClicks"))
	var tip: String = "GameTooltip:SetOwner(UIParent, 'ANCHOR_CURSOR') GameTooltip:SetText('Latency') GameTooltip:AddLine(string.rep('The average time to talk to the game server. ', 6), 1, 0.82, 0, %s) GameTooltip:Show()"
	_stock.run_lua(tip % "nil")
	await _frames(2)
	var single: Rect2 = _stock.get_widget_rect("GameTooltip")
	_stock.run_lua(tip % "1")
	await _frames(2)
	var wrapped: Rect2 = _stock.get_widget_rect("GameTooltip")
	_check(wrapped.size.x < single.size.x * 0.5 and wrapped.size.y > single.size.y * 2.0,
		"a wrapping tooltip line breaks onto more lines")
	_capture("user://stock_interface_tooltip.png")
	_stock.run_lua("GameTooltip:Hide()")
	# The newbie tip goes to the default corner at its own width, whatever tooltip came before.
	var widths: Array[float] = []
	for before: String in ["'ANCHOR_CURSOR'", "'ANCHOR_RIGHT'", "'ANCHOR_NONE'"]:
		_stock.run_lua("GameTooltip:SetOwner(UIParent, %s) GameTooltip:SetText(string.rep('wide ', 40)) GameTooltip:Show()" % before)
		await _frames(2)
		_stock.run_widget_script(_stock.get_widget_id("PlayerFrame"), "OnEnter")
		await _frames(2)
		var tooltip: Rect2 = _stock.get_widget_rect("GameTooltip")
		var text: Rect2 = _stock.get_widget_rect("GameTooltipTextLeft2")
		_check(tooltip.encloses(text), "the portrait tip holds its text after " + before)
		_check(tooltip.end.x > _stock.size.x * 0.9 and tooltip.end.y > _stock.size.y * 0.75,
			"the portrait tip sits in the lower right after " + before)
		widths.append(tooltip.size.x)
		_capture("user://stock_interface_tooltip.png")
		_stock.run_widget_script(_stock.get_widget_id("PlayerFrame"), "OnLeave")
	_check(is_equal_approx(widths.min(), widths.max()), "the portrait tip keeps one width")
	_capture("user://stock_interface_hud.png")
	for error: String in _stock.get_errors():
		_failures.append("Lua: " + error)
	print("stock_interface_check: missing ", _stock.get_missing())
	_finish()


func _check(condition: bool, what: String) -> void:
	if not condition:
		_failures.append(what)


func _capture(path: String) -> void:
	get_viewport().get_texture().get_image().save_png(path)
	print("stock_interface_check: saved ", ProjectSettings.globalize_path(path))


func _click(at: Vector2) -> void:
	for pressed: bool in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		event.position = at
		event.global_position = at
		get_viewport().push_input(event)
		await _frames(2)


func _frames(count: int) -> void:
	for i: int in count:
		await get_tree().process_frame


func _finish() -> void:
	for failure: String in _failures:
		print("stock_interface_check: FAIL ", failure)
	print("stock_interface_check: ", "ok" if _failures.is_empty() else "%d failures" % _failures.size())
	get_tree().quit(0 if _failures.is_empty() else 1)
