
extends Node

var health := 100.0
var damage_log := []
var spell_casts := {} # dictionary of cast_id -> {events: [], total: 0.0, meta: {}}
var _active_cast_id
signal dot_complete
var _dot_timer := Timer.new()
var dot_buffer := 0.06 # seconds to wait after last damage to consider DoT finished
var _event_counter := 0

func clear_damage_log():
	damage_log.clear()

func get_damage_log():
	return damage_log.duplicate()

func get_total_recorded_damage() -> float:
	var s := 0.0
	for d in damage_log:
		s += float(d.amount)
	return s

func _ready():
	# create a small one-shot timer that we restart on every damage event; when it times out
	# we emit `dot_complete` so tests can await the end of DoT activity.
	_dot_timer.one_shot = true
	_dot_timer.wait_time = dot_buffer
	add_child(_dot_timer)
	_dot_timer.connect("timeout", Callable(self, "_on_dot_complete"))

func _on_dot_complete():
	emit_signal("dot_complete")
	# Consider the active spell cast finished once DoT activity stabilizes
	_active_cast_id = null

func apply_damage(amount, aspect, metadata = null):
	health -= float(amount)
	print("[DemoTarget] %s took %s %s damage, health now %s" % [name, amount, aspect, health])

	# record the damage event with provenance metadata so tests can inspect source
	_event_counter += 1
	var ev = {"index": _event_counter, "amount": float(amount), "aspect": aspect, "meta": metadata}
	# append to linear log for backward compatibility
	damage_log.append(ev)

	# Determine which spell cast this event belongs to.
	var cast_id
	if metadata and typeof(metadata) == TYPE_DICTIONARY and metadata.has("cast_id"):
		cast_id = str(metadata.get("cast_id"))
		_active_cast_id = cast_id
	elif metadata and typeof(metadata) == TYPE_DICTIONARY and metadata.get("phase", "") == "instant":
		# treat an instant hit as the start of a new spell cast
		cast_id = "spell_cast_%d" % _event_counter
		_active_cast_id = cast_id
	elif _active_cast_id != null:
		cast_id = _active_cast_id
	else:
		# fallback: create a cast id per event (ungrouped)
		cast_id = "spell_cast_%d" % _event_counter

	# Ensure cast entry exists
	if not spell_casts.has(cast_id):
		spell_casts[cast_id] = {"events": [], "total": 0.0, "meta": {}}

	# store metadata & event
	spell_casts[cast_id]["events"].append(ev)
	spell_casts[cast_id]["total"] += float(amount)
	# merge/record any top-level metadata for the cast if provided
	if metadata and typeof(metadata) == TYPE_DICTIONARY:
		for k in metadata.keys():
			# keep first-seen values under cast meta
			if not spell_casts[cast_id]["meta"].has(k):
				spell_casts[cast_id]["meta"][k] = metadata[k]

	# restart the dot completion timer so tests can await stable end-of-DoT
	if _dot_timer:
		_dot_timer.stop()
		_dot_timer.start()

	# print compact summary and then a dedicated per-cast new-line entry for the appended event
	var src = ""
	if metadata and typeof(metadata) == TYPE_DICTIONARY:
		var eid = metadata.get("executor_id", "<unknown>")
		var phase = metadata.get("phase", "")
		src = " (src=" + str(eid) + ", phase=" + str(phase) + ")"
	if src != "":
		print("[DemoTarget]   metadata:" + str(metadata))
	# Print the new event as its own line prefixed by the spell cast id so logs are easier to scan
	# print("[DemoTarget] spell_cast=%s +event: %s" % [cast_id, str(ev)])

func apply_knockback(force, speed, area, caster):
	print("[DemoTarget] %s knocked back by force %s speed %s with area %s from %s" % [name, force, speed, area, caster.name if caster else "<none>"])

func apply_knockback_meta(force, speed, area, caster, metadata = null):
	# Record knockback as a cast-level event with metadata
	_event_counter += 1
	var caster_name = "<none>"
	if caster and typeof(caster) == TYPE_OBJECT and caster.has_method("get_name"):
		caster_name = caster.get_name()
	var ev = {"index": _event_counter, "type": "knockback", "params": {"force": float(force), "speed": float(speed), "area": float(area), "caster": caster_name}, "meta": metadata}

	# append to linear log for backward compatibility
	damage_log.append(ev)

	# Determine which spell cast this event belongs to.
	var cast_id
	if metadata and typeof(metadata) == TYPE_DICTIONARY and metadata.has("cast_id"):
		cast_id = str(metadata.get("cast_id"))
		_active_cast_id = cast_id
	elif metadata and typeof(metadata) == TYPE_DICTIONARY and metadata.get("phase", "") == "instant":
		# treat an instant-phase event as the start of a new spell cast
		cast_id = "spell_cast_%d" % _event_counter
		_active_cast_id = cast_id
	elif _active_cast_id != null:
		cast_id = _active_cast_id
	else:
		cast_id = "spell_cast_%d" % _event_counter

	if not spell_casts.has(cast_id):
		spell_casts[cast_id] = {"events": [], "total": 0.0, "meta": {}}

	spell_casts[cast_id]["events"].append(ev)

	# store top-level metadata (first-seen)
	if metadata and typeof(metadata) == TYPE_DICTIONARY:
		for k in metadata.keys():
			if not spell_casts[cast_id]["meta"].has(k):
				spell_casts[cast_id]["meta"][k] = metadata[k]

	# restart dot timer to keep cast active until stable
	if _dot_timer:
		_dot_timer.stop()
		_dot_timer.start()

	# print a compact debug line and the per-cast event line
	print("[DemoTarget] %s received knockback force=%s speed=%s area=%s from %s" % [name, force, speed, area, caster_name])
	if metadata:
		print("[DemoTarget]   metadata:" + str(metadata))
	print("[DemoTarget] spell_cast=%s +event: %s" % [cast_id, str(ev)])

func apply_heal(amount):
	health += float(amount)
	print("[DemoTarget] %s healed by %s, health now %s" % [name, amount, health])
