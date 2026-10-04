## The login, realm and character screens from Interface\GlueXML, run against the session.
class_name StockGlue
extends StockUI

signal world_requested(map_id: int)

const TOC: String = "Interface\\GlueXML\\GlueXML.toc"
const SETTINGS_PATH: String = "user://settings.cfg"
const SETTINGS_SECTION: String = "login"
const DEFAULT_REALMLIST: String = "127.0.0.1"
const AUTH_PORT: int = 3724
const REALMLIST_FILE: String = "realmlist.wtf"
const REALM_FLAG_OFFLINE: int = 0x02
const REALM_FLAG_FULL: int = 0x80
# RealmList.lua reads load 2 as full and -3 as recommended; anything else is the population.
const LOAD_FULL: float = 2.0
const CHARACTER_FLAG_GHOST: int = 0x2000
const FIRST_CHARACTER_RESPONSE: int = 42
const CUSTOMIZATIONS: Array[CharacterModels.Option] = [
	CharacterModels.Option.SKIN,
	CharacterModels.Option.FACE,
	CharacterModels.Option.HAIR_STYLE,
	CharacterModels.Option.HAIR_COLOR,
	CharacterModels.Option.FACIAL_HAIR,
]
const OPTION_KEYS: Dictionary[CharacterModels.Option, String] = {
	CharacterModels.Option.SKIN: "skin",
	CharacterModels.Option.FACE: "face",
	CharacterModels.Option.HAIR_STYLE: "hair_style",
	CharacterModels.Option.HAIR_COLOR: "hair_color",
	CharacterModels.Option.FACIAL_HAIR: "facial_hair",
}
const FACTION_FILES: Array[String] = ["Alliance", "Horde"]
const FACTION_KEYS: Array[String] = ["ALLIANCE", "HORDE"]

## The account and realm last logged into, which name the in-game interface's WTF folders.
static var logged_account: String = ""
static var logged_realm: String = ""

var _settings: ConfigFile = ConfigFile.new()
var _screen: String = ""
var _realms: Array = []
var _realm_name: String = ""
var _choosing_realm: bool = false
var _characters: Array = []
var _characters_ready: bool = false
var _selected: int = 0
var _auto_entering: bool = false
var _auto_character: String = ""
var _launch_realmlist: String = ""
var _created_name: String = ""
# Set while a status dialog's Cancel would abandon a connection.
var _connecting: bool = false
var _select_frame: String = ""
var _create_frame: String = ""
var _look: Dictionary = {}
var _class_ids: Array[int] = []
var _class_index: int = 0


func _ready() -> void:
	_settings.load(SETTINGS_PATH)
	_realm_name = _settings.get_value(SETTINGS_SECTION, "realm", "")
	_register_glue()
	var session: WowSession = WowClient.session
	session.state_changed.connect(_on_state_changed)
	session.realms_received.connect(_on_realms_received)
	session.characters_received.connect(_on_characters_received)
	session.character_created.connect(_on_character_created)
	session.character_deleted.connect(_on_character_deleted)
	session.character_login_failed.connect(_on_character_login_failed)
	load_toc(TOC)
	fire_event("FRAMES_LOADED")
	set_screen("login")
	report_errors()


func set_screen(screen: String) -> void:
	fire_event("SET_GLUE_SCREEN", [screen])


func auto_login(realmlist: String, account: String, password: String, character: String) -> void:
	_auto_entering = true
	_auto_character = character
	if not realmlist.is_empty():
		_launch_realmlist = realmlist
	_log_in(account, password)


func use_realmlist(realmlist: String) -> void:
	_launch_realmlist = realmlist


