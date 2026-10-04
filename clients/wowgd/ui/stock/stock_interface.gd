## The in-game interface from Interface\FrameXML and the addons, fed from the session.
## Unit frames, action bars, the casting bar, buffs and chat are the stock ones; the
## full-screen windows stay the HUD's, and the stock buttons and bindings open those.
class_name StockInterface
extends StockUI

const TOC: String = "Interface\\FrameXML\\FrameXML.toc"
const BINDINGS: String = "Interface\\FrameXML\\Bindings.xml"
const WTF: String = "user://WTF/Account"
const SETTINGS_PATH: String = "user://stock_interface.cfg"
const POLL_SECONDS: float = 0.1
const SPELL_ATTACK: int = 6603
const ACTION_MASK: int = 0xFFFFFF
const ACTION_MACRO: int = 0x40
const ACTION_ITEM: int = 0x80
const UNIT_FLAG_TAXI_FLIGHT: int = 0x100000
const UNIT_FLAG_PVP: int = 0x1000
const UNIT_FLAG_IN_COMBAT: int = 0x80000
const PLAYER_FLAG_GHOST: int = 0x10
const PLAYER_FLAG_RESTING: int = 0x20
const PLAYER_FLAG_FFA_PVP: int = 0x80
const DYNAMIC_FLAG_TAPPED: int = 0x4
const DYNAMIC_FLAG_TAPPED_BY_PLAYER: int = 0x8
# Rage is stored at ten times what the bar shows.
const RAGE: int = 1
const RAGE_SCALE: int = 10
const POWER_EVENTS: Array[String] = ["MANA", "RAGE", "FOCUS", "ENERGY", "HAPPINESS"]
const DISPEL_TYPES: Array = [null, "Magic", "Curse", "Disease", "Poison"]
const CLASSIFICATIONS: Array[String] = ["normal", "elite", "rareelite", "worldboss", "rare"]
const WORLD_BOSS: int = 3
# Languages.dbc: what each faction speaks by default.
const HORDE_LANGUAGE: int = 1
const ALLIANCE_LANGUAGE: int = 7
const UNIVERSAL: String = "Universal"
const PANEL_UNITS: Array[String] = [
	"player", "target", "targettarget", "pet", "party1", "party2", "party3", "party4",
	"partypet1", "partypet2", "partypet3", "partypet4",
]
const CHAT_TYPES: Dictionary[int, String] = {
	WowSession.CHAT_SAY: "SAY", WowSession.CHAT_PARTY: "PARTY", WowSession.CHAT_RAID: "RAID",
	WowSession.CHAT_GUILD: "GUILD", WowSession.CHAT_OFFICER: "OFFICER", WowSession.CHAT_YELL: "YELL",
	WowSession.CHAT_WHISPER: "WHISPER", WowSession.CHAT_WHISPER_INFORM: "WHISPER_INFORM",
	WowSession.CHAT_EMOTE: "EMOTE", WowSession.CHAT_TEXT_EMOTE: "TEXT_EMOTE",
	WowSession.CHAT_SYSTEM: "SYSTEM", WowSession.CHAT_MONSTER_SAY: "MONSTER_SAY",
	WowSession.CHAT_MONSTER_YELL: "MONSTER_YELL", WowSession.CHAT_MONSTER_EMOTE: "MONSTER_EMOTE",
	WowSession.CHAT_MONSTER_WHISPER: "MONSTER_WHISPER", WowSession.CHAT_CHANNEL: "CHANNEL",
	WowSession.CHAT_AFK: "AFK", WowSession.CHAT_DND: "DND",
	WowSession.CHAT_RAID_LEADER: "RAID_LEADER", WowSession.CHAT_RAID_WARNING: "RAID_WARNING",
	WowSession.CHAT_RAID_BOSS_EMOTE: "RAID_BOSS_EMOTE",
	WowSession.CHAT_BATTLEGROUND: "BATTLEGROUND",
	WowSession.CHAT_BATTLEGROUND_LEADER: "BATTLEGROUND_LEADER",
	WowSession.CHAT_BG_SYSTEM_NEUTRAL: "BG_SYSTEM_NEUTRAL",
	WowSession.CHAT_BG_SYSTEM_ALLIANCE: "BG_SYSTEM_ALLIANCE",
	WowSession.CHAT_BG_SYSTEM_HORDE: "BG_SYSTEM_HORDE",
}
# ChatTypeInfo colours the stock client's default chat-cache.txt gives each type.
const CHAT_COLORS: Dictionary[String, Color] = {
	"SYSTEM": Color(1.0, 1.0, 0.0), "SAY": Color(1.0, 1.0, 1.0), "PARTY": Color(0.67, 0.67, 1.0),
	"RAID": Color(1.0, 0.5, 0.0), "GUILD": Color(0.25, 1.0, 0.25), "OFFICER": Color(0.25, 0.75, 0.25),
	"YELL": Color(1.0, 0.25, 0.25), "WHISPER": Color(1.0, 0.5, 1.0),
	"WHISPER_INFORM": Color(1.0, 0.5, 1.0), "EMOTE": Color(1.0, 0.5, 0.25),
	"TEXT_EMOTE": Color(1.0, 0.5, 0.25), "MONSTER_SAY": Color(1.0, 1.0, 0.62),
	"MONSTER_YELL": Color(1.0, 0.25, 0.25), "MONSTER_EMOTE": Color(1.0, 0.5, 0.25),
	"MONSTER_WHISPER": Color(1.0, 0.71, 0.92), "CHANNEL": Color(1.0, 0.75, 0.75),
	"SKILL": Color(0.33, 0.33, 1.0), "LOOT": Color(0.0, 0.67, 0.0),
	"RAID_LEADER": Color(1.0, 0.28, 0.04), "RAID_WARNING": Color(1.0, 0.28, 0.0),
	"COMBAT_MISC_INFO": Color(0.5, 0.5, 1.0), "AFK": Color(1.0, 0.5, 1.0), "DND": Color(1.0, 0.5, 1.0),
}
const GENERAL_GROUPS: Array[String] = [
	"SYSTEM", "SAY", "YELL", "WHISPER", "PARTY", "GUILD", "CREATURE", "CHANNEL", "SKILL", "LOOT",
	"COMBAT_FACTION_CHANGE",
]
const COMBAT_GROUPS: Array[String] = ["COMBAT_MISC_INFO", "COMBAT_XP_GAIN", "COMBAT_HONOR_GAIN"]
# The stock slot names, with the empty-slot art GetInventorySlotInfo hands back.
const INVENTORY_SLOTS: Dictionary[String, Array] = {
	"AmmoSlot": [0, "Ammo"], "HeadSlot": [1, "Head"], "NeckSlot": [2, "Neck"],
	"ShoulderSlot": [3, "Shoulder"], "ShirtSlot": [4, "Shirt"], "ChestSlot": [5, "Chest"],
	"WaistSlot": [6, "Waist"], "LegsSlot": [7, "Legs"], "FeetSlot": [8, "Feet"],
	"WristSlot": [9, "Wrists"], "HandsSlot": [10, "Hands"], "Finger0Slot": [11, "Finger"],
	"Finger1Slot": [12, "Finger"], "Trinket0Slot": [13, "Trinket"], "Trinket1Slot": [14, "Trinket"],
	"BackSlot": [15, "Chest"], "MainHandSlot": [16, "MainHand"],
	"SecondaryHandSlot": [17, "SecondaryHand"], "RangedSlot": [18, "Ranged"],
	"TabardSlot": [19, "Tabard"], "Bag0Slot": [20, "Bag"], "Bag1Slot": [21, "Bag"],
	"Bag2Slot": [22, "Bag"], "Bag3Slot": [23, "Bag"],
}
const EMPTY_SLOT_ART: String = "Interface\\PaperDoll\\UI-PaperDoll-Slot-"
# Which stock binding each of the game's input actions drives; the rest stay the world's.
const ACTION_BINDINGS: Dictionary[String, String] = {
	"chat": "OPENCHAT", "reply_whisper": "REPLY",
	"action_button_1": "ACTIONBUTTON1", "action_button_2": "ACTIONBUTTON2",
	"action_button_3": "ACTIONBUTTON3", "action_button_4": "ACTIONBUTTON4",
	"action_button_5": "ACTIONBUTTON5", "action_button_6": "ACTIONBUTTON6",
	"action_button_7": "ACTIONBUTTON7", "action_button_8": "ACTIONBUTTON8",
	"action_button_9": "ACTIONBUTTON9", "action_button_10": "ACTIONBUTTON10",
	"action_button_11": "ACTIONBUTTON11", "action_button_12": "ACTIONBUTTON12",
	"action_page_1": "ACTIONPAGE1", "action_page_2": "ACTIONPAGE2", "action_page_3": "ACTIONPAGE3",
	"action_page_4": "ACTIONPAGE4", "action_page_5": "ACTIONPAGE5", "action_page_6": "ACTIONPAGE6",
}
const FIXED_BINDINGS: Dictionary[String, String] = {
	"/": "OPENCHATSLASH", "PAGEUP": "CHATPAGEUP", "PAGEDOWN": "CHATPAGEDOWN",
	"SHIFT-PAGEDOWN": "CHATBOTTOM",
}
# Frames the HUD keeps its own versions of.
const HIDDEN_FRAMES: Array[String] = [
	"MinimapCluster", "QuestWatchFrame", "QuestTimerFrame", "DurabilityFrame", "TutorialFrameParent",
]
const PANEL_OVERRIDES: String = """
function ToggleCharacter(tab) WowdotPanel("character", tab) end
function ToggleSpellBook(book) WowdotPanel("spellbook", book) end
function ToggleTalentFrame() WowdotPanel("talents") end
function ToggleQuestLog() WowdotPanel("quest_log") end
function ToggleFriendsFrame(tab) WowdotPanel("social", tab) end
function ToggleWorldMap() WowdotPanel("world_map") end
function ToggleGameMenu() WowdotPanel("game_menu") end
function ToggleHelpFrame() WowdotPanel("help") end
function ToggleBackpack() WowdotPanel("backpack") end
function ToggleBag(id) WowdotPanel("bag", id) end
function ToggleKeyRing() WowdotPanel("keyring") end
function OpenAllBags() WowdotPanel("all_bags") end
"""
# Writes globals back as Lua; frames, functions, cycles and non-finite numbers are left out.
const SERIALIZER: String = """
function WowdotSerialize(names)
	local seen = {}
	local value
	value = function(v, indent)
		local t = type(v)
		if t == "number" then
			if v ~= v or v == 1/0 or v == -1/0 then return nil end
			return string.format("%.17g", v)
		elseif t == "string" then
			return string.format("%q", v)
		elseif t == "boolean" then
			return v and "true" or "false"
		elseif t == "table" then
			if seen[v] or type(v.GetObjectType) == "function" then return nil end
			seen[v] = true
			local parts = {}
			for k, x in pairs(v) do
				local key
				if type(k) == "string" then
					key = "[" .. string.format("%q", k) .. "]"
				elseif type(k) == "number" and k == k then
					key = "[" .. string.format("%.17g", k) .. "]"
				end
				local text = key and value(x, indent .. "\\t")
				if text then
					table.insert(parts, indent .. "\\t" .. key .. " = " .. text .. ",\\n")
				end
			end
			seen[v] = nil
			return "{\\n" .. table.concat(parts) .. indent .. "}"
		end
		return nil
	end
	local out = {}
	for name in (string.gmatch or string.gfind)(names, "%S+") do
		local text = value(rawget(_G, name), "")
		if text then table.insert(out, name .. " = " .. text .. "\\n") end
	end
	return table.concat(out)
end
"""

