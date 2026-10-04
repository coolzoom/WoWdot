## GameTooltip's text for spells, items and units, kept as lines for the stock GameTooltip to show.
class_name TooltipLines
extends GameTooltip

# Each line as WowdotTooltip returns it: left text and colour, then right text and colour.
var lines: Array = []


func _init() -> void:
	shared = false
	var archive: WowArchive = WowAssets.archive
	_creature_types = WowDBC.open(archive, "CreatureType")
	_races = WowDBC.open(archive, "ChrRaces")
	_classes = WowDBC.open(archive, "ChrClasses")


func begin(
	tooltip_owner: Object, anchor: TooltipAnchor = TooltipAnchor.DEFAULT, anchor_to: Control = null,
) -> void:
	lines = []
	_owner = tooltip_owner
	_anchor = anchor
	_anchor_to = anchor_to


func add_double_line(
	left: String, right: String, left_color: Color = HIGHLIGHT, right_color: Color = HIGHLIGHT,
	_word_wrap: bool = false,
) -> void:
	lines.append([
		left, left_color.r, left_color.g, left_color.b,
		right, right_color.r, right_color.g, right_color.b,
	])


func present() -> void:
	pass


func hide_for(_tooltip_owner: Object) -> void:
	lines = []
