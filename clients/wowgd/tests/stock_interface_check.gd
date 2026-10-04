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
	_run.call_deferred(layer)


func _run(layer: CanvasLayer) -> void:
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


func _frames(count: int) -> void:
	for i: int in count:
		await get_tree().process_frame


func _finish() -> void:
	for failure: String in _failures:
		print("stock_interface_check: FAIL ", failure)
	print("stock_interface_check: ", "ok" if _failures.is_empty() else "%d failures" % _failures.size())
	get_tree().quit(0 if _failures.is_empty() else 1)