var hud: Hud

var _session: WowSession = WowClient.session
var _tooltip: TooltipLines = TooltipLines.new()
var _frame_saved: PackedStringArray = []
var _addon_saved: Dictionary[String, Dictionary] = {}
var _character: String = ""
var _entered: bool = false
var _snapshots: Dictionary[String, Array] = {}
var _player_snapshot: Array = []
var _poll_elapsed: float = 0.0
var _cursor: int = 0
var _cursor_icon: TextureRect
var _typing_proxy: LineEdit
var _casting_spell: int = 0
var _channeling: bool = false
var _attacking: bool = false
var _bags_dirty: bool = false
var _chat_channels: PackedStringArray = []
var _chat_windows: Array[Dictionary] = []
var _settings: ConfigFile = ConfigFile.new()
var _portraits: Dictionary[int, Array] = {}
var _forms: Array[Vector2i] = []
var _languages: WowDBC
var _area: int = 0


func _init() -> void:
	super()
	mouse_filter = Control.MOUSE_FILTER_STOP
	_cursor_icon = TextureRect.new()
	_cursor_icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_cursor_icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_cursor_icon.size = Vector2(32, 32)
	_cursor_icon.visible = false
	add_child(_cursor_icon)
	# The world and the HUD stay out of the keyboard while a stock edit box has it.
	_typing_proxy = LineEdit.new()
	_typing_proxy.modulate = Color(1, 1, 1, 0)
	_typing_proxy.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_typing_proxy.size = Vector2.ONE
	_typing_proxy.position = Vector2(-10, -10)
	add_child(_typing_proxy)
	_languages = WowDBC.open(WowAssets.archive, "Languages")
	_settings.load(SETTINGS_PATH)
	_load_chat_windows()
	_register_all()


func _ready() -> void:
	addon_files_loaded.connect(_on_addon_files_loaded)
	_session.object_updated.connect(_on_object_updated)
	_session.auras_changed.connect(_on_auras_changed)
	_session.spell_cast_started.connect(_on_cast_started)
	_session.spell_cast_finished.connect(_on_cast_finished)
	_session.spell_cast_failed.connect(_on_cast_failed)
	_session.spell_cast_delayed.connect(_on_cast_delayed)
	_session.spell_channel_started.connect(_on_channel_started)
	_session.spell_channel_updated.connect(_on_channel_updated)
	_session.attack_started.connect(_on_attack_changed.bind(true))
	_session.attack_stopped.connect(_on_attack_changed.bind(false))
	_session.action_buttons_changed.connect(_fire.bind("ACTIONBAR_SLOT_CHANGED", [0]))
	_session.spells_changed.connect(_on_spells_changed)
	_session.chat_received.connect(_on_chat_received)
	_session.leveled_up.connect(_on_leveled_up)
	_session.name_received.connect(_on_name_received)
	_session.item_info_received.connect(func(_entry: int) -> void: _bags_dirty = true)
	WowClient.cooldowns.changed.connect(_on_cooldowns_changed)
	WowClient.combat.logged.connect(_on_combat_logged)
	WowClient.macros.changed.connect(_fire.bind("ACTIONBAR_SLOT_CHANGED", [0]))
	run_lua(SERIALIZER, "WowdotSerialize")
	run_lua("RegisterCVar('cameraSmoothStyle', '1') RegisterCVar('cameraSmoothTrackingStyle', '1')")
	load_toc(TOC)
	load_bindings(BINDINGS)
	set_key_bindings(_key_bindings())
	run_lua(PANEL_OVERRIDES, "WowdotPanels")
	for frame: String in HIDDEN_FRAMES:
		run_lua("if %s then %s:Hide() %s.Show = %s.Hide end" % [frame, frame, frame, frame])
	_rebuild_forms()
	report_errors()


func _exit_tree() -> void:
	if _entered:
		fire_event("PLAYER_LOGOUT")
		save_variables()
	_tooltip.free()


func _process(delta: float) -> void:
	super(delta)
	_follow_typing()
	if _cursor != 0:
		_cursor_icon.position = get_local_mouse_position() - _cursor_icon.size * 0.5
	if not _entered:
		return
	_poll_elapsed += delta
	if _poll_elapsed < POLL_SECONDS:
		return
	_poll_elapsed = 0.0
	_poll_units()
	if _bags_dirty:
		_bags_dirty = false
		for bag: int in range(0, Inventory.BAG_COUNT + 1):
			fire_event("BAG_UPDATE", [bag])
		fire_event("UNIT_INVENTORY_CHANGED", ["player"])
		fire_event("ACTIONBAR_UPDATE_USABLE")


func _unhandled_input(event: InputEvent) -> void:
	var click: InputEventMouseButton = event as InputEventMouseButton
	if _cursor != 0 and click and click.pressed:
		_set_cursor(0)


# The world asks for the player once it is in; saved variables and addons wait for a name.
func show_player(guid: int) -> void:
	if _entered:
		_fire_entering()
		return
	var player_name: String = _session.get_object_name(guid)
	if player_name.is_empty():
		_session.name_received.connect(
			func(named: int, _n: String) -> void: if named == guid: show_player(guid),
			CONNECT_ONE_SHOT,
		)
		return
	enter(player_name)


func enter(character_name: String) -> void:
	_character = character_name
	_entered = true
	_load_chat_windows()
	_restore(_frame_saved, _account_dir().path_join("SavedVariables.lua"))
	for addon: String in get_addons():
		var info: Dictionary = get_addon_info(addon)
		if str(info.get("LoadOnDemand", "0")) != "1" and _addon_enabled(addon):
			load_addon(addon)
	fire_event("VARIABLES_LOADED")
	fire_event("PLAYER_LOGIN")
	_fire_entering()
	report_errors()


func show_target(_guid: int) -> void:
	_poll_units()


func show_error(text: String) -> void:
	fire_event("UI_ERROR_MESSAGE", [text])


func show_notice(text: String) -> void:
	fire_event("UI_INFO_MESSAGE", [text])


func add_system_line(text: String) -> void:
	_fire_chat("SYSTEM", text)


func show_area(area_id: int) -> void:
	_area = area_id
	fire_event("ZONE_CHANGED")
	fire_event("ZONE_CHANGED_NEW_AREA")
	fire_event("MINIMAP_ZONE_CHANGED")


func on_group_changed() -> void:
	fire_event("PARTY_MEMBERS_CHANGED")
	fire_event("PARTY_LEADER_CHANGED")
	if PartyFrame.is_raid:
		fire_event("RAID_ROSTER_UPDATE")


func save_variables() -> void:
	_save(_frame_saved, _account_dir().path_join("SavedVariables.lua"))
	for addon: String in _addon_saved:
		var names: Dictionary = _addon_saved[addon]
		_save(names["account"], _account_dir().path_join("SavedVariables").path_join(addon + ".lua"))
		_save(names["character"], _character_dir().path_join("SavedVariables").path_join(addon + ".lua"))


func _fire_entering() -> void:
	fire_event("PLAYER_ENTERING_WORLD")
	fire_event("UPDATE_BINDINGS")
	fire_event("UPDATE_CHAT_WINDOWS")
	run_lua("FCF_SelectDockFrame(DEFAULT_CHAT_FRAME)")
	for type: String in CHAT_COLORS:
		var color: Color = CHAT_COLORS[type]
		fire_event("UPDATE_CHAT_COLOR", [type, color.r, color.g, color.b])
	_snapshots.clear()
	_player_snapshot = []
	_poll_units()
	for event: String in [
		"PLAYER_TARGET_CHANGED", "PARTY_MEMBERS_CHANGED", "PLAYER_AURAS_CHANGED", "UNIT_PET",
		"UPDATE_BONUS_ACTIONBAR", "ACTIONBAR_PAGE_CHANGED", "UPDATE_SHAPESHIFT_FORMS",
		"PLAYER_XP_UPDATE", "UPDATE_EXHAUSTION", "PLAYER_MONEY", "PLAYER_UPDATE_RESTING",
		"SPELLS_CHANGED", "ACTIONBAR_UPDATE_COOLDOWN",
	]:
		fire_event(event, ["player"] if event == "UNIT_PET" else [])
	fire_event("CHARACTER_POINTS_CHANGED", [0, 0])
	fire_event("ACTIONBAR_SLOT_CHANGED", [0])
	show_area(_area)
	_bags_dirty = true


