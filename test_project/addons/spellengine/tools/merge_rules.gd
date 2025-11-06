@tool
class_name MergeRules

# Simple merge rules loader and applier for the Synergy editor.
# Rules are stored in res://addons/spellengine/merge_rules.json and describe how to merge
# scaler keys across multiple aspects. Supported modes: "average", "sum", "max", "min", "mana_penalty".

static var rules := {}

static func load_rules() -> void:
	var path = "res://addons/spellengine/merge_rules.json"
	print("[MergeRules] load_rules() path=", path)
	var file = FileAccess.open(path, FileAccess.ModeFlags.READ)
	if not file:
		print("[MergeRules] could not open file: ", path)
		return
	var txt = file.get_as_text()
	file.close()
	var j = JSON.new()
	var res = j.parse_string(txt)
	# JSON.parse_string historically returned a wrapper { error, result } but
	# newer Godot versions may return the parsed object directly. Handle both.
	print("[MergeRules] parse result:", res)
	if typeof(res) == TYPE_DICTIONARY:
		# Case A: parse returned { error, result }
		if res.has("error"):
			if res["error"] != OK:
				print("[MergeRules] JSON parse error:", res)
				return
			var obj = res.get("result", null)
			if typeof(obj) == TYPE_DICTIONARY:
				rules = obj
				print("[MergeRules] loaded rules keys:", rules.keys())
				return
			# fall through to try treating res as the rules dict
		# Case B: parse returned the rules dictionary directly
		if res.keys().size() > 0:
			# verify it's a map of dictionaries
			var ok = true
			for k in res.keys():
				if typeof(res[k]) != TYPE_DICTIONARY:
					ok = false
					break
			if ok:
				rules = res
				print("[MergeRules] loaded rules keys:", rules.keys())
				return
		# nothing usable found
		print("[MergeRules] parse returned no dictionary result: ", res)
	else:
		# parse returned something unexpected (error code/int/string), try JSON.parse again using the quick helper
		print("[MergeRules] parse returned unexpected type:", typeof(res), res)

static func get_rule(key:String) -> Dictionary:
	if rules.has(key):
		return rules[key]
	return {}

static func apply_rule(key:String, values:Array, aspect_count:int) -> Variant:
	# values: array of values for this key (from aspects)
	var rule = get_rule(key)
	var mode = rule.get("mode", "average")
	if mode == "average":
		# Treat unset (null) scaler values as 1 for merging purposes
		var sum = 0.0
		var cnt = 0
		for v in values:
			if typeof(v) == TYPE_INT or typeof(v) == TYPE_FLOAT:
				sum += float(v)
				cnt += 1
			elif v == null:
				# unset scaler: treat as 1
				sum += 1.0
				cnt += 1
		if cnt > 0:
			return sum / float(cnt)
		# no numeric values -> return last string
		if values.size() > 0:
			return str(values[values.size()-1])
		return null
	elif mode == "sum":
		var sum2 = 0.0
		for v in values:
			if typeof(v) == TYPE_INT or typeof(v) == TYPE_FLOAT:
				sum2 += float(v)
			elif v == null:
				# unset scaler contributes 1 to sums
				sum2 += 1.0
		return sum2
	elif mode == "max":
		var best = null
		for v in values:
			if typeof(v) == TYPE_INT or typeof(v) == TYPE_FLOAT:
				if best == null or float(v) > float(best):
					best = v
			elif v == null:
				# unset scaler considered as 1
				if best == null or 1.0 > float(best):
					best = 1.0
		if best != null:
			return best
		if values.size() > 0:
			return str(values[values.size()-1])
		return null
	elif mode == "min":
		var low = null
		for v in values:
			if typeof(v) == TYPE_INT or typeof(v) == TYPE_FLOAT:
				if low == null or float(v) < float(low):
					low = v
			elif v == null:
				# unset scaler treated as 1
				if low == null or 1.0 < float(low):
					low = 1.0
		if low != null:
			return low
		if values.size() > 0:
			return str(values[values.size()-1])
		return null
	elif mode == "mana_penalty":
		# debug: log when mana_penalty is used to help troubleshooting
		# print("[MergeRules] mana_penalty for key=", key, " values=", values, " aspect_count=", aspect_count, " rule=", rule)
		# base may be 'average' or 'sum' etc. fallback to average
		var base_mode = rule.get("base", "average")
		var base_val = apply_rule_generic(base_mode, values)
		if typeof(base_val) == TYPE_INT or typeof(base_val) == TYPE_FLOAT:
			# support new 'penalty_per_aspect' key, fall back to legacy 'penalty_per_extra_aspect'
			var penalty = 0.1
			if rule.has("penalty_per_aspect"):
				penalty = float(rule.get("penalty_per_aspect"))
			elif rule.has("penalty_per_extra_aspect"):
				# legacy behaviour used 'per extra aspect' — convert to per-aspect for backwards compat
				penalty = float(rule.get("penalty_per_extra_aspect"))
			# Use the total number of aspects in the multiplier (user-desired formula)
			return float(base_val) * (1.0 + penalty * max(0,float(aspect_count) - 1))
		return base_val
	else:
		# unknown mode: fallback to average
		return apply_rule("", values, aspect_count)


static func apply_rule_generic(mode:String, values:Array) -> Variant:
	if mode == "average":
		# Treat unset (null) scaler values as 1 for merging purposes
		var sum = 0.0
		var cnt = 0
		for v in values:
			if typeof(v) == TYPE_INT or typeof(v) == TYPE_FLOAT:
				sum += float(v)
				cnt += 1
			elif v == null:
				# unset scaler: treat as 1
				sum += 1.0
				cnt += 1
		if cnt > 0:
			return sum / float(cnt)
		if values.size() > 0:
			return str(values[values.size()-1])
		return null
	elif mode == "sum":
		var s = 0.0
		for v in values:
			if typeof(v) == TYPE_INT or typeof(v) == TYPE_FLOAT:
				s += float(v)
			elif v == null:
				# unset scaler contributes 1 to sums
				s += 1.0
		return s
	elif mode == "max":
		var b = null
		for v in values:
			if typeof(v) == TYPE_INT or typeof(v) == TYPE_FLOAT:
				if b == null or float(v) > float(b):
					b = v
			elif v == null:
				# unset scaler considered as 1
				if b == null or 1.0 > float(b):
					b = 1.0
		if b != null:
			return b
		if values.size() > 0:
			return str(values[values.size()-1])
		return null
	elif mode == "min":
		var l = null
		for v in values:
			if typeof(v) == TYPE_INT or typeof(v) == TYPE_FLOAT:
				if l == null or float(v) < float(l):
					l = v
			elif v == null:
				# unset scaler treated as 1
				if l == null or 1.0 < float(l):
					l = 1.0
		if l != null:
			return l
		if values.size() > 0:
			return str(values[values.size()-1])
		return null
	# default
	return null