func _register_glue() -> void:
	var functions: Dictionary[String, Callable] = {
		"GetSavedAccountName": func() -> String: return _settings.get_value(
			SETTINGS_SECTION, "account", ""),
		"SetSavedAccountName": _set_saved_account_name,
		"DefaultServerLogin": func(account: Variant, password: Variant) -> void: _log_in(
			str(account) if account else "", str(password) if password else ""),
		"StatusDialogClick": _on_status_dialog_click,
		"DisconnectFromServer": _disconnect,
		"IsConnectedToServer": func() -> bool: return (
			WowClient.session.get_state() == WowSession.STATE_CHARACTER_LIST),
		"EULAAccepted": _yes, "TOSAccepted": _yes, "ScanningAccepted": _yes,
		"ContestAccepted": _yes,
		"AcceptEULA": _nothing, "AcceptTOS": _nothing, "AcceptScanning": _nothing,
		"AcceptContest": _nothing,
		"ShowEULANotice": _no, "ShowTOSNotice": _no, "ShowScanningNotice": _no,
		"ShowContestNotice": _no,
		"SetCurrentScreen": func(screen: Variant) -> void: _screen = str(screen),
		"PlayGlueMusic": _play_glue_music,
		"PlayCreditsMusic": _nothing,
		"StopGlueMusic": WowAssets.audio.stop_music,
		"GetServerName": _server_name,
		"RequestRealmList": _request_realm_list,
		"CancelRealmListQuery": _on_realm_list_cancelled,
		"RealmListDialogCancelled": _on_realm_list_cancelled,
		"GetRealmCategories": func() -> String: return WowStrings.get_text("REALM_LIST"),
		"GetSelectedCategory": func() -> int: return 1,
		"GetNumRealms": func(_category: Variant) -> int: return _realms.size(),
		"GetRealmInfo": _realm_info,
		"ChangeRealm": _change_realm,
		"SortRealms": _sort_realms,
		"SetPreferredInfo": _nothing,
		"GetNumCharacters": func() -> int: return _characters.size(),
		"GetCharacterInfo": _character_info,
		"GetCharacterListUpdate": _character_list_update,
		"SelectCharacter": _select_character,
		"EnterWorld": _enter_world,
		"DeleteCharacter": _delete_character,
		"RenameCharacter": _nothing,
		"SetCharSelectModelFrame": func(frame: Variant) -> void: _select_frame = str(frame),
		"SetCharCustomizeFrame": func(frame: Variant) -> void: _create_frame = str(frame),
		"SetCharSelectBackground": func(path: Variant) -> void: _set_background(_select_frame, path),
		"SetCharCustomizeBackground": func(path: Variant) -> void:
			_set_background(_create_frame, path),
		"GetCharacterSelectFacing": func() -> float: return _facing(_select_frame),
		"SetCharacterSelectFacing": func(degrees: Variant) -> void:
			_set_facing(_select_frame, degrees),
		"GetCharacterCreateFacing": func() -> float: return _facing(_create_frame),
		"SetCharacterCreateFacing": func(degrees: Variant) -> void:
			_set_facing(_create_frame, degrees),
		"UpdateSelectionCustomizationScene": _nothing,
		"UpdateCustomizationScene": _nothing,
		"ResetCharCustomize": _reset_customize,
		"RandomizeCharCustomization": _randomize_customization,
		"CycleCharCustomization": _cycle_customization,
		"GetAvailableRaces": _available_races,
		"GetSelectedRace": func() -> int: return CharacterOptions.race_order().find(_race()) + 1,
		"SetSelectedRace": _set_selected_race,
		"GetSelectedSex": func() -> int: return int(_look.get("gender", 0)) + 1,
		"SetSelectedSex": _set_selected_sex,
		"GetClassesForRace": _classes_for_race,
		"GetSelectedClass": _selected_class,
		"SetSelectedClass": _set_selected_class,
		"GetFactionForRace": _faction_for_race,
		"GetNameForRace": _name_for_race,
		"GetHairCustomization": func() -> String: return CharacterOptions.hair_kind(_race()),
		"GetFacialHairCustomization": func() -> String: return CharacterOptions.facial_hair_kind(
			_race(), _look.get("gender", 0)),
		"GetRandomName": func() -> String: return "",
		"CreateCharacter": _create_character,
		"GetBillingPlan": func() -> int: return 0,
		"GetBillingTimeRemaining": func() -> int: return 0,
		"GetMovieResolution": func() -> int: return 0,
		"GetScriptMemory": func() -> int: return 0,
		"SetScriptMemory": _nothing,
		"IsAddonVersionCheckEnabled": _yes,
		"SetAddonVersionCheck": _nothing,
		"SaveAddOns": _nothing,
		"ResetAddOns": _nothing,
		"LaunchAddOnURL": _nothing,
		"PatchDownloadProgress": func() -> Array: return [0, 0],
		"PatchDownloadCancel": _nothing,
		"PatchDownloadApply": _nothing,
		"PINEntered": _nothing,
		"SurveyNotificationDone": _nothing,
		"CloseMenus": _nothing,
	}
	for function_name: String in functions:
		register_function(function_name, functions[function_name])


func _set_saved_account_name(account: Variant) -> void:
	_settings.set_value(SETTINGS_SECTION, "account", str(account) if account else "")
	_settings.save(SETTINGS_PATH)


func _play_glue_music(path: Variant) -> void:
	if path:
		WowAssets.audio.play_music(str(path))


func _change_realm(_category: Variant, index: Variant) -> void:
	if index and int(index) >= 1 and int(index) <= _realms.size():
		_join_realm(int(index) - 1)