func _fire(event: String, args: Array) -> void:
	fire_event(event, args)


# The binding table: keys of the input actions the stock bindings replace, and chat's own keys.
func _key_bindings() -> Dictionary:
	var keys: Dictionary = FIXED_BINDINGS.duplicate()
	for action: String in ACTION_BINDINGS:
		if not InputMap.has_action(action):
			continue
		for event: InputEvent in InputMap.action_get_events(action):
			var key: InputEventKey = event as InputEventKey
			if key == null:
				continue
			var named: InputEventKey = key.duplicate()
			if named.keycode == KEY_NONE:
				named.keycode = named.physical_keycode
			var key_name: String = event_key_name(named)
			if not key_name.is_empty():
				keys[key_name] = ACTION_BINDINGS[action]
	return keys


func _follow_typing() -> void:
	if has_text_focus():
		if not _typing_proxy.has_focus():
			_typing_proxy.grab_focus()
	elif _typing_proxy.has_focus():
		_typing_proxy.release_focus()


# Saved variables.

func _account_dir() -> String:
	var account: String = StockGlue.logged_account if not StockGlue.logged_account.is_empty() else "Default"
	return WTF.path_join(account.to_upper())


func _character_dir() -> String:
	var realm: String = StockGlue.logged_realm if not StockGlue.logged_realm.is_empty() else "Default"
	return _account_dir().path_join(realm).path_join(_character)


func _addon_enabled(addon: String) -> bool:
	return _settings.get_value("addons", addon.to_lower(), true)


func _names_of(info: Dictionary, key: String) -> PackedStringArray:
	var names: PackedStringArray = []
	for each: String in str(info.get(key, "")).split(",", false):
		if not each.strip_edges().is_empty():
			names.append(each.strip_edges())
	return names


# The addon's files are in; its saved values go in before it hears ADDON_LOADED.
func _on_addon_files_loaded(addon: String, info: Dictionary) -> void:
	var names: Dictionary = {
		"account": _names_of(info, "SavedVariables"),
		"character": _names_of(info, "SavedVariablesPerCharacter"),
	}
	if names["account"].is_empty() and names["character"].is_empty():
		return
	_addon_saved[addon] = names
	_restore(names["account"], _account_dir().path_join("SavedVariables").path_join(addon + ".lua"))
	_restore(names["character"], _character_dir().path_join("SavedVariables").path_join(addon + ".lua"))


func _restore(names: PackedStringArray, path: String) -> void:
	if names.is_empty() or not FileAccess.file_exists(path):
		return
	run_lua(FileAccess.get_file_as_string(path), path.get_file())


func _save(names: PackedStringArray, path: String) -> void:
	if names.is_empty():
		return
	var text: Variant = call_lua("WowdotSerialize", [" ".join(names)])
	DirAccess.make_dir_recursive_absolute(path.get_base_dir())
	var file: FileAccess = FileAccess.open(path, FileAccess.WRITE)
	if file:
		file.store_string(str(text) if text != null else "")


# Units.

func unit_guid(unit: Variant) -> int:
	if not unit is String:
		return 0
	var token: String = (unit as String).to_lower()
	if token.length() > 6 and token.ends_with("target"):
		var base: String = token.left(-6)
		if base == "player":
			return _target()
		var of: int = unit_guid(base)
		return _session.get_field_guid(of, "UNIT_FIELD_TARGET") if of and _session.has_object(of) else 0
	match token:
		"player":
			return _session.get_player_guid()
		"target":
			return _target()
		"pet":
			return WowClient.pet.guid
	for prefix: String in ["partypet", "party", "raidpet", "raid"]:
		var number: String = token.substr(prefix.length())
		if token.begins_with(prefix) and number.is_valid_int():
			var guid: int = _group_member(prefix.begins_with("raid"), number.to_int())
			if prefix.ends_with("pet"):
				return _session.get_field_guid(guid, "UNIT_FIELD_SUMMON") if guid else 0
			return guid
	return 0


func _target() -> int:
	return hud.target() if hud else 0


func _group_member(raid: bool, number: int) -> int:
	var roster: Array[Dictionary] = []
	if raid:
		roster.append({"guid": _session.get_player_guid()})
		roster.append_array(PartyFrame.members)
	else:
		for member: Dictionary in PartyFrame.members:
			if not PartyFrame.is_raid or member.get("subgroup", 0) == PartyFrame.own_subgroup:
				roster.append(member)
	return roster[number - 1].get("guid", 0) if number >= 1 and number <= roster.size() else 0


func _member(guid: int) -> Dictionary:
	for member: Dictionary in PartyFrame.members:
		if member.get("guid", 0) == guid:
			return member
	return {}


func _seen(guid: int) -> bool:
	return guid != 0 and _session.has_object(guid)


func _field(guid: int, field: String) -> int:
	return _session.get_field(guid, field) if _seen(guid) else 0


func _is_player(guid: int) -> bool:
	return _seen(guid) and _session.get_object_type(guid) == Entities.ObjectType.PLAYER \
	or not _member(guid).is_empty()


func _power_type(guid: int) -> int:
	if not _seen(guid):
		return PartyFrame.member_stats.get(guid, {}).get("power_type", 0)
	return (_session.get_field(guid, "UNIT_FIELD_BYTES_0") >> 24) & 0xFF


func _health(guid: int, maximum: bool) -> int:
	if not _seen(guid):
		return PartyFrame.member_stats.get(guid, {}).get("max_health" if maximum else "health", 0)
	return _session.get_field(guid, "UNIT_FIELD_MAXHEALTH" if maximum else "UNIT_FIELD_HEALTH")


func _power(guid: int, maximum: bool) -> int:
	var type: int = _power_type(guid)
	var value: int
	if not _seen(guid):
		value = PartyFrame.member_stats.get(guid, {}).get("max_power" if maximum else "power", 0)
	else:
		var first: int = _session.field_index("UNIT_FIELD_MAXPOWER1" if maximum else "UNIT_FIELD_POWER1")
		value = _session.get_field(guid, first + type)
	return value / RAGE_SCALE if type == RAGE else value


func _unit_name(unit: Variant) -> Variant:
	var guid: int = unit_guid(unit)
	if guid == 0:
		return null
	var known: String = _session.get_object_name(guid) if _seen(guid) else ""
	if known.is_empty():
		known = _member(guid).get("name", "")
	return known if not known.is_empty() else WowStrings.get_text("UNKNOWNOBJECT", "Unknown")


func _unit_level(unit: Variant) -> int:
	var guid: int = unit_guid(unit)
	if _seen(guid) and _classification(guid) == WORLD_BOSS:
		return -1
	if not _seen(guid):
		return PartyFrame.member_stats.get(guid, {}).get("level", 0)
	return _session.get_field(guid, "UNIT_FIELD_LEVEL")


func _classification(guid: int) -> int:
	if not _seen(guid) or _session.get_object_type(guid) != Entities.ObjectType.UNIT:
		return 0
	return _session.get_creature_info(guid).get("rank", 0)


func _unit_class(unit: Variant) -> Variant:
	var guid: int = unit_guid(unit)
	if not _seen(guid):
		return null
	var class_id: int = (_session.get_field(guid, "UNIT_FIELD_BYTES_0") >> 8) & 0xFF
	return [CharacterOptions.class_label(class_id), CharacterOptions.class_file(class_id)]


func _unit_race(unit: Variant) -> Variant:
	var guid: int = unit_guid(unit)
	if not _is_player(guid) or not _seen(guid):
		return null
	var race: int = _session.get_field(guid, "UNIT_FIELD_BYTES_0") & 0xFF
	return [CharacterOptions.race_name(race), CharacterOptions.race_file(race)]


func _unit_sex(unit: Variant) -> int:
	var guid: int = unit_guid(unit)
	if not _seen(guid):
		return 1
	var gender: int = (_session.get_field(guid, "UNIT_FIELD_BYTES_0") >> 16) & 0xFF
	return 2 if gender == 0 else 3 if gender == 1 else 1


func _unit_faction(unit: Variant) -> Variant:
	var guid: int = unit_guid(unit)
	if not _is_player(guid) or not _seen(guid):
		return null
	match AreaInfo.player_group(_session.get_field(guid, "UNIT_FIELD_BYTES_0") & 0xFF):
		AreaInfo.FactionGroup.ALLIANCE:
			return ["Alliance", WowStrings.get_text("FACTION_ALLIANCE", "Alliance")]
		AreaInfo.FactionGroup.HORDE:
			return ["Horde", WowStrings.get_text("FACTION_HORDE", "Horde")]
	return null


func _reaction(unit: Variant, other: Variant) -> Variant:
	var from: int = unit_guid(unit)
	var to: int = unit_guid(other)
	if not _seen(from) or not _seen(to):
		return null
	return UnitReaction.between(_session, from, to)


func _is_dead(unit: Variant) -> bool:
	var guid: int = unit_guid(unit)
	return guid != 0 and _health(guid, false) <= 0 and _health(guid, true) > 0


func _is_ghost(unit: Variant) -> bool:
	return _field(unit_guid(unit), "PLAYER_FLAGS") & PLAYER_FLAG_GHOST != 0


func _is_connected(unit: Variant) -> bool:
	var guid: int = unit_guid(unit)
	var member: Dictionary = _member(guid)
	return guid != 0 and (member.is_empty() or member.get("online", true))


func _can_attack(unit: Variant, other: Variant) -> bool:
	var reaction: Variant = _reaction(unit, other)
	return reaction != null and reaction < UnitReaction.Reaction.FRIENDLY and not _is_dead(other)


func _player_controlled(unit: Variant) -> bool:
	var guid: int = unit_guid(unit)
	return _is_player(guid) or (_seen(guid) and _session.get_field_guid(guid, "UNIT_FIELD_SUMMONEDBY") != 0)


