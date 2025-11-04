@tool
extends EditorPlugin

# preload centralized helpers
const SynergyHelpers = preload("res://addons/spellengine/tools/synergy_helpers.gd")
const PANEL_SCENE = preload("res://addons/spellengine/synergy_generator.tscn")

var panel
var aspect_list
var preview_text
var save_button
var suggest_button
var id_edit
var name_edit
var scalers_list
var add_scaler_button
var executors_list
var add_exec_button
var edit_exec_button
var remove_exec_button
var executor_dialog
var executor_editor

var executors_data = []
var editing_exec_index = -1
var scalers_data = []
var scalers_editing_index = -1
var scaler_dialog
var scaler_key_edit
var scaler_value_edit
var edit_scaler_button
var remove_scaler_button

func _enter_tree():
	# Instantiate the human-editable UI scene and wire the nodes
	panel = PANEL_SCENE.instantiate()
	# Ensure the dock shows a friendly name
	panel.name = "Synergy Generator"

	# Find nodes inside the scene
	aspect_list = panel.get_node("Main/AspectList")
	var btn_generate = panel.get_node("Buttons/GenerateButton")
	suggest_button = panel.get_node("Buttons/SuggestButton")
	save_button = panel.get_node("Buttons/SaveButton")
	preview_text = panel.get_node("Main/PreviewText")

	# Mid-area editor nodes
	id_edit = panel.get_node("Main/Mid/IDRow/IDEdit")
	name_edit = panel.get_node("Main/Mid/NameRow/NameEdit")
	scalers_list = panel.get_node("Main/Mid/Scalers/ScalersList")
	add_scaler_button = panel.get_node("Main/Mid/Scalers/ScalersControls/AddScalerButton")
	# create Edit/Remove scaler buttons dynamically for parity with executors
	var scalers_controls = panel.get_node("Main/Mid/Scalers/ScalersControls")
	edit_scaler_button = Button.new()
	edit_scaler_button.text = "Edit Scaler"
	scalers_controls.add_child(edit_scaler_button)
	remove_scaler_button = Button.new()
	remove_scaler_button.text = "Remove Scaler"
	scalers_controls.add_child(remove_scaler_button)

	# scaler dialog
	scaler_dialog = panel.get_node("ScalerDialog")
	scaler_key_edit = panel.get_node("ScalerDialog/ScalerEdit/ScalerKeyEdit")
	scaler_value_edit = panel.get_node("ScalerDialog/ScalerEdit/ScalerValueEdit")
	executors_list = panel.get_node("Main/Mid/Executors/ExecutorsList")
	add_exec_button = panel.get_node("Main/Mid/Executors/ExecButtons/AddExecButton")
	edit_exec_button = panel.get_node("Main/Mid/Executors/ExecButtons/EditExecButton")
	remove_exec_button = panel.get_node("Main/Mid/Executors/ExecButtons/RemoveExecButton")
	executor_dialog = panel.get_node("ExecutorDialog")
	executor_editor = panel.get_node("ExecutorDialog/ExecutorEditor")

	# Connect signals to plugin handlers
	btn_generate.connect("pressed", Callable(self, "_on_generate_proposal"))
	suggest_button.connect("pressed", Callable(self, "_on_suggest_defaults"))
	save_button.connect("pressed", Callable(self, "_on_save_synergy"))
	add_scaler_button.connect("pressed", Callable(self, "_on_add_scaler_pressed"))
	edit_scaler_button.connect("pressed", Callable(self, "_on_edit_scaler_pressed"))
	remove_scaler_button.connect("pressed", Callable(self, "_on_remove_scaler_pressed"))
	scaler_dialog.connect("confirmed", Callable(self, "_on_scaler_dialog_confirmed"))
	add_exec_button.connect("pressed", Callable(self, "_on_add_exec_pressed"))
	edit_exec_button.connect("pressed", Callable(self, "_on_edit_exec_pressed"))
	remove_exec_button.connect("pressed", Callable(self, "_on_remove_exec_pressed"))
	executor_dialog.connect("confirmed", Callable(self, "_on_executor_dialog_confirmed"))

	# Populate aspects
	_refresh_aspect_list()

	# Add to dock on the right
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, panel)

func _exit_tree():
	remove_control_from_docks(panel)
	if panel:
		panel.free()

func _refresh_aspect_list():
	aspect_list.clear()
	# scan res://aspects for .tres files
	var dir = DirAccess.open("res://aspects")
	if not dir:
		# nothing to show
		return
	dir.list_dir_begin()
	var fname = dir.get_next()
	while fname != "":
		if not dir.current_is_dir():
			if fname.ends_with(".tres") or fname.ends_with(".res"):
				var path = "res://aspects/" + fname
				var res = ResourceLoader.load(path)
				if res:
					var label = fname
					# try to show aspect name if available
					if res.has_method("get_name"):
						label = str(res.get_name()) + " (" + fname + ")"
					aspect_list.add_item(label)
					aspect_list.set_item_metadata(aspect_list.get_item_count()-1, path)
		fname = dir.get_next()
	dir.list_dir_end()