func _race() -> int:
	return _look.get("race", 1)


func _class_id() -> int:
	return _class_ids[_class_index] if not _class_ids.is_empty() else 1


func _selected_class() -> Array:
	return [CharacterOptions.class_label(_class_id()), CharacterOptions.class_file(_class_id())]


func _set_selected_class(index: Variant) -> void:
	_class_index = clampi(int(index if index else 1) - 1, 0, maxi(_class_ids.size() - 1, 0))
	_show_created()


func _faction_for_race() -> Array:
	var faction: int = CharacterOptions.faction(_race())
	return [WowStrings.get_text(FACTION_KEYS[faction]), FACTION_FILES[faction]]


func _name_for_race() -> Array:
	return [CharacterOptions.race_name(_race()), CharacterOptions.race_file(_race())]


func _randomize_customization() -> void:
	_randomize_look()
	_show_created()


func _yes() -> bool:
	return true


func _no() -> bool:
	return false


func _status(which: String, key: String) -> void:
	fire_event("OPEN_STATUS_DIALOG", [which, WowStrings.get_text(key)])


func _message(text: String) -> void:
	_connecting = false
	fire_event("OPEN_STATUS_DIALOG", ["OKAY", text])


func _response_text(code: int) -> String:
	var index: int = code - FIRST_CHARACTER_RESPONSE
	if index < 0 or index >= Glue.CHARACTER_RESPONSES.size():
		return WowStrings.get_text("CHAR_CREATE_UNKNOWN")
	return WowStrings.get_text(Glue.CHARACTER_RESPONSES[index])


# The stock client reads realmlist.wtf beside the executable; settings and --realm win over it.
func _realmlist() -> String:
	if not _launch_realmlist.is_empty():
		return _launch_realmlist
	var saved: String = _settings.get_value(SETTINGS_SECTION, "realmlist", "")
	if not saved.is_empty():
		return saved
	for path: String in [
		disk_root.path_join(REALMLIST_FILE), disk_root.path_join("Data").path_join(REALMLIST_FILE),
	]:
		var file: FileAccess = FileAccess.open(path, FileAccess.READ)
		if file == null:
			continue
		for line: String in file.get_as_text().split("\n", false):
			var words: PackedStringArray = line.strip_edges().split(" ", false)
			if words.size() >= 3 and words[0].to_lower() == "set" and words[1].to_lower() == "realmlist":
				return words[2].trim_prefix("\"").trim_suffix("\"")
	return DEFAULT_REALMLIST


func _log_in(account: String, password: String) -> void:
	if account.is_empty():
		_message(WowStrings.get_text("LOGIN_ENTER_NAME"))
		return
	if password.is_empty():
		_message(WowStrings.get_text("LOGIN_ENTER_PASSWORD"))
		return
	var realmlist: String = _realmlist()
	var host: String = realmlist
	var port: int = AUTH_PORT
	if realmlist.count(":") == 1:
		host = realmlist.get_slice(":", 0)
		port = realmlist.get_slice(":", 1).to_int()
	logged_account = account
	_connecting = true
	_status("CANCEL", "LOGIN_STATE_CONNECTING")
	WowClient.session.set_locale(WowAssets.video.locale)
	WowClient.session.login(host, port, account, password)


# Cancel on a status dialog abandons the connection; OKAY on a message only closes it.
func _on_status_dialog_click() -> void:
	if not _connecting:
		return
	_connecting = false
	_disconnect()
	set_screen("login")


func _disconnect() -> void:
	_characters = []
	_characters_ready = false
	WowClient.session.disconnect()


func _on_state_changed(state: WowSession.State, message: String) -> void:
	match state:
		WowSession.STATE_AUTHENTICATING:
			_connecting = true
			_status("CANCEL", "LOGIN_STATE_AUTHENTICATING")
		WowSession.STATE_CONNECTING_WORLD:
			_connecting = true
			_status("CANCEL", "LOGIN_STATE_CONNECTING")
		WowSession.STATE_CHARACTER_LIST:
			_connecting = false
			_characters_ready = false
			if _screen != "charselect" and _screen != "charcreate":
				set_screen("charselect")
			_status("CANCEL", "CHAR_LIST_RETRIEVING")
		WowSession.STATE_FAILED:
			WowClient.session.disconnect()
			set_screen("login")
			_message(message if not message.is_empty() else WowStrings.get_text("DISCONNECTED"))