func _party_count() -> int:
	return PartyFrame.members.size() if not PartyFrame.is_raid else _group_size()


func _group_size() -> int:
	var count: int = 0
	for member: Dictionary in PartyFrame.members:
		if member.get("subgroup", 0) == PartyFrame.own_subgroup:
			count += 1
	return count


func _party_leader_index() -> int:
	for i: int in range(1, 5):
		if PartyFrame.leader != 0 and _group_member(false, i) == PartyFrame.leader:
			return i
	return 0


func _raid_roster_info(index: Variant) -> Variant:
	var guid: int = _group_member(true, int(index) if index != null else 0)
	if guid == 0:
		return null
	var member: Dictionary = _member(guid)
	var rank: int = 2 if guid == PartyFrame.leader else 1 if PartyFrame.is_assistant(guid) else 0
	var unit: String = "raid%d" % int(index)
	var class_info: Variant = _unit_class(unit)
	return [
		_unit_name(unit), rank, PartyFrame.subgroup_of(guid) + 1, _unit_level(unit),
		class_info[0] if class_info else null, class_info[1] if class_info else null,
		null, member.get("online", true) if not member.is_empty() else true, _is_dead(unit),
	]


func _raid_target_index(unit: Variant) -> Variant:
	var guid: int = unit_guid(unit)
	for icon: int in PartyFrame.target_icons:
		if PartyFrame.target_icons[icon] == guid and guid != 0:
			return icon + 1
	return null


func _select(unit: Variant) -> void:
	if hud:
		hud.unit_selected.emit(unit_guid(unit))


func _assist(unit: Variant) -> void:
	var guid: int = unit_guid(unit)
	if hud and _seen(guid):
		hud.unit_selected.emit(_session.get_field_guid(guid, "UNIT_FIELD_TARGET"))


func _combo_points() -> int:
	var player: int = _session.get_player_guid()
	if _target() == 0 or _session.get_field_guid(player, "PLAYER_FIELD_COMBO_TARGET") != _target():
		return 0
	return (_session.get_field(player, "PLAYER_FIELD_BYTES") >> 8) & 0xFF


func _rest_state() -> Array:
	var player: int = _session.get_player_guid()
	var state: int = (_session.get_field(player, "PLAYER_BYTES_2") >> 24) & 0xFF
	if state == 1:
		return [1, WowStrings.get_text("TUTORIAL_TITLE26", "Rested"), 2]
	return [2, WowStrings.get_text("PLAYER_STATUS_NORMAL", "Normal"), 1]


func _interact_distance(unit: Variant, index: Variant) -> bool:
	const DISTANCES: Array[float] = [0.0, 10.0, 11.11, 10.0, 28.0]
	var guid: int = unit_guid(unit)
	if not _seen(guid):
		return false
	var limit: float = DISTANCES[clampi(int(index) if index != null else 4, 1, 4)]
	var from: Vector3 = _session.get_object_position(_session.get_player_guid())
	return from.distance_to(_session.get_object_position(guid)) <= limit


# Actions, counted from 1 as the stock bars do.

func _action(slot: Variant) -> int:
	var buttons: PackedInt32Array = _session.get_action_buttons()
	var index: int = (int(slot) if slot != null else 0) - 1
	return buttons[index] if index >= 0 and index < buttons.size() else 0


func _action_kind(packed: int) -> int:
	return (packed >> 24) & 0xFF


func _action_spell(slot: Variant) -> int:
	var packed: int = _action(slot)
	return packed & ACTION_MASK if packed != 0 and _action_kind(packed) == 0 else 0


func _action_item(slot: Variant) -> int:
	var packed: int = _action(slot)
	return packed & ACTION_MASK if _action_kind(packed) == ACTION_ITEM else 0


func _texture_file(texture: Texture2D) -> Variant:
	var file: WowTexture = texture as WowTexture
	return file.file if file and not file.file.is_empty() else null


func _action_texture(slot: Variant) -> Variant:
	var packed: int = _action(slot)
	if packed == 0:
		return null
	match _action_kind(packed):
		ACTION_MACRO:
			return WowClient.macros.info(packed & ACTION_MASK).get(
				"icon", "Interface\\Icons\\INV_Misc_QuestionMark"
			)
		ACTION_ITEM:
			return _texture_file(Inventory.icon(packed & ACTION_MASK))
	var spell: int = packed & ACTION_MASK
	if spell == SPELL_ATTACK:
		var weapon: int = Inventory.entry(Inventory.equipped(Inventory.Slot.MAIN_HAND))
		if weapon:
			var weapon_icon: Variant = _texture_file(Inventory.icon(weapon))
			if weapon_icon:
				return weapon_icon
	return _texture_file(WowAssets.spells.icon(spell))


func _action_cooldown(slot: Variant) -> Array:
	var spell: int = _action_spell(slot)
	var cooldown: Vector2i = WowClient.cooldowns.get_cooldown(spell) if spell else Vector2i.ZERO
	if cooldown.x + cooldown.y <= Time.get_ticks_msec():
		return [0, 0, 1]
	return [cooldown.x / 1000.0, cooldown.y / 1000.0, 1]


func _action_in_range(slot: Variant) -> Variant:
	var spell: int = _action_spell(slot)
	if spell == 0 or _target() == 0:
		return null
	match WowAssets.spells.range_check(spell, _session.get_player_guid(), _target()):
		SpellInfo.RangeCheck.IN_RANGE:
			return 1
		SpellInfo.RangeCheck.OUT_OF_RANGE:
			return 0
	return null


func _action_has_range(slot: Variant) -> bool:
	var spell: int = _action_spell(slot)
	return spell != 0 and WowAssets.spells.range_check(
		spell, _session.get_player_guid(), _session.get_player_guid()
	) != SpellInfo.RangeCheck.NO_RANGE


func _action_usable(slot: Variant) -> Array:
	var item: int = _action_item(slot)
	if item:
		return [Inventory.item_count(item) > 0, false]
	return [_action(slot) != 0, false]


func _current_action(slot: Variant) -> bool:
	var spell: int = _action_spell(slot)
	return spell != 0 and (spell == _casting_spell or (spell == SPELL_ATTACK and _attacking))


func _action_count(slot: Variant) -> int:
	var item: int = _action_item(slot)
	return Inventory.item_count(item) if item else 0


func _action_text(slot: Variant) -> Variant:
	var packed: int = _action(slot)
	if _action_kind(packed) != ACTION_MACRO:
		return null
	return WowClient.macros.info(packed & ACTION_MASK).get("name", null)


func _use_action(slot: Variant, _check_cursor: Variant = null, _on_self: Variant = null) -> void:
	if _cursor != 0:
		_place_action(slot)
	elif hud and _action(slot) != 0:
		hud.action_used.emit(int(slot) - 1)


# The cursor carries an action as the bars store it; a spell is its id, the rest carry a type byte.
func _pickup_action(slot: Variant) -> void:
	var held: int = _action(slot)
	if _cursor != 0:
		_session.set_action_button(int(slot) - 1, _cursor)
	elif held != 0:
		_session.set_action_button(int(slot) - 1, 0)
	else:
		return
	_set_cursor(held)


func _place_action(slot: Variant) -> void:
	if _cursor != 0:
		_pickup_action(slot)


func _set_cursor(packed: int) -> void:
	_cursor = packed
	var icon: Variant = null
	if packed != 0:
		var kind: int = _action_kind(packed)
		if kind == ACTION_MACRO:
			icon = WowClient.macros.info(packed & ACTION_MASK).get("icon", null)
		elif kind == ACTION_ITEM:
			icon = _texture_file(Inventory.icon(packed & ACTION_MASK))
		else:
			icon = _texture_file(WowAssets.spells.icon(packed & ACTION_MASK))
	_cursor_icon.visible = icon != null
	if icon != null:
		_cursor_icon.texture = WowAssets.spells.icon_texture(str(icon).trim_suffix(".blp"))
	fire_event("ACTIONBAR_SHOWGRID" if packed != 0 else "ACTIONBAR_HIDEGRID")


func _can_drop_data(at_position: Vector2, data: Variant) -> bool:
	return data is Dictionary and (data.has("spell") or data.has("macro")) \
	and get_widget_at(at_position) >= 0


func _drop_data(at_position: Vector2, data: Variant) -> void:
	_set_cursor(data["spell"] if data.has("spell") else data["macro"] | ACTION_MACRO << 24)
	run_widget_script(get_widget_at(at_position), "OnReceiveDrag")
	if _cursor != 0:
		_set_cursor(0)


func _bonus_bar_offset() -> int:
	var form: int = (_field(_session.get_player_guid(), "UNIT_FIELD_BYTES_1") >> 16) & 0xFF
	return MainMenuBar.BONUS_PAGE_BY_FORM.get(form, MainMenuBar.PAGE_COUNT) - MainMenuBar.PAGE_COUNT


func _action_bar_toggles() -> Array:
	return _settings.get_value("bars", "toggles", [false, false, false, false])


func _set_action_bar_toggles(a: Variant, b: Variant, c: Variant, d: Variant) -> void:
	_settings.set_value("bars", "toggles", [a == 1 or a == true, b == 1 or b == true,
		c == 1 or c == true, d == 1 or d == true])
	_settings.save(SETTINGS_PATH)


# Stances and forms, ordered by form as ShapeshiftBar_Update reads them.
func _rebuild_forms() -> void:
	const EFFECT_AURA_COLUMN: int = 91
	const EFFECT_MISC_COLUMN: int = 106
	const AURA_MOD_SHAPESHIFT: int = 36
	var spells: WowDBC = WowDBC.open(WowAssets.archive, "Spell")
	_forms.clear()
	for spell_id: int in _session.get_known_spells():
		var row: int = spells.find(spell_id)
		for i: int in 3:
			if row >= 0 and spells.get_uint(row, EFFECT_AURA_COLUMN + i) == AURA_MOD_SHAPESHIFT:
				_forms.append(Vector2i(spells.get_uint(row, EFFECT_MISC_COLUMN + i), spell_id))
	_forms.sort()


