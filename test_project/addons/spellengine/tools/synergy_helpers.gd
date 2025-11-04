@tool
class_name SynergyEditorHelpers

# Centralized helpers for the Synergy editor plugin.
# Provides JSON parsing/stringifying, array joining, and editor messages.

static func json_stringify(value) -> String:
	# Prefer instance JSON.stringify when available, otherwise fall back to a plain string repr.
	var j = JSON.new()
	if j.has_method("stringify"):
		return j.stringify(value)
	return str(value)

static func parse_json_text(txt:String) -> Dictionary:
	# Use an instance of JSON to parse text and normalize return shapes across Godot builds.
	var j = JSON.new()
	var res = null
	if j.has_method("parse_string"):
		res = j.parse_string(txt)
	elif j.has_method("parse"):
		res = j.parse(txt)
	else:
		return {"error": ERR_PARSE_ERROR, "result": null}

	# Normalize result: some variants return a Dictionary {error, result}, some an int, some the parsed dict directly.
	if typeof(res) == TYPE_DICTIONARY:
		if res.has("error") or res.has("result"):
			if not res.has("error"):
				res["error"] = OK
			if not res.has("result"):
				res["result"] = null
			return res
		return {"error": OK, "result": res}
	elif typeof(res) == TYPE_INT:
		if res != OK:
			return {"error": res, "result": null}
		# If OK but no result, attempt to get result from the JSON instance if available
		if j.has_method("get_result"):
			var parsed = j.get_result()
			return {"error": OK, "result": parsed}
		return {"error": OK, "result": null}
	else:
		return {"error": ERR_PARSE_ERROR, "result": null}

static func join_array(arr:Array, sep:String) -> String:
	var out = ""
	for i in range(arr.size()):
		if i > 0:
			out += sep
		out += str(arr[i])
	return out

static func show_message(msg:String) -> void:
	# Simple message sink for the plugin - prints and pushes an editor error so it appears in Output/Debugger.
	print("[Synergy Plugin] " + msg)
	push_error("[Synergy Plugin] " + msg)