func _get_selected_aspect_paths():
	var out = []
	for i in range(aspect_list.get_item_count()):
		if aspect_list.is_selected(i):
			out.append(aspect_list.get_item_metadata(i))
	return out

	

func _on_generate_proposal():
	var paths = _get_selected_aspect_paths()
	if paths.size() == 0:
		SynergyHelpers.show_message("Please select one or more aspects to generate a proposal.")
		return
	# Build a basic proposal dict
	var comp_aspects = []
	for p in paths:
		var r = ResourceLoader.load(p)
		if r and r.has_method("get_name"):
			comp_aspects.append(r.get_name())
		else:
			# fallback to filename without extension
			var parts = p.get_file().split(".")
			comp_aspects.append(parts[0])

	var id = SynergyHelpers.join_array(comp_aspects, "_").to_lower().replace(" ", "_")
	var name_parts = []
	for s in comp_aspects:
		name_parts.append(str(s).capitalize())
	var name = SynergyHelpers.join_array(name_parts, " ")

	var spec = {
		"id": id,
		"name": name,
		"component_aspects": comp_aspects,
		"default_scalers": {},
		"extra_executors": []
	}


	preview_text.text = SynergyHelpers.json_stringify(spec)
	_populate_mid_controls(spec)

func _on_suggest_defaults():
	# Simple heuristic: set amount and mana_cost multipliers based on number of aspects
	var txt = preview_text.text.strip_edges()
	if txt == "":
		SynergyHelpers.show_message("Generate a proposal first.")
		return
	var j = JSON.new()
	var parse_res = SynergyHelpers.parse_json_text(txt)
	if parse_res.has("error") and parse_res["error"] != OK:
		SynergyHelpers.show_message("Proposal is not valid JSON: " + str(parse_res["error"]))
		return
	var proposal = parse_res.get("result", null)
	if proposal == null or typeof(proposal) != TYPE_DICTIONARY:
		SynergyHelpers.show_message("Proposal parse returned no result or invalid type.")
		return
	var n = 1
	if proposal.has("component_aspects"):
		n = proposal["component_aspects"].size()
	# heuristic: single aspect -> 1.3, two aspects -> 1.15 average, more aspects -> 1.1
	var base = 1.0
	if n == 1: base = 1.3
	elif n == 2: base = 1.15
	else: base = 1.1
	proposal["default_scalers"] = {"amount": base, "mana_cost": base}
	preview_text.text = SynergyHelpers.json_stringify(proposal)
	_populate_mid_controls(proposal)

func _on_save_synergy():
	var txt = preview_text.text.strip_edges()
	if txt == "":
		SynergyHelpers.show_message("Nothing to save. Generate or paste a proposal first.")
		return
	var parse_res = SynergyHelpers.parse_json_text(txt)
	if parse_res.has("error") and parse_res["error"] != OK:
		SynergyHelpers.show_message("Proposal is not valid JSON: " + str(parse_res["error"]))
		return
	var spec = parse_res.get("result", null)

	# Merge in any edits from the Mid UI (id/name/scalers/executors)
	_merge_mid_controls_into_spec(spec)
	# validate minimal fields
	if not spec.has("id") or not spec.has("component_aspects"):
		SynergyHelpers.show_message("Proposal missing required keys 'id' or 'component_aspects'")
		return
	var id = spec["id"].to_lower()
	var path = "res://synergies/" + id + ".tres"
	# create Synergy resource
	var s = Synergy.new()
	s.set_spec(spec)
	var err = ResourceSaver.save(s, path)
	if err == OK:
		SynergyHelpers.show_message("Saved synergy: " + path)
		_refresh_aspect_list()
	else:
		# Fallback: write JSON spec if typed Synergy resource isn't available in the editor
		SynergyHelpers.show_message("Failed to save typed Synergy resource (ResourceSaver returned: " + str(err) + "). Attempting JSON fallback...")
		var json_path = "res://synergies/" + id + ".json"
		var file = FileAccess.open(json_path, FileAccess.ModeFlags.WRITE)
		if file:
			file.store_string(SynergyHelpers.json_stringify(spec))
			file.close()
			SynergyHelpers.show_message("Saved JSON fallback: " + json_path)
		else:
			SynergyHelpers.show_message("JSON fallback failed: could not open file " + json_path)


func _populate_mid_controls(spec:Dictionary) -> void:
	if spec == null:
		return
	# id / name
	id_edit.text = str(spec.get("id", ""))
	name_edit.text = str(spec.get("name", ""))

	# scalers: populate scalers_data and refresh list
	scalers_data.clear()
	var scalers = spec.get("default_scalers", {})
	if typeof(scalers) == TYPE_DICTIONARY:
		for k in scalers.keys():
			scalers_data.append({"key": k, "value": scalers[k]})
	_refresh_scalers_list()

	# executors
	executors_data = []
	var exs = spec.get("extra_executors", [])
	if typeof(exs) == TYPE_ARRAY:
		for e in exs:
			executors_data.append(e)
	_refresh_executors_list()