func _form_info(index: Variant) -> Variant:
	var i: int = (int(index) if index != null else 0) - 1
	if i < 0 or i >= _forms.size():
		return null
	var form: int = (_field(_session.get_player_guid(), "UNIT_FIELD_BYTES_1") >> 16) & 0xFF
	var spell: int = _forms[i].y
	return [_texture_file(WowAssets.spells.icon(spell)), WowAssets.spells.spell_name(spell),
		_forms[i].x == form, true]


func _cast_form(index: Variant) -> void:
	var i: int = (int(index) if index != null else 0) - 1
	if i >= 0 and i < _forms.size():
		_session.cast_spell(_forms[i].y)


# Auras.

func _auras(guid: int) -> Array:
	return _session.get_auras(guid) if _seen(guid) else []


func _shown_auras(guid: int, harmful: bool, passive: bool = false) -> Array:
	var shown: Array = []
	for aura: Dictionary in _auras(guid):
		if aura.get("harmful", false) != harmful:
			continue
		if not passive and WowAssets.spells.is_passive(aura["spell"]):
			continue
		shown.append(aura)
	return shown


func _player_aura(index: Variant) -> Dictionary:
	if index == null:
		return {}
	for aura: Dictionary in _auras(_session.get_player_guid()):
		if aura.get("slot", -1) == int(index):
			return aura
	return {}


func _player_buff(id: Variant, filter: Variant) -> Array:
	var wanted: String = str(filter) if filter else "HELPFUL"
	var shown: Array = _shown_auras(_session.get_player_guid(), "HARMFUL" in wanted, "PASSIVE" in wanted)
	var index: int = int(id) if id != null else 0
	if index < 0 or index >= shown.size():
		return [-1, false]
	return [shown[index]["slot"], shown[index].get("max_duration_msec", 0) <= 0]


func _player_buff_texture(index: Variant) -> Variant:
	var aura: Dictionary = _player_aura(index)
	return _texture_file(WowAssets.spells.icon(aura["spell"])) if not aura.is_empty() else null


func _player_buff_applications(index: Variant) -> int:
	return _player_aura(index).get("stacks", 0)


func _player_buff_dispel(index: Variant) -> Variant:
	var aura: Dictionary = _player_aura(index)
	if aura.is_empty():
		return null
	var type: int = WowAssets.spells.dispel_type(aura["spell"])
	return DISPEL_TYPES[type] if type < DISPEL_TYPES.size() else null


func _player_buff_time_left(index: Variant) -> float:
	var aura: Dictionary = _player_aura(index)
	if aura.is_empty() or aura.get("max_duration_msec", 0) <= 0:
		return 0.0
	return maxf(0.0, (aura.get("ends_msec", 0) - Time.get_ticks_msec()) / 1000.0)


func _cancel_player_buff(index: Variant) -> void:
	var aura: Dictionary = _player_aura(index)
	if not aura.is_empty():
		_session.cancel_aura(aura["spell"])


func _unit_aura(unit: Variant, index: Variant, harmful: bool) -> Variant:
	var shown: Array = _shown_auras(unit_guid(unit), harmful)
	var i: int = (int(index) if index != null else 0) - 1
	if i < 0 or i >= shown.size():
		return null
	var aura: Dictionary = shown[i]
	var texture: Variant = _texture_file(WowAssets.spells.icon(aura["spell"]))
	if not harmful:
		return [texture, aura.get("stacks", 0)]
	var type: int = WowAssets.spells.dispel_type(aura["spell"])
	return [texture, aura.get("stacks", 0), DISPEL_TYPES[type] if type < DISPEL_TYPES.size() else null]


# Inventory.

func _inventory_item(slot: Variant) -> int:
	var stock_slot: int = int(slot) if slot != null else 0
	return Inventory.equipped((stock_slot - 1) as Inventory.Slot) if stock_slot >= 1 else 0


func _inventory_texture(_unit: Variant, slot: Variant) -> Variant:
	var item: int = _inventory_item(slot)
	return _texture_file(Inventory.icon(Inventory.entry(item))) if item else null


func _inventory_slot_info(slot_name: Variant) -> Variant:
	var entry: Array = INVENTORY_SLOTS.get(str(slot_name), [])
	return [entry[0], EMPTY_SLOT_ART + entry[1]] if not entry.is_empty() else null


func _container_item_info(bag: Variant, slot: Variant) -> Variant:
	var item: int = Inventory.container_item(int(bag), int(slot) - 1)
	if item == 0:
		return null
	var entry: int = Inventory.entry(item)
	var quality: int = _session.get_item_info(entry).get("quality", 1)
	return [_texture_file(Inventory.icon(entry)), Inventory.stack_count(item), false, quality, false]


func _quality_color(quality: Variant) -> Array:
	var color: Color = GameTooltip.QUALITY_COLORS[clampi(int(quality) if quality != null else 1, 0, 6)]
	return [color.r, color.g, color.b, "|cff" + color.to_html(false)]


# Chat.

func _fire_chat(type: String, text: String, sender: String = "", language: String = "",
		channel_number: int = 0, channel: String = "") -> void:
	var full: String = "%d. %s" % [channel_number, channel] if not channel.is_empty() else ""
	fire_event("CHAT_MSG_" + type, [text, sender, language, full, "", "", 0, channel_number, channel])


func _on_chat_received(line: Dictionary) -> void:
	var type: String = CHAT_TYPES.get(line.get("type", -1), "")
	if type.is_empty():
		return
	var channel: String = line.get("channel", "")
	if not channel.is_empty() and channel not in _chat_channels:
		_chat_channels.append(channel)
		var frame: int = get_widget_id("ChatFrame1")
		if frame >= 0:
			call_lua("ChatFrame_AddChannel", [{"widget": frame}, channel])
	_fire_chat(type, line.get("text", ""), line.get("sender_name", ""),
		_language_name(line.get("language", 0)),
		Channels.number_of(channel) if not channel.is_empty() else 0, channel)


func _on_combat_logged(event: CombatEvents.CombatEvent) -> void:
	var text: String = CombatLogFrame.format(event)
	if not text.is_empty():
		_fire_chat("COMBAT_MISC_INFO", text)


func _language_name(language: Variant) -> String:
	var id: int = int(language) if language != null else 0
	if id == 0:
		return UNIVERSAL
	var row: int = _languages.find(id) if _languages else -1
	return _languages.get_text(row, "Name") if row >= 0 else ""


func _default_language() -> Variant:
	var race: int = _field(_session.get_player_guid(), "UNIT_FIELD_BYTES_0") & 0xFF
	if race == 0:
		return null
	var horde: bool = AreaInfo.player_group(race) == AreaInfo.FactionGroup.HORDE
	return _language_name(HORDE_LANGUAGE if horde else ALLIANCE_LANGUAGE)


func _send_chat(message: Variant, type: Variant, _language: Variant = null, target: Variant = null) -> void:
	var text: String = str(message) if message != null else ""
	var kind: String = str(type).to_upper() if type != null else "SAY"
	var chat_type: int = CHAT_TYPES.find_key(kind) if CHAT_TYPES.values().has(kind) else WowSession.CHAT_SAY
	var to: String = ""
	if kind == "CHANNEL":
		to = Channels.name_at(int(target)) if str(target).is_valid_int() else str(target)
	elif target != null:
		to = str(target)
	_session.send_chat(chat_type, text, to)


func _channel_list() -> Array:
	var list: Array = []
	for channel: String in Channels.joined:
		list.append_array([Channels.number_of(channel), channel])
	return list


func _channel_name(which: Variant) -> Array:
	var channel: String = Channels.name_at(int(which)) if str(which).is_valid_int() else str(which)
	if channel.is_empty() or channel not in Channels.joined:
		return [0, null]
	return [Channels.number_of(channel), channel]


func _load_chat_windows() -> void:
	var defaults: Array[Dictionary] = []
	for i: int in 7:
		defaults.append({
			"name": [WowStrings.get_text("GENERAL", "General"),
				WowStrings.get_text("COMBAT_LOG", "Combat Log")][i] if i < 2 else "",
			"size": 0, "color": Color(0, 0, 0), "alpha": 0.25, "shown": i < 2, "locked": i < 2,
			"docked": i + 1 if i < 2 else 0,
			"groups": GENERAL_GROUPS.duplicate() if i == 0 else COMBAT_GROUPS.duplicate() if i == 1 else [],
		})
	var saved: Array = _settings.get_value("chat", _character, []) if not _character.is_empty() else []
	for i: int in mini(saved.size(), defaults.size()):
		defaults[i].merge(saved[i], true)
	_chat_windows = defaults


func _save_chat_windows() -> void:
	if _character.is_empty():
		return
	_settings.set_value("chat", _character, _chat_windows)
	_settings.save(SETTINGS_PATH)


func _window(id: Variant) -> Dictionary:
	var i: int = (int(id) if id != null else 0) - 1
	return _chat_windows[i] if i >= 0 and i < _chat_windows.size() else {}


func _chat_window_info(id: Variant) -> Variant:
	var window: Dictionary = _window(id)
	if window.is_empty():
		return null
	var color: Color = window["color"]
	return [window["name"], window["size"], color.r, color.g, color.b, window["alpha"],
		window["shown"], window["locked"], window["docked"] if window["docked"] else null]


func _set_window(id: Variant, key: String, value: Variant) -> void:
	var window: Dictionary = _window(id)
	if not window.is_empty():
		window[key] = value
		_save_chat_windows()


func _window_groups(id: Variant, group: Variant, adding: bool) -> void:
	var window: Dictionary = _window(id)
	if window.is_empty() or group == null:
		return
	var groups: Array = window["groups"]
	if adding and str(group) not in groups:
		groups.append(str(group))
	elif not adding:
		groups.erase(str(group))
	_save_chat_windows()


