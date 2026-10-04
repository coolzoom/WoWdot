class_name StockGlueCheck
extends Node

const MAIN: PackedScene = preload("res://game/main.tscn")
const SAMPLE_CHARACTER: Dictionary = {
	"guid": 1, "name": "Stockcheck", "race": 2, "class": 1, "gender": 0, "level": 12,
	"zone": 14, "map": 1, "flags": 0, "skin": 1, "face": 2, "hair_style": 3, "hair_color": 1,
	"facial_hair": 1,
}

var _failures: PackedStringArray = []
var _stock: StockGlue


# Walks the stock glue screens without a server, feeding the session's own signals.
func _ready() -> void:
	if not StockUI.enabled():
		print("stock_glue_check: stock interface is off")
		get_tree().quit(1)
		return
	var main: Main = MAIN.instantiate()
	add_child(main)
	_stock = main.get_node("%Glue").get_node("StockGlue")
	_run.call_deferred()


func _run() -> void:
	await _frames(60)
	_check(_stock.is_widget_visible("AccountLogin"), "the login screen shows")
	_check(_stock.is_widget_visible("AccountLoginAccountEdit"), "the account box shows")
	if not _stock.get_disk_file(StockGlue.TOC).is_empty():
		print("stock_glue_check: GlueXML.toc overridden by ", _stock.get_disk_file(StockGlue.TOC))
	if _stock.get_widget_id("AccountLoginServerEdit") >= 0:
		_check(_stock.is_widget_visible("AccountLoginServerEdit"), "the server address box shows")
		_stock.run_lua("AccountLoginServerEdit:SetText('10.0.0.5:3724')", "check")
		_check(str(_stock.get_cvars().get("realmList")) == "10.0.0.5:3724",
			"typing an address sets realmList")
	_capture("user://stock_glue_login.png")
	var session: WowSession = WowClient.session
	session.state_changed.emit(WowSession.STATE_CHARACTER_LIST, "")
	session.characters_received.emit([SAMPLE_CHARACTER])
	await _frames(60)
	_check(_stock.is_widget_visible("CharacterSelect"), "character select shows")
	_check(_stock.is_widget_visible("CharSelectCharacterButton1"), "the character is listed")
	_capture("user://stock_glue_select.png")
	_stock.set_screen("charcreate")
	await _frames(60)
	_check(_stock.is_widget_visible("CharacterCreate"), "character creation shows")
	_capture("user://stock_glue_create.png")
	for error: String in _stock.get_errors():
		_failures.append("Lua: " + error)
	print("stock_glue_check: missing ", _stock.get_missing())
	_finish()


func _check(condition: bool, what: String) -> void:
	if not condition:
		_failures.append(what)


func _capture(path: String) -> void:
	get_viewport().get_texture().get_image().save_png(path)
	print("stock_glue_check: saved ", ProjectSettings.globalize_path(path))


func _frames(count: int) -> void:
	for i: int in count:
		await get_tree().process_frame


func _finish() -> void:
	for failure: String in _failures:
		print("stock_glue_check: FAIL ", failure)
	print("stock_glue_check: ", "ok" if _failures.is_empty() else "%d failures" % _failures.size())
	get_tree().quit(0 if _failures.is_empty() else 1)