func _server_name() -> Array:
	for realm: Dictionary in _realms:
		if realm["name"] == _realm_name:
			var icon: int = realm["icon"]
			return [_realm_name, icon in [RealmList.RealmType.PVP, RealmList.RealmType.RP_PVP],
				icon in [RealmList.RealmType.RP, RealmList.RealmType.RP_PVP]]
	return [_realm_name if not _realm_name.is_empty() else null]


func _request_realm_list(_show: Variant = null) -> void:
	_choosing_realm = true
	_status("CANCEL", "REALM_LIST_IN_PROGRESS")
	WowClient.session.request_realms()


func _on_realms_received(realms: Array) -> void:
	_realms = realms
	var known: int = -1
	for i: int in realms.size():
		if realms[i]["name"] == _realm_name:
			known = i
	if not _choosing_realm and (known >= 0 or realms.size() == 1):
		_join_realm(maxi(known, 0))
		return
	_choosing_realm = true
	_connecting = false
	fire_event("CLOSE_STATUS_DIALOG")
	set_lua_global("WOWDOT_REALM", _realm_name)
	run_lua("RealmList.selectedName = WOWDOT_REALM", "realm")
	fire_event("OPEN_REALM_LIST")


func _join_realm(index: int) -> void:
	_choosing_realm = false
	_realm_name = _realms[index]["name"]
	logged_realm = _realm_name
	_settings.set_value(SETTINGS_SECTION, "realm", _realm_name)
	_settings.save(SETTINGS_PATH)
	_connecting = true
	WowClient.session.select_realm(index)


func _on_realm_list_cancelled() -> void:
	_choosing_realm = false
	for i: int in _realms.size():
		if _realms[i]["name"] == _realm_name:
			_join_realm(i)
			return
	_disconnect()
	set_screen("login")


func _realm_info(_category: Variant, index: Variant) -> Array:
	var i: int = int(index if index else 0) - 1
	if i < 0 or i >= _realms.size():
		return []
	var realm: Dictionary = _realms[i]
	var icon: int = realm["icon"]
	var load: float = LOAD_FULL if realm["flags"] & REALM_FLAG_FULL else float(realm["population"]) - 1.0
	return [
		realm["name"], realm["characters"], false, realm["flags"] & REALM_FLAG_OFFLINE != 0,
		realm["name"] == _realm_name,
		icon in [RealmList.RealmType.PVP, RealmList.RealmType.RP_PVP],
		icon in [RealmList.RealmType.RP, RealmList.RealmType.RP_PVP], load,
	]


func _sort_realms(key: Variant) -> void:
	match str(key):
		"name":
			_realms.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return a["name"] < b["name"])
		"mode":
			_realms.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return a["icon"] < b["icon"])
		"characters":
			_realms.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
				return a["characters"] > b["characters"])
		"load":
			_realms.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
				return a["population"] > b["population"])
	fire_event("OPEN_REALM_LIST")


func _on_characters_received(characters: Array) -> void:
	_characters = characters
	_characters_ready = true
	_connecting = false
	fire_event("CLOSE_STATUS_DIALOG")
	for character: Dictionary in characters:
		if _auto_entering and _auto_character in ["", character["name"]]:
			_auto_entering = false
			_selected = characters.find(character) + 1
			_enter_world()
			return
	var created: int = -1
	for i: int in characters.size():
		if characters[i]["name"] == _created_name:
			created = i
	_created_name = ""
	if _screen != "charselect":
		set_screen("charselect")
	fire_event("CHARACTER_LIST_UPDATE")
	if created >= 0:
		call_lua("CharacterSelect_SelectCharacter", [created + 1, 1])


func _character_list_update() -> void:
	if _characters_ready:
		fire_event.call_deferred("CHARACTER_LIST_UPDATE")


func _character_info(index: Variant) -> Array:
	var i: int = int(index if index else 0) - 1
	if i < 0 or i >= _characters.size():
		return []
	var character: Dictionary = _characters[i]
	var race: int = character["race"]
	return [
		character["name"], CharacterOptions.race_name(race),
		CharacterOptions.class_label(character["class"]), character["level"],
		AreaInfo.area_name(character["zone"]), CharacterOptions.race_file(race),
		character["gender"], character.get("flags", 0) & CHARACTER_FLAG_GHOST != 0,
	]


func _select_character(index: Variant) -> void:
	_selected = int(index if index else 0)
	var frame: WowModelFrame = model_frame_named(_select_frame)
	if frame and _selected >= 1 and _selected <= _characters.size():
		var character: Dictionary = _characters[_selected - 1]
		frame.facing = 0.0
		frame.show_character(
			CharacterOptions.character_model(CharacterModels.listed_look(character))
		)
	fire_event("UPDATE_SELECTED_CHARACTER", [_selected])