func _emote(token: Variant, _unit: Variant = null) -> void:
	var text_emote: int = Emotes.find(str(token)) if token != null else 0
	if text_emote != 0 and hud:
		hud._on_emote_requested(text_emote)


# Tooltips: the HUD's tooltip code writes lines the stock GameTooltip shows.

func _tooltip_query(_tooltip_widget: Variant, method: String, a: Variant = null, b: Variant = null) -> Array:
	_tooltip.lines = []
	match method:
		"SetAction":
			var spell: int = _action_spell(a)
			var item: int = _action_item(a)
			if spell:
				_tooltip.set_spell(self, spell)
			elif item:
				_tooltip.set_item(null, item)
			elif _action_text(a) != null:
				_tooltip.add_line(str(_action_text(a)))
		"SetUnit":
			_tooltip.set_unit(self, unit_guid(a))
		"SetPlayerBuff":
			var aura: Dictionary = _player_aura(a)
			if not aura.is_empty():
				_tooltip.set_aura(null, aura["spell"], GameTooltip.TooltipAnchor.DEFAULT)
		"SetUnitBuff", "SetUnitDebuff":
			var shown: Array = _shown_auras(unit_guid(a), method == "SetUnitDebuff")
			var i: int = (int(b) if b != null else 0) - 1
			if i >= 0 and i < shown.size():
				_tooltip.set_aura(null, shown[i]["spell"], GameTooltip.TooltipAnchor.DEFAULT)
		"SetInventoryItem":
			var equipped: int = _inventory_item(b)
			if equipped:
				_tooltip.set_item(null, Inventory.entry(equipped), equipped)
		"SetBagItem":
			var held: int = Inventory.container_item(int(a), int(b) - 1)
			if held:
				_tooltip.set_item(null, Inventory.entry(held), held)
		"SetShapeshift":
			var i: int = (int(a) if a != null else 0) - 1
			if i >= 0 and i < _forms.size():
				_tooltip.set_spell(self, _forms[i].y)
	return _tooltip.lines


# Portraits: a model render masked round, which the stock texture region draws.

func _set_portrait(texture: Variant, unit: Variant) -> void:
	if not texture is Dictionary or not texture.has("widget"):
		return
	var id: int = texture["widget"]
	if not _portraits.has(id):
		var portrait: UnitPortrait = UnitFrame.PORTRAIT.instantiate()
		var masked: SubViewport = SubViewport.new()
		masked.transparent_bg = true
		masked.size = portrait.size
		masked.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		var face: TextureRect = TextureRect.new()
		var mask: ShaderMaterial = ShaderMaterial.new()
		mask.shader = UnitFrame.PORTRAIT_MASK
		face.material = mask
		face.texture = portrait.get_texture()
		face.set_anchors_preset(Control.PRESET_FULL_RECT)
		face.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		masked.add_child(face)
		_model_holder.add_child(portrait)
		_model_holder.add_child(masked)
		_portraits[id] = [portrait, masked, 0]
	var entry: Array = _portraits[id]
	var guid: int = unit_guid(unit)
	var display: int = _field(guid, "UNIT_FIELD_DISPLAYID")
	if entry[2] != guid * 31 + display:
		entry[2] = guid * 31 + display
		(entry[0] as UnitPortrait).show_unit(guid)
	set_widget_texture_id(id, (entry[1] as SubViewport).get_texture())


# Panels the HUD keeps.

func _panel(panel: Variant, arg: Variant = null) -> void:
	if hud:
		hud.open_stock_panel(str(panel), arg)


# Events.

func _unit_tokens(guid: int) -> Array[String]:
	var tokens: Array[String] = []
	if guid == 0:
		return tokens
	for token: String in PANEL_UNITS:
		if unit_guid(token) == guid:
			tokens.append(token)
	return tokens


func _unit_snapshot(guid: int) -> Array:
	if guid == 0:
		return []
	return [
		guid, _health(guid, false), _health(guid, true), _power_type(guid), _power(guid, false),
		_power(guid, true), _field(guid, "UNIT_FIELD_LEVEL"), _field(guid, "UNIT_FIELD_FLAGS"),
		_field(guid, "UNIT_DYNAMIC_FLAGS"), _field(guid, "UNIT_FIELD_DISPLAYID"),
		_session.get_object_name(guid) if _seen(guid) else "",
	]


# Diffs each unit against the last poll, firing what the stock client fires on those changes.
func _poll_units() -> void:
	var party_changed: bool = false
	for token: String in PANEL_UNITS:
		var now: Array = _unit_snapshot(unit_guid(token))
		var before: Array = _snapshots.get(token, [])
		_snapshots[token] = now
		if now == before:
			continue
		if now.is_empty() or before.is_empty() or now[0] != before[0]:
			match token:
				"target":
					fire_event("PLAYER_TARGET_CHANGED")
				"targettarget":
					fire_event("UNIT_TARGET", ["target"])
				"pet":
					fire_event("UNIT_PET", ["player"])
				_:
					party_changed = party_changed or token.begins_with("party")
			continue
		_fire_unit_changes(token, before, now)
	if party_changed:
		fire_event("PARTY_MEMBERS_CHANGED")
	_poll_player()


func _fire_unit_changes(token: String, before: Array, now: Array) -> void:
	if before[1] != now[1]:
		fire_event("UNIT_HEALTH", [token])
	if before[2] != now[2]:
		fire_event("UNIT_MAXHEALTH", [token])
	var power: String = POWER_EVENTS[clampi(now[3], 0, POWER_EVENTS.size() - 1)]
	if before[3] != now[3]:
		fire_event("UNIT_DISPLAYPOWER", [token])
	if before[4] != now[4]:
		fire_event("UNIT_" + power, [token])
		if token == "player":
			fire_event("ACTIONBAR_UPDATE_USABLE")
	if before[5] != now[5]:
		fire_event("UNIT_MAX" + power, [token])
	if before[6] != now[6]:
		fire_event("UNIT_LEVEL", [token])
	if before[7] != now[7]:
		fire_event("UNIT_FLAGS", [token])
		fire_event("UNIT_FACTION", [token])
		if token == "player" and (before[7] ^ now[7]) & UNIT_FLAG_IN_COMBAT:
			fire_event("PLAYER_REGEN_DISABLED" if now[7] & UNIT_FLAG_IN_COMBAT else "PLAYER_REGEN_ENABLED")
	if before[8] != now[8]:
		fire_event("UNIT_DYNAMIC_FLAGS", [token])
	if before[9] != now[9]:
		fire_event("UNIT_MODEL_CHANGED", [token])
		fire_event("UNIT_PORTRAIT_UPDATE", [token])
	if before[10] != now[10]:
		fire_event("UNIT_NAME_UPDATE", [token])


func _poll_player() -> void:
	var player: int = _session.get_player_guid()
	if not _seen(player):
		return
	var now: Array = [
		_session.get_field(player, "PLAYER_XP"), _session.get_field(player, "PLAYER_NEXT_LEVEL_XP"),
		_session.get_field(player, "PLAYER_REST_STATE_EXPERIENCE"),
		_session.get_field(player, "PLAYER_FIELD_COINAGE"), _session.get_field(player, "PLAYER_FLAGS"),
		_combo_points(), _session.get_field(player, "PLAYER_CHARACTER_POINTS1"),
		(_session.get_field(player, "UNIT_FIELD_BYTES_1") >> 16) & 0xFF,
	]
	var before: Array = _player_snapshot
	_player_snapshot = now
	if before.is_empty() or before == now:
		return
	if before[0] != now[0] or before[1] != now[1]:
		fire_event("PLAYER_XP_UPDATE", ["player"])
	if before[2] != now[2]:
		fire_event("UPDATE_EXHAUSTION")
	if before[3] != now[3]:
		fire_event("PLAYER_MONEY")
	if (before[4] ^ now[4]) & PLAYER_FLAG_RESTING:
		fire_event("PLAYER_UPDATE_RESTING")
	if before[5] != now[5]:
		fire_event("PLAYER_COMBO_POINTS")
	if before[6] != now[6]:
		fire_event("CHARACTER_POINTS_CHANGED", [now[6] - before[6], 0])
	if before[7] != now[7]:
		fire_event("UPDATE_SHAPESHIFT_FORMS")
		fire_event("UPDATE_BONUS_ACTIONBAR")
		fire_event("ACTIONBAR_UPDATE_STATE")


func _on_object_updated(guid: int) -> void:
	if guid != _session.get_player_guid() and _session.get_object_type(guid) in [1, 2]:
		_bags_dirty = true


func _on_auras_changed(guid: int) -> void:
	for token: String in _unit_tokens(guid):
		fire_event("UNIT_AURA", [token])
	if guid == _session.get_player_guid():
		fire_event("PLAYER_AURAS_CHANGED")


func _is_me(guid: int) -> bool:
	return guid == _session.get_player_guid()


func _on_cast_started(caster: int, spell_id: int, cast_time_msec: int) -> void:
	if not _is_me(caster):
		return
	_casting_spell = spell_id
	if cast_time_msec > 0:
		fire_event("SPELLCAST_START", [WowAssets.spells.spell_name(spell_id), cast_time_msec])
	fire_event("ACTIONBAR_UPDATE_STATE")


func _on_cast_finished(caster: int, _spell_id: int, _targets: PackedInt64Array = []) -> void:
	if not _is_me(caster):
		return
	_casting_spell = 0
	if not _channeling:
		fire_event("SPELLCAST_STOP")
	fire_event("ACTIONBAR_UPDATE_STATE")


func _on_cast_failed(caster: int, _spell_id: int, _reason: int) -> void:
	if not _is_me(caster):
		return
	var was_casting: bool = _casting_spell != 0
	_casting_spell = 0
	if _channeling:
		_channeling = false
		fire_event("SPELLCAST_CHANNEL_STOP")
	elif was_casting:
		fire_event("SPELLCAST_INTERRUPTED")
	fire_event("ACTIONBAR_UPDATE_STATE")