func _on_add_scaler_pressed() -> void:
	scalers_editing_index = -1
	scaler_key_edit.text = ""
	scaler_value_edit.text = ""
	scaler_dialog.popup_centered()


func _on_edit_scaler_pressed() -> void:
	var sel = scalers_list.get_selected_items()
	if sel.size() == 0:
		SynergyHelpers.show_message("Select a scaler to edit.")
		return
	var idx = sel[0]
	scalers_editing_index = idx
	var entry = scalers_data[idx]
	scaler_key_edit.text = str(entry.get("key", ""))
	scaler_value_edit.text = str(entry.get("value", ""))
	scaler_dialog.popup_centered()


func _on_remove_scaler_pressed() -> void:
	var sel = scalers_list.get_selected_items()
	if sel.size() == 0:
		SynergyHelpers.show_message("Select a scaler to remove.")
		return
	var idx = sel[0]
	scalers_data.remove_at(idx)
	_refresh_scalers_list()


func _on_scaler_dialog_confirmed() -> void:
	var key = scaler_key_edit.text.strip_edges()
	var val_s = scaler_value_edit.text.strip_edges()
	if key == "":
		SynergyHelpers.show_message("Scaler key cannot be empty.")
		return
	var val
	if val_s.is_valid_float():
		val = float(val_s)
	else:
		val = val_s
	if scalers_editing_index >= 0 and scalers_editing_index < scalers_data.size():
		scalers_data[scalers_editing_index] = {"key": key, "value": val}
	else:
		scalers_data.append({"key": key, "value": val})
	scalers_editing_index = -1
	_refresh_scalers_list()
	# update preview
	var parse_res = SynergyHelpers.parse_json_text(preview_text.text)
	var spec = parse_res.get("result", {})
	_merge_mid_controls_into_spec(spec)


func _collect_scalers_from_ui() -> Dictionary:
	var out = {}
	for entry in scalers_data:
		if typeof(entry) == TYPE_DICTIONARY and entry.has("key"):
			out[str(entry["key"]).strip_edges()] = entry.get("value")
	return out


func _refresh_scalers_list() -> void:
	scalers_list.clear()
	for entry in scalers_data:
		var k = str(entry.get("key", ""))
		var v = entry.get("value")
		var vs = ""
		if typeof(v) == TYPE_FLOAT or typeof(v) == TYPE_INT:
			vs = str(v)
		else:
			vs = str(v)
		scalers_list.add_item(k + ": " + vs)


func _refresh_executors_list() -> void:
	executors_list.clear()
	for e in executors_data:
		executors_list.add_item(SynergyHelpers.json_stringify(e))


func _on_add_exec_pressed() -> void:
	editing_exec_index = -1
	executor_editor.text = ""
	executor_dialog.popup_centered()


func _on_edit_exec_pressed() -> void:
	var sel = executors_list.get_selected_items()
	if sel.size() == 0:
		SynergyHelpers.show_message("Select an executor to edit.")
		return
	var idx = sel[0]
	editing_exec_index = idx
	executor_editor.text = SynergyHelpers.json_stringify(executors_data[idx])
	executor_dialog.popup_centered()


func _on_remove_exec_pressed() -> void:
	var sel = executors_list.get_selected_items()
	if sel.size() == 0:
		SynergyHelpers.show_message("Select an executor to remove.")
		return
	var idx = sel[0]
	executors_data.remove_at(idx)
	_refresh_executors_list()


func _on_executor_dialog_confirmed() -> void:
	var txt = executor_editor.text.strip_edges()
	if txt == "":
		SynergyHelpers.show_message("Executor JSON is empty.")
		return
	var parse_res = SynergyHelpers.parse_json_text(txt)
	if parse_res.has("error") and parse_res["error"] != OK:
		SynergyHelpers.show_message("Executor JSON invalid: " + str(parse_res["error"]))
		return
	var obj = parse_res.get("result", null)
	if obj == null:
		SynergyHelpers.show_message("Executor parse returned no result.")
		return
	if editing_exec_index >= 0 and editing_exec_index < executors_data.size():
		executors_data[editing_exec_index] = obj
	else:
		executors_data.append(obj)
	editing_exec_index = -1
	_refresh_executors_list()


func _merge_mid_controls_into_spec(spec:Dictionary) -> void:
	if spec == null:
		return
	# id/name
	var sid = str(id_edit.text).strip_edges()
	var sname = str(name_edit.text).strip_edges()
	if sid != "":
		spec["id"] = sid
	if sname != "":
		spec["name"] = sname
	# scalers
	spec["default_scalers"] = _collect_scalers_from_ui()
	# executors
	spec["extra_executors"] = executors_data
	# update preview
	preview_text.text = SynergyHelpers.json_stringify(spec)