func _enter_world() -> void:
	if _selected < 1 or _selected > _characters.size():
		return
	var character: Dictionary = _characters[_selected - 1]
	world_requested.emit(character["map"])
	WowClient.session.enter_world(character["guid"])


func _delete_character(index: Variant) -> void:
	var i: int = int(index if index else 0) - 1
	if i < 0 or i >= _characters.size():
		return
	_status("CANCEL", "CHAR_DELETE_IN_PROGRESS")
	WowClient.session.delete_character(_characters[i]["guid"])


func _on_character_deleted(success: bool, code: int) -> void:
	if success:
		_selected = 0
		WowClient.session.request_characters()
	else:
		_message(_response_text(code))


func _on_character_login_failed(code: int) -> void:
	set_screen("charselect")
	_message(_response_text(code))


func _set_background(frame_name: String, path: Variant) -> void:
	var frame: WowModelFrame = model_frame_named(frame_name)
	if frame and path:
		frame.model_file = model_path(str(path))


func _facing(frame_name: String) -> float:
	var frame: WowModelFrame = model_frame_named(frame_name)
	return frame.facing if frame else 0.0


func _set_facing(frame_name: String, degrees: Variant) -> void:
	var frame: WowModelFrame = model_frame_named(frame_name)
	if frame:
		frame.facing = float(degrees if degrees else 0.0)


func _available_races() -> Array:
	var races: Array = []
	for race: int in CharacterOptions.race_order():
		races.append(CharacterOptions.race_name(race))
		races.append(CharacterOptions.race_file(race))
	return races


func _classes_for_race() -> Array:
	var classes: Array = []
	for class_id: int in _class_ids:
		classes.append(CharacterOptions.class_label(class_id))
		classes.append(CharacterOptions.class_file(class_id))
	return classes


func _reset_customize() -> void:
	_look = {"gender": randi_range(0, 1)}
	_set_race(CharacterOptions.race_order().pick_random())


func _set_selected_race(index: Variant) -> void:
	var order: Array[int] = CharacterOptions.race_order()
	_set_race(order[clampi(int(index if index else 1) - 1, 0, order.size() - 1)])


func _set_race(race: int) -> void:
	_look["race"] = race
	_class_ids = CharacterOptions.classes_for(race)
	_class_index = 0
	_randomize_look()
	_show_created()


func _set_selected_sex(sex: Variant) -> void:
	var gender: int = 1 if int(sex if sex else 1) == 2 else 0
	if _look.get("gender", -1) != gender:
		_look["gender"] = gender
		_randomize_look()
	_show_created()


func _randomize_look() -> void:
	for option: CharacterModels.Option in CUSTOMIZATIONS:
		var count: int = WowAssets.characters.option_count(_look, option)
		_look[OPTION_KEYS[option]] = randi_range(0, maxi(count - 1, 0))


# Faces follow the skin colour and hair colours the style, so later choices are re-clamped.
func _cycle_customization(index: Variant, step: Variant) -> void:
	var at: int = int(index if index else 1) - 1
	if at < 0 or at >= CUSTOMIZATIONS.size():
		return
	var option: CharacterModels.Option = CUSTOMIZATIONS[at]
	var count: int = WowAssets.characters.option_count(_look, option)
	if count > 0:
		_look[OPTION_KEYS[option]] = posmod(_look.get(OPTION_KEYS[option], 0) + int(step if step else 1), count)
	for later: CharacterModels.Option in CUSTOMIZATIONS.slice(at + 1):
		var later_count: int = WowAssets.characters.option_count(_look, later)
		_look[OPTION_KEYS[later]] = mini(_look.get(OPTION_KEYS[later], 0), maxi(later_count - 1, 0))
	_show_created()


func _show_created() -> void:
	var frame: WowModelFrame = model_frame_named(_create_frame)
	if frame == null or not _look.has("race"):
		return
	var look: Dictionary = _look.duplicate()
	look["class"] = _class_id()
	frame.show_character(CharacterOptions.character_model(CharacterModels.starting_look(look)))


func _create_character(character_name: Variant) -> void:
	var character: Dictionary = _look.duplicate()
	character["name"] = str(character_name if character_name else "").strip_edges()
	character["class"] = _class_id()
	_created_name = character["name"]
	_status("CANCEL", "CHAR_CREATE_IN_PROGRESS")
	WowClient.session.create_character(character)


func _on_character_created(success: bool, code: int) -> void:
	if success:
		set_screen("charselect")
		WowClient.session.request_characters()
	else:
		_created_name = ""
		_message(_response_text(code))