func _on_cast_delayed(caster: int, delay_msec: int) -> void:
	if _is_me(caster):
		fire_event("SPELLCAST_DELAYED", [delay_msec])


func _on_channel_started(spell_id: int, duration_msec: int) -> void:
	_channeling = true
	fire_event("SPELLCAST_CHANNEL_START", [duration_msec, WowAssets.spells.spell_name(spell_id)])


func _on_channel_updated(remaining_msec: int) -> void:
	if remaining_msec <= 0:
		_channeling = false
		fire_event("SPELLCAST_CHANNEL_STOP")
	else:
		fire_event("SPELLCAST_CHANNEL_UPDATE", [remaining_msec])


func _on_attack_changed(attacker: int, _victim: int, attacking: bool) -> void:
	if _is_me(attacker):
		_attacking = attacking
		fire_event("PLAYER_ENTER_COMBAT" if attacking else "PLAYER_LEAVE_COMBAT")


func _on_spells_changed() -> void:
	_rebuild_forms()
	fire_event("SPELLS_CHANGED")
	fire_event("UPDATE_SHAPESHIFT_FORMS")


func _on_cooldowns_changed() -> void:
	fire_event("ACTIONBAR_UPDATE_COOLDOWN")
	fire_event("SPELL_UPDATE_COOLDOWN")


func _on_leveled_up(level: int, health: int, mana: int, talents: int = 0, a: int = 0, b: int = 0,
		c: int = 0, d: int = 0, e: int = 0) -> void:
	fire_event("PLAYER_LEVEL_UP", [level, health, mana, talents, a, b, c, d, e])


func _on_name_received(guid: int, _name: String) -> void:
	for token: String in _unit_tokens(guid):
		fire_event("UNIT_NAME_UPDATE", [token])


# The engine functions the HUD's FrameXML files call.
func _register_all() -> void:
	var functions: Dictionary[String, Callable] = {
		"WowdotTooltip": _tooltip_query,
		"WowdotPanel": _panel,
		"RegisterForSave": func(variable: Variant) -> void:
			if variable != null and str(variable) not in _frame_saved:
				_frame_saved.append(str(variable)),
		# Units.
		"UnitExists": func(unit: Variant) -> bool: return unit_guid(unit) != 0,
		"UnitName": _unit_name,
		"UnitLevel": _unit_level,
		"UnitHealth": func(unit: Variant) -> int: return _health(unit_guid(unit), false),
		"UnitHealthMax": func(unit: Variant) -> int: return _health(unit_guid(unit), true),
		"UnitMana": func(unit: Variant) -> int: return _power(unit_guid(unit), false),
		"UnitManaMax": func(unit: Variant) -> int: return _power(unit_guid(unit), true),
		"UnitPowerType": func(unit: Variant) -> int: return _power_type(unit_guid(unit)),
		"UnitClass": _unit_class,
		"UnitRace": _unit_race,
		"UnitSex": _unit_sex,
		"UnitFactionGroup": _unit_faction,
		"UnitReaction": _reaction,
		"UnitIsPlayer": func(unit: Variant) -> bool: return _is_player(unit_guid(unit)),
		"UnitIsUnit": func(a: Variant, b: Variant) -> bool:
			return unit_guid(a) != 0 and unit_guid(a) == unit_guid(b),
		"UnitIsDead": _is_dead,
		"UnitIsGhost": _is_ghost,
		"UnitIsDeadOrGhost": func(unit: Variant) -> bool: return _is_dead(unit) or _is_ghost(unit),
		"UnitIsCorpse": func(_unit: Variant) -> bool: return false,
		"UnitIsConnected": _is_connected,
		"UnitIsVisible": func(unit: Variant) -> bool: return _seen(unit_guid(unit)),
		"UnitIsTapped": func(unit: Variant) -> bool:
			return _field(unit_guid(unit), "UNIT_DYNAMIC_FLAGS") & DYNAMIC_FLAG_TAPPED != 0,
		"UnitIsTappedByPlayer": func(unit: Variant) -> bool:
			return _field(unit_guid(unit), "UNIT_DYNAMIC_FLAGS") & DYNAMIC_FLAG_TAPPED_BY_PLAYER != 0,
		"UnitIsPVP": func(unit: Variant) -> bool:
			return _field(unit_guid(unit), "UNIT_FIELD_FLAGS") & UNIT_FLAG_PVP != 0,
		"UnitIsPVPFreeForAll": func(unit: Variant) -> bool:
			return _is_player(unit_guid(unit)) and _field(unit_guid(unit), "PLAYER_FLAGS") & PLAYER_FLAG_FFA_PVP != 0,
		"UnitAffectingCombat": func(unit: Variant) -> bool:
			return _field(unit_guid(unit), "UNIT_FIELD_FLAGS") & UNIT_FLAG_IN_COMBAT != 0,
		"UnitOnTaxi": func(unit: Variant) -> bool:
			return _field(unit_guid(unit), "UNIT_FIELD_FLAGS") & UNIT_FLAG_TAXI_FLIGHT != 0,
		"UnitIsFriend": func(a: Variant, b: Variant) -> bool:
			var reaction: Variant = _reaction(a, b)
			return reaction != null and reaction >= UnitReaction.Reaction.FRIENDLY,
		"UnitIsEnemy": func(a: Variant, b: Variant) -> bool:
			var reaction: Variant = _reaction(a, b)
			return reaction != null and reaction <= UnitReaction.Reaction.HOSTILE,
		"UnitCanAttack": _can_attack,
		"UnitCanCooperate": func(a: Variant, b: Variant) -> bool:
			var reaction: Variant = _reaction(a, b)
			return _is_player(unit_guid(a)) and _is_player(unit_guid(b)) and reaction != null \
			and reaction >= UnitReaction.Reaction.FRIENDLY,
		"UnitPlayerControlled": _player_controlled,
		"UnitClassification": func(unit: Variant) -> String:
			return CLASSIFICATIONS[clampi(_classification(unit_guid(unit)), 0, CLASSIFICATIONS.size() - 1)],
		"UnitIsPlusMob": func(unit: Variant) -> bool: return _classification(unit_guid(unit)) != 0,
		"UnitIsCivilian": func(_unit: Variant) -> bool: return false,
		"UnitIsCharmed": func(unit: Variant) -> bool:
			return _seen(unit_guid(unit)) and _session.get_field_guid(unit_guid(unit), "UNIT_FIELD_CHARMEDBY") != 0,
		"UnitInParty": func(unit: Variant) -> bool:
			return _is_me(unit_guid(unit)) or not _member(unit_guid(unit)).is_empty(),
		"UnitInRaid": func(unit: Variant) -> Variant:
			return 1 if PartyFrame.is_raid and not _member(unit_guid(unit)).is_empty() else null,
		"UnitPlayerOrPetInParty": func(unit: Variant) -> bool: return not _member(unit_guid(unit)).is_empty(),
		"UnitIsPartyLeader": func(unit: Variant) -> bool:
			return unit_guid(unit) != 0 and unit_guid(unit) == PartyFrame.leader,
		"UnitXP": func(_unit: Variant) -> int: return _field(_session.get_player_guid(), "PLAYER_XP"),
		"UnitXPMax": func(_unit: Variant) -> int:
			return _field(_session.get_player_guid(), "PLAYER_NEXT_LEVEL_XP"),
		"UnitCharacterPoints": func(_unit: Variant) -> Array:
			return [_field(_session.get_player_guid(), "PLAYER_CHARACTER_POINTS1"),
				_field(_session.get_player_guid(), "PLAYER_CHARACTER_POINTS2")],
		"UnitCreatureType": func(_unit: Variant) -> Variant: return null,
		"UnitBuff": func(unit: Variant, index: Variant) -> Variant: return _unit_aura(unit, index, false),
		"UnitDebuff": func(unit: Variant, index: Variant) -> Variant: return _unit_aura(unit, index, true),
		"TargetUnit": _select,
		"ClearTarget": func() -> void: _select(null),
		"AssistUnit": _assist,
		"CheckInteractDistance": _interact_distance,
		"GetComboPoints": _combo_points,
		"IsResting": func() -> bool:
			return _field(_session.get_player_guid(), "PLAYER_FLAGS") & PLAYER_FLAG_RESTING != 0,
		"GetXPExhaustion": func() -> Variant:
			var rest: int = _field(_session.get_player_guid(), "PLAYER_REST_STATE_EXPERIENCE")
			return rest if rest > 0 else null,
		"GetRestState": _rest_state,
		"GetMoney": Inventory.money,
		"GetPVPDesired": func() -> bool: return false,
		"HasPetUI": func() -> bool: return WowClient.pet.guid != 0,
		"PetHasActionBar": func() -> bool: return false,
		"HasKey": func() -> bool: return true,
		"IsMacClient": func() -> bool: return OS.get_name() == "macOS",
		"PlayerHasSpells": func() -> bool: return not _session.get_known_spells().is_empty(),
		"GetNetStats": func() -> Array: return [0, 0, _session.get_latency()],
		# Group.
		"GetNumPartyMembers": _party_count,
		"GetNumRaidMembers": func() -> int: return PartyFrame.members.size() + 1 if PartyFrame.is_raid else 0,
		"GetPartyMember": func(index: Variant) -> Variant:
			return 1 if _group_member(false, int(index) if index != null else 0) != 0 else null,
		"GetPartyLeaderIndex": _party_leader_index,
		"IsPartyLeader": PartyFrame.is_leader,
		"IsRaidLeader": func() -> bool: return PartyFrame.is_raid and PartyFrame.is_leader(),
		"IsRaidOfficer": func() -> bool:
			return PartyFrame.is_raid and PartyFrame.is_assistant(_session.get_player_guid()),
		"GetRaidRosterInfo": _raid_roster_info,
		"GetRaidTargetIndex": _raid_target_index,
		"GetLootMethod": func() -> Array: return ["freeforall", null, null],
		"InviteByName": func(who: Variant) -> void: PartyFrame.invite(str(who)),
		"UninviteByName": func(who: Variant) -> void: PartyFrame.uninvite(str(who)),
		"LeaveParty": PartyFrame.leave,
		# Actions.
		"HasAction": func(slot: Variant) -> bool: return _action(slot) != 0,
		"GetActionTexture": _action_texture,
		"GetActionCooldown": _action_cooldown,
		"GetActionCount": _action_count,
		"GetActionText": _action_text,
		"IsActionInRange": _action_in_range,
		"ActionHasRange": _action_has_range,
		"IsUsableAction": _action_usable,
		"IsCurrentAction": _current_action,
		"IsAttackAction": func(slot: Variant) -> bool: return _action_spell(slot) == SPELL_ATTACK,
		"IsAutoRepeatAction": func(_slot: Variant) -> bool: return false,
		"IsEquippedAction": func(_slot: Variant) -> bool: return false,
		"IsConsumableAction": func(slot: Variant) -> bool: return _action_item(slot) != 0,
		"UseAction": _use_action,
		"PickupAction": _pickup_action,
		"PlaceAction": _place_action,
		"CursorHasSpell": func() -> bool: return _cursor != 0 and _action_kind(_cursor) != ACTION_ITEM,
		"CursorHasItem": func() -> bool: return _cursor != 0 and _action_kind(_cursor) == ACTION_ITEM,
		"CursorHasMoney": func() -> bool: return false,
		"ClearCursor": func() -> void: _set_cursor(0),
		"ResetCursor": _nothing,
		"SetCursor": func(_cursor_name: Variant) -> void: pass,
		"GetBonusBarOffset": _bonus_bar_offset,
		"GetActionBarToggles": _action_bar_toggles,
		"SetActionBarToggles": _set_action_bar_toggles,
		"GetNumShapeshiftForms": func() -> int: return _forms.size(),
		"GetShapeshiftFormInfo": _form_info,
		"GetShapeshiftFormCooldown": func(_index: Variant) -> Array: return [0, 0, 1],
		"CastShapeshiftForm": _cast_form,
		"SpellIsTargeting": func() -> bool: return false,
		"SpellStopCasting": func() -> void:
			if _casting_spell:
				_session.cancel_cast(_casting_spell),
		# Buffs.
		"GetPlayerBuff": _player_buff,
		"GetPlayerBuffTexture": _player_buff_texture,
		"GetPlayerBuffApplications": _player_buff_applications,
		"GetPlayerBuffDispelType": _player_buff_dispel,
		"GetPlayerBuffTimeLeft": _player_buff_time_left,
		"CancelPlayerBuff": _cancel_player_buff,
		"GetWeaponEnchantInfo": func() -> Variant: return null,
		# Inventory.
		"GetInventoryItemTexture": _inventory_texture,
		"GetInventorySlotInfo": _inventory_slot_info,
		"GetInventoryItemCount": func(_unit: Variant, slot: Variant) -> int:
			var item: int = _inventory_item(slot)
			return Inventory.stack_count(item) if item else 0,
		"GetContainerNumSlots": func(bag: Variant) -> int:
			return Inventory.container_size(int(bag)) if bag != null else 0,
		"GetContainerItemInfo": _container_item_info,
		"GetItemQualityColor": _quality_color,
		"GetBagName": func(bag: Variant) -> Variant:
			var item: int = Inventory.container_of(int(bag)) if bag != null and int(bag) > 0 else 0
			return _session.get_item_info(Inventory.entry(item)).get("name", null) if item else null,
		"PutItemInBag": func(_slot: Variant) -> bool: return false,
		"PutItemInBackpack": func() -> bool: return false,
		"KeyRingButtonIDToInvSlotID": func(id: Variant) -> int:
			return Inventory.wire_keyring_start + (int(id) if id != null else 0),
		# Chat.
		"SendChatMessage": _send_chat,
		"GetChannelList": _channel_list,
		"GetChannelName": _channel_name,
		"JoinChannelByName": func(channel: Variant, password: Variant = null, _frame: Variant = null) -> void:
			if channel != null:
				Channels.join(str(channel), str(password) if password != null else ""),
		"LeaveChannelByName": func(channel: Variant) -> void:
			if channel != null:
				Channels.leave(str(channel)),
		"GetDefaultLanguage": _default_language,
		"GetNumLanguages": func() -> int: return 1,
		"GetNumLaguages": func() -> int: return 1,
		"GetLanguageByIndex": func(_index: Variant) -> Variant: return _default_language(),
		"GetChatTypeIndex": func(type: Variant) -> int:
			return CHAT_TYPES.find_key(str(type)) if CHAT_TYPES.values().has(str(type)) else 0,
		"GetChatWindowInfo": _chat_window_info,
		"GetChatWindowMessages": func(id: Variant) -> Array: return _window(id).get("groups", []).duplicate(),
		"GetChatWindowChannels": func(_id: Variant) -> Array: return [],
		"AddChatWindowChannel": func(_id: Variant, _channel: Variant) -> int: return 0,
		"RemoveChatWindowChannel": func(_id: Variant, _channel: Variant) -> void: pass,
		"AddChatWindowMessages": func(id: Variant, group: Variant) -> void: _window_groups(id, group, true),
		"RemoveChatWindowMessages": func(id: Variant, group: Variant) -> void: _window_groups(id, group, false),
		"SetChatWindowName": func(id: Variant, value: Variant) -> void: _set_window(id, "name", str(value)),
		"SetChatWindowSize": func(id: Variant, value: Variant) -> void: _set_window(id, "size", value),
		"SetChatWindowAlpha": func(id: Variant, value: Variant) -> void: _set_window(id, "alpha", value),
		"SetChatWindowColor": func(id: Variant, r: Variant, g: Variant, b: Variant) -> void:
			_set_window(id, "color", Color(float(r), float(g), float(b))),
		"SetChatWindowShown": func(id: Variant, value: Variant) -> void:
			_set_window(id, "shown", value == 1 or value == true),
		"SetChatWindowLocked": func(id: Variant, value: Variant) -> void:
			_set_window(id, "locked", value == 1 or value == true),
		"SetChatWindowDocked": func(id: Variant, value: Variant) -> void:
			_set_window(id, "docked", int(value) if value != null else 0),
		"ChangeChatColor": func(type: Variant, r: Variant, g: Variant, b: Variant) -> void:
			fire_event("UPDATE_CHAT_COLOR", [type, r, g, b]),
		"LoggingChat": func(_on: Variant = null) -> bool: return false,
		"LoggingCombat": func(_on: Variant = null) -> bool: return false,
		"DoEmote": _emote,
		"RandomRoll": func(low: Variant, high: Variant) -> void:
			var payload: PackedByteArray = []
			payload.resize(8)
			payload.encode_u32(0, int(low) if low != null else 1)
			payload.encode_u32(4, int(high) if high != null else 100)
			_session.send_packet("MSG_RANDOM_ROLL", payload),
		"Logout": func() -> void: _session.logout(),
		# Places.
		"GetZoneText": func() -> String: return AreaInfo.area_name(AreaInfo.zone_of(_area)),
		"GetRealZoneText": func() -> String: return AreaInfo.area_name(AreaInfo.zone_of(_area)),
		"GetSubZoneText": func() -> String:
			return AreaInfo.area_name(_area) if AreaInfo.zone_of(_area) != _area else "",
		"GetMinimapZoneText": func() -> String: return AreaInfo.area_name(_area),
		"GetZonePVPInfo": func() -> Variant: return null,
		"GetGameTime": func() -> Array:
			var minute: int = int(WowClient.clock.minute())
			return [minute / 60, minute % 60],
		# Portraits.
		"SetPortraitTexture": _set_portrait,
		# The windows the HUD keeps still load and ask these once.
		"GetCursorMoney": func() -> int: return 0,
		"GetPlayerTradeMoney": func() -> int: return 0,
		"GetTargetTradeMoney": func() -> int: return 0,
		"GetInventoryItemCooldown": func(_unit: Variant, _slot: Variant) -> Array: return [0, 0, 0],
		"GetContainerItemCooldown": func(_bag: Variant, _slot: Variant) -> Array: return [0, 0, 0],
		"GetPetActionCooldown": func(_index: Variant) -> Array: return [0, 0, 0],
		"GetTabardCreationCost": func() -> int: return 0,
		"GetNextStableSlotCost": func() -> int: return 0,
		"GetBankSlotCost": func(_slots: Variant = null) -> int: return 0,
		"GetNumBankSlots": func() -> Array: return [0, false],
		"GuildControlGetNumRanks": func() -> int: return 0,
		"GetNumQuestLogEntries": func() -> Array: return [0, 0],
		"GetNumSpellTabs": func() -> int: return 0,
		"GetSpellTabInfo": func(_tab: Variant) -> Array: return ["", "", 0, 0],
		"GetNumSkillLines": func() -> int: return 0,
		"GetSkillLineInfo": func(_index: Variant) -> Array:
			return ["", false, false, 0, 0, 0, 0, false, 0, 0, 0, 0, ""],
		"GetSendMailPrice": func() -> int: return 0,
		"GetPetIcon": func() -> Variant: return null,
		"GetPVPRankInfo": func(_rank: Variant, _unit: Variant = null) -> Array: return ["", 0],
		"UnitPVPRank": func(_unit: Variant) -> int: return 0,
		"GetTrackingTexture": func() -> Variant: return null,
		"CreateWorldMapArrowFrame": _nothing,
		"PositionWorldMapArrowFrame": func(_a: Variant = null, _b: Variant = null) -> void: pass,
		"UpdateWorldMapArrowFrames": _nothing,
		"ShowWorldMapArrowFrame": func(_shown: Variant = null) -> void: pass,
	}
	for function_name: String in functions:
		register_function(function_name, functions[function_name])
