@tool
extends EditorPlugin

# preload centralized helpers
const SynergyHelpers = preload("res://addons/spellengine/tools/synergy_helpers.gd")
const MergeRules = preload("res://addons/spellengine/tools/merge_rules.gd")
const PANEL_SCENE = preload("res://addons/spellengine/synergy_generator.tscn")

var panel
var aspect_list
var current_spec := {}
var copy_json_button
var synergy_list
var save_button
var loaded_synergy_path := ""
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
var exec_id_option
var exec_trigger_edit
var exec_trigger_option
var params_container
var add_param_button
var exec_schemas = {}
var execs_populated := false
var delete_button
var confirm_dialog
var synergy_json_edit
var pending_delete_path := ""
var aspect_dialog
var aspect_dialog_id_edit
var aspect_dialog_name_edit
var editing_aspect_path := ""
var aspect_scalers_data = []
var aspect_scalers_list
var aspect_defaults_list
var _aspect_scaler_mode := false
var aspect_scalers_editing_index = -1
var aspect_scaler_catalog = {}
var aspect_defaults_panel

var executors_data = []
var editing_exec_index = -1
var scalers_data = []
var scalers_editing_index = -1
var scaler_dialog
var scaler_key_edit
var scaler_value_edit
var edit_scaler_button
var remove_scaler_button
var _prev_aspect_selection := []
var selection_timer: Timer
var selection_debounce_time := 0.25
var _suppress_selection_callbacks := false
var _last_matched_indices := []
var _suppress_exec_id_callbacks := false

func _enter_tree():
	# Instantiate the human-editable UI scene and wire the nodes
	panel = PANEL_SCENE.instantiate()
	# Ensure the dock shows a friendly name
	panel.name = "Synergy Generator"

	# Find nodes inside the scene
	aspect_list = panel.get_node("Main/Aspects/AspectList")
	# Aspect create/edit/delete buttons
	var create_aspect_btn = panel.get_node_or_null("Main/Aspects/AspectButtons/CreateAspect")
	var edit_aspect_btn = panel.get_node_or_null("Main/Aspects/AspectButtons/EditAspect")
	var delete_aspect_btn = panel.get_node_or_null("Main/Aspects/AspectButtons/DeleteAspect")
	synergy_list = panel.get_node("Main/Synergies/SynergyList")
	save_button = panel.get_node("Controls/SaveButton")

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
	# Aspect defaults informative panel (created dynamically)
	aspect_defaults_panel = panel.get_node_or_null("Main/Aspects/AspectDefaults")
	executors_list = panel.get_node("Main/Mid/Executors/ExecutorsList")
	add_exec_button = panel.get_node("Main/Mid/Executors/ExecButtons/AddExecButton")
	edit_exec_button = panel.get_node("Main/Mid/Executors/ExecButtons/EditExecButton")
	remove_exec_button = panel.get_node("Main/Mid/Executors/ExecButtons/RemoveExecButton")
	# double-clicking an executor in the list should open the editor for that executor
	if executors_list:
		executors_list.connect("item_activated", Callable(self, "_on_executor_activated"))
	# Debugging button to inspect executor registry / ProjectSettings at runtime
	var exec_buttons = panel.get_node("Main/Mid/Executors/ExecButtons")
	var debug_exec_button = Button.new()
	debug_exec_button.text = "Debug Executors"
	exec_buttons.add_child(debug_exec_button)
	debug_exec_button.connect("pressed", Callable(self, "_on_debug_execs_pressed"))
	executor_dialog = panel.get_node("ExecutorDialog")
	executor_editor = panel.get_node("ExecutorDialog/ExecutorEditor")

	# Connect signals to plugin handlers
	save_button.connect("pressed", Callable(self, "_on_save_synergy"))
	# Load synergy button (explicit load from list)
	var load_synergy_btn = panel.get_node("Main/Synergies/SynergyButtons/LoadSynergy")
	if load_synergy_btn:
		load_synergy_btn.connect("pressed", Callable(self, "_on_load_synergy_pressed"))
	# Delete synergy button + confirm dialog
	delete_button = panel.get_node("Main/Synergies/SynergyButtons/DeleteSynergy")
	if delete_button:
		delete_button.connect("pressed", Callable(self, "_on_delete_synergy_pressed"))
	confirm_dialog = panel.get_node("ConfirmDialog")
	if confirm_dialog:
		confirm_dialog.connect("confirmed", Callable(self, "_on_confirm_delete_synergy_confirmed"))
		synergy_json_edit = panel.get_node("ConfirmDialog/BoxContainer/SynergyJSON")
		# connect explicit cancel button if present
		var cancel_btn = panel.get_node_or_null("ConfirmDialog/BoxContainer/CancelButton")
		if cancel_btn:
			cancel_btn.connect("pressed", Callable(self, "_on_confirm_delete_cancel_pressed"))
	# Do NOT auto-load synergies on double-click for now; only load via the explicit Load button
	# If you want to enable double-click loading later, reconnect the signal below:
	if synergy_list:
		synergy_list.connect("item_activated", Callable(self, "_on_synergy_activated"))
	# Double-clicking an aspect should start its edit flow
	if aspect_list:
		aspect_list.connect("item_activated", Callable(self, "_on_aspect_activated"))
	# Connect aspect buttons if present
	if create_aspect_btn:
		create_aspect_btn.connect("pressed", Callable(self, "_on_create_aspect_pressed"))
	if edit_aspect_btn:
		edit_aspect_btn.connect("pressed", Callable(self, "_on_edit_aspect_pressed"))
	if delete_aspect_btn:
		delete_aspect_btn.connect("pressed", Callable(self, "_on_delete_aspect_pressed"))
	# update proposal automatically when aspects are selected (debounced)
	aspect_list.connect("item_selected", Callable(self, "_on_aspect_selected"))
	add_scaler_button.connect("pressed", Callable(self, "_on_add_scaler_pressed"))
	# hook aspect scaler add/edit/remove (we'll create handlers below)
	# create aspect defaults panel if not present
	if not aspect_defaults_panel:
		# create a small VBox under Main/Aspects to show unique default_scalers across all aspects
		aspect_defaults_panel = VBoxContainer.new()
		aspect_defaults_panel.name = "AspectDefaults"
		var lbl = Label.new()
		lbl.text = "Aspect Defaults"
		aspect_defaults_panel.add_child(lbl)
		aspect_defaults_list = ItemList.new()
		aspect_defaults_list.name = "AspectDefaultsList"
		aspect_defaults_list.custom_minimum_size = Vector2(0, 120)
		aspect_defaults_panel.add_child(aspect_defaults_list)
		# add a refresh button
		var refresh_btn = Button.new()
		refresh_btn.text = "Refresh Defaults"
		refresh_btn.connect("pressed", Callable(self, "_refresh_aspect_defaults_panel"))
		aspect_defaults_panel.add_child(refresh_btn)
		panel.get_node("Main/Aspects").add_child(aspect_defaults_panel)
	# connect scaler dialog confirm - already connected elsewhere; reuse same dialog for aspect scalers by toggling a flag when opening
	edit_scaler_button.connect("pressed", Callable(self, "_on_edit_scaler_pressed"))
	remove_scaler_button.connect("pressed", Callable(self, "_on_remove_scaler_pressed"))
	scaler_dialog.connect("confirmed", Callable(self, "_on_scaler_dialog_confirmed"))
	add_exec_button.connect("pressed", Callable(self, "_on_add_exec_pressed"))
	edit_exec_button.connect("pressed", Callable(self, "_on_edit_exec_pressed"))
	remove_exec_button.connect("pressed", Callable(self, "_on_remove_exec_pressed"))
	executor_dialog.connect("confirmed", Callable(self, "_on_executor_dialog_confirmed"))
	# new structured executor dialog controls
	exec_id_option = panel.get_node_or_null("ExecutorDialog/ExecutorEditor/ExecIDRow/ExecIDOption")
	exec_trigger_edit = panel.get_node_or_null("ExecutorDialog/ExecutorEditor/TriggerRow/TriggerEdit")
	# If the Trigger control in the scene is a plain LineEdit, replace it at runtime
	# with an OptionButton so users can pick an executor id to trigger on.
	if exec_trigger_edit:
		# detect LineEdit by method presence
		if exec_trigger_edit is LineEdit:
			var parent = exec_trigger_edit.get_parent()
			var idx = parent.get_child_index(exec_trigger_edit)
			parent.remove_child(exec_trigger_edit)
			exec_trigger_edit.queue_free()
			# create an OptionButton in its place
			exec_trigger_option = OptionButton.new()
			parent.add_child(exec_trigger_option)
			parent.move_child(exec_trigger_option, idx)
		else:
			# control is not a LineEdit; if it's already an OptionButton, use it
			if exec_trigger_edit is OptionButton:
				exec_trigger_option = exec_trigger_edit
				exec_trigger_edit = null
	# If scene did not include the trigger control, create one under the TriggerRow if present
	if not exec_trigger_option:
		var trigger_row = panel.get_node_or_null("ExecutorDialog/ExecutorEditor/TriggerRow")
		if trigger_row:
			exec_trigger_option = OptionButton.new()
			trigger_row.add_child(exec_trigger_option)
	params_container = panel.get_node_or_null("ExecutorDialog/ExecutorEditor/ParamsContainer")
	add_param_button = panel.get_node_or_null("ExecutorDialog/ExecutorEditor/ParamsButtons/AddParamButton")
	if add_param_button:
		add_param_button.connect("pressed", Callable(self, "_on_add_param_pressed"))
	# connect selection change on executor dropdown to refresh params
	if exec_id_option:
		exec_id_option.connect("item_selected", Callable(self, "_on_exec_id_selected"))
	# populate executor id list from ProjectSettings (written by the native module) or runtime registry
	if exec_id_option:
		exec_id_option.clear()
		# Try ProjectSettings first (set by the module during initialization)
		if ProjectSettings.has_setting("spellengine/executor_ids"):
			var ids = ProjectSettings.get_setting("spellengine/executor_ids")
			if typeof(ids) == TYPE_ARRAY:
				for i in ids:
					exec_id_option.add_item(str(i))
		# load schemas if provided
		if ProjectSettings.has_setting("spellengine/executor_schemas"):
			exec_schemas = ProjectSettings.get_setting("spellengine/executor_schemas")
		# If ProjectSettings didn't provide executor ids, leave the dropdown empty.
		# (Fallback to runtime registry was removed because some editor builds
		# don't expose native singletons to GDScript reliably.)
		# Mark whether we've populated executors so the _process loop can retry later
		execs_populated = exec_id_option.get_item_count() > 0
		# populate trigger dropdown as well
		_populate_trigger_dropdown()

	# Populate aspects
	_refresh_aspect_list()
	# Populate existing synergies list
	_refresh_synergy_list()

	# Debounce timer for aspect selection (so user-driven changes don't immediately overwrite loaded synergies)
	selection_timer = Timer.new()
	selection_timer.one_shot = true
	selection_timer.wait_time = selection_debounce_time
	panel.add_child(selection_timer)
	selection_timer.connect("timeout", Callable(self, "_on_debounced_aspect_change"))

	# load merge rules
	MergeRules.load_rules()
	# SynergyHelpers.show_message("Loading Merge rules.")

	# start processing to detect selection changes (robust across editor versions)
	set_process(true)

	# Add to dock on the right
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, panel)


func _ensure_execs_loaded() -> void:
	# Populate the executor dropdown from ProjectSettings if available. This helps
	# when the native module writes ProjectSettings after this plugin has already
	# entered the tree (module init ordering may vary).
	if execs_populated:
		return
	if not exec_id_option:
		return
	# If ProjectSettings has the executor ids/schemas, populate now
	if ProjectSettings.has_setting("spellengine/executor_ids"):
		exec_id_option.clear()
		var ids = ProjectSettings.get_setting("spellengine/executor_ids")
		if typeof(ids) == TYPE_ARRAY:
			for i in ids:
				exec_id_option.add_item(str(i))
		# load schemas if provided
		if ProjectSettings.has_setting("spellengine/executor_schemas"):
			exec_schemas = ProjectSettings.get_setting("spellengine/executor_schemas")
		else:
			exec_schemas = {}
		execs_populated = exec_id_option.get_item_count() > 0
		print("[SynergyPlugin] loaded executor ids from ProjectSettings: ", exec_id_option.get_item_count())
		_populate_trigger_dropdown()
		return
	else:
		# ProjectSettings didn't have executor ids; try a bundled JSON fallback in the addon
		var fb_path = "res://addons/spellengine/executor_schemas.json"
		var f = FileAccess.open(fb_path, FileAccess.ModeFlags.READ)
		if f:
			var txt = f.get_as_text()
			f.close()
			var pr = SynergyHelpers.parse_json_text(txt)
			if pr.has("error") and pr["error"] == OK:
				var data = pr.get("result", null)
				if typeof(data) == TYPE_DICTIONARY:
					if data.has("executor_ids") and typeof(data["executor_ids"]) == TYPE_ARRAY:
						exec_id_option.clear()
						for i in data["executor_ids"]:
							exec_id_option.add_item(str(i))
						if data.has("executor_schemas") and typeof(data["executor_schemas"]) == TYPE_DICTIONARY:
							exec_schemas = data["executor_schemas"]
							execs_populated = exec_id_option.get_item_count() > 0
							print("[SynergyPlugin] loaded executor ids from bundled fallback JSON: ", exec_id_option.get_item_count())
							_populate_trigger_dropdown()
						return
		# fallback failed; we'll wait and retry later
		return

func _exit_tree():
	remove_control_from_docks(panel)
	if panel:
		panel.free()

	# stop processing when plugin unloaded
	set_process(false)

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
	# rebuild aspect default scalers catalog and update informative panel
	_build_aspect_scaler_catalog()
	_refresh_aspect_defaults_panel()


func _refresh_synergy_list():
	synergy_list.clear()
	var dir = DirAccess.open("res://synergies")
	if not dir:
		# no synergies folder yet
		return
	# Collect entries grouped by component count -> list of {label, path, sort_key}
	var groups = {}
	dir.list_dir_begin()
	var fname = dir.get_next()
	while fname != "":
		if not dir.current_is_dir():
			if fname.ends_with('.tres') or fname.ends_with('.res') or fname.ends_with('.json'):
				var path = 'res://synergies/' + fname
				var spec = _load_spec_from_path(path)
				# display friendly label
				var label = fname
				if typeof(spec) == TYPE_DICTIONARY and spec.has('name'):
					label = str(spec.get('name')) + ' (' + fname + ')'
				# determine component_aspects and canonical sort key
				var aspects = []
				if typeof(spec) == TYPE_DICTIONARY and spec.has('component_aspects'):
					var cav = spec.get('component_aspects')
					if typeof(cav) == TYPE_ARRAY:
						for a in cav:
							if typeof(a) == TYPE_STRING:
								aspects.append(str(a).strip_edges().to_lower())
				# sort aspects for canonicalization
				aspects.sort()
				var sort_key = ""
				for i in range(aspects.size()):
					if i: sort_key += "+"
					sort_key += aspects[i]
				var count = aspects.size()
				if not groups.has(count):
					groups[count] = []
				groups[count].append({"label": label, "path": path, "sort_key": sort_key})
		fname = dir.get_next()
	dir.list_dir_end()
	# Now iterate groups in ascending component count order and add a disabled header per group,
	# then add items sorted by the canonical sort_key
	var counts = groups.keys()
	counts.sort()
	print("[SynergyPlugin] _refresh_synergy_list: found groups count=", counts.size())
	for gc in counts:
		print("[SynergyPlugin] group ", gc, " entries=", groups[gc].size())
	for ci in range(counts.size()):
		var c = counts[ci]
		var header = str(c) + " aspect synergies"
		# add header as disabled item
		var hidx = synergy_list.get_item_count()
		synergy_list.add_item(header)
		synergy_list.set_item_metadata(hidx, null)
		synergy_list.set_item_disabled(hidx, true)
		# sort group's entries (Godot 4 expects a single Callable for sort_custom)
		var entries = groups[c]
		entries.sort_custom(Callable(self, "_sort_group_entries_by_key"))
		for e in entries:
			var idx = synergy_list.get_item_count()
			var lab = ""
			var pth = null
			if typeof(e) == TYPE_DICTIONARY:
				lab = str(e.get("label", ""))
				pth = e.get("path", null)
			else:
				lab = str(e)
				pth = null
			synergy_list.add_item(lab)
			synergy_list.set_item_metadata(idx, pth)


func _on_synergy_activated(idx:int) -> void:
	var path = synergy_list.get_item_metadata(idx)
	if not path:
		return
	var spec = _load_spec_from_path(path)
	if typeof(spec) == TYPE_DICTIONARY:
		current_spec = spec
		loaded_synergy_path = path
		# Deselect any visible aspect selections so the UI doesn't imply selection from the loaded spec
		if aspect_list:
			if aspect_list.has_method('deselect_all'):
				aspect_list.deselect_all()
			else:
				for i in range(aspect_list.get_item_count()):
					if aspect_list.is_selected(i):
						if aspect_list.has_method('deselect'):
							aspect_list.deselect(i)
						elif aspect_list.has_method('set_item_selected'):
							aspect_list.set_item_selected(i, false)
		# clear selection snapshots to avoid stale debounce triggers
		_prev_aspect_selection = []
		_last_matched_indices = []
		_populate_mid_controls(current_spec)
		SynergyHelpers.show_message('Loaded synergy: ' + path)
	else:
		SynergyHelpers.show_message('Failed to load synergy: ' + path)


func _sort_group_entries_by_key(a, b) -> int:
	# both a and b are dictionaries with a 'sort_key' string
	var ak = ""
	var bk = ""
	if typeof(a) == TYPE_DICTIONARY and a.has("sort_key"):
		ak = str(a["sort_key"])
	if typeof(b) == TYPE_DICTIONARY and b.has("sort_key"):
		bk = str(b["sort_key"])
	if ak < bk:
		return -1
	elif ak > bk:
		return 1
	return 0


func _on_load_synergy_pressed() -> void:
	# Load the currently-selected synergy in the SynergyList (button-activated)
	var sel = synergy_list.get_selected_items()
	if sel.size() == 0:
		SynergyHelpers.show_message('Select a synergy in the list first.')
		return
	var idx = sel[0]
	var path = synergy_list.get_item_metadata(idx)
	if not path:
		SynergyHelpers.show_message('Selected synergy has no path metadata.')
		return
	var spec = _load_spec_from_path(path)
	if typeof(spec) == TYPE_DICTIONARY:
		# Load synergy into programmatic state only. Do NOT change ItemList selection.
		# This avoids automatic visual selection and the debounce handler overwriting the loaded spec.
		current_spec = spec
		loaded_synergy_path = path
		# Clear any previously-stored programmatic match indices — we no longer auto-select aspects on load
		_last_matched_indices = []
		# Deselect any visual aspect selection
		if aspect_list:
			if aspect_list.has_method('deselect_all'):
				aspect_list.deselect_all()
			else:
				for i in range(aspect_list.get_item_count()):
					if aspect_list.is_selected(i):
						if aspect_list.has_method('deselect'):
							aspect_list.deselect(i)
						elif aspect_list.has_method('set_item_selected'):
							aspect_list.set_item_selected(i, false)
		# Cancel any pending debounce so residual UI events won't trigger generation immediately
		_cancel_selection_debounce()
		# Populate the Mid UI from the programmatic spec only
		_populate_mid_controls(current_spec)
		SynergyHelpers.show_message('Loaded synergy: ' + path)
	else:
		SynergyHelpers.show_message('Failed to load synergy: ' + path)


func _on_delete_synergy_pressed() -> void:
	# Open a confirm dialog showing the pretty-printed JSON of the selected synergy
	var sel = synergy_list.get_selected_items()
	if sel.size() == 0:
		SynergyHelpers.show_message('Select a synergy in the list first.')
		return
	var idx = sel[0]
	var path = synergy_list.get_item_metadata(idx)
	if not path:
		SynergyHelpers.show_message('Selected synergy has no path metadata.')
		return
	var spec = _load_spec_from_path(path)
	if typeof(spec) != TYPE_DICTIONARY:
		SynergyHelpers.show_message('Failed to load synergy for deletion preview: ' + path)
		return
	# pretty-print JSON into the dialog text field
	if synergy_json_edit:
		synergy_json_edit.text = _pretty_json(spec)
	# remember path and popup
	pending_delete_path = path
	if confirm_dialog:
		# ensure the dialog title and label reflect synergy deletion
		confirm_dialog.title = "Delete Synergy?"
		var lbl = confirm_dialog.get_node_or_null("BoxContainer/Label")
		if lbl:
			lbl.text = "Are you sure you want to delete the following synergy?"
		confirm_dialog.popup_centered()


func _on_confirm_delete_cancel_pressed() -> void:
	# Cancel deletion: clear pending path and hide dialog
	pending_delete_path = ""
	if confirm_dialog:
		confirm_dialog.hide()


func _on_confirm_delete_synergy_confirmed() -> void:
	if pending_delete_path == "":
		SynergyHelpers.show_message('No pending synergy to delete.')
		return
	var path = pending_delete_path
	var removed = false
	# determine whether this is a synergy or aspect based on path prefix
	if path.begins_with('res://synergies/'):
		var fname = path.get_file()
		var dir = DirAccess.open('res://synergies')
		if dir:
			if not dir.dir_exists('trash'):
				dir.make_dir('trash')
			var src_file = FileAccess.open(path, FileAccess.ModeFlags.READ)
			if src_file:
				var contents = src_file.get_as_text()
				src_file.close()
				var dest_path = 'res://synergies/trash/' + fname
				var dest_file = FileAccess.open(dest_path, FileAccess.ModeFlags.WRITE)
				if dest_file:
					dest_file.store_string(contents)
					dest_file.close()
					if dir.has_method('remove_file'):
						var derr = dir.remove_file(fname)
						if derr == OK:
							removed = true
					elif dir.has_method('remove'):
						var derr2 = dir.remove(fname)
						if derr2 == OK:
							removed = true
			dir.list_dir_end()
		else:
			SynergyHelpers.show_message('Deletion not supported on this editor build. Please remove the file manually: ' + path)
	elif path.begins_with('res://aspects/'):
		var fname2 = path.get_file()
		var dir2 = DirAccess.open('res://aspects')
		if dir2:
			if not dir2.dir_exists('trash'):
				dir2.make_dir('trash')
			var src_file2 = FileAccess.open(path, FileAccess.ModeFlags.READ)
			if src_file2:
				var contents2 = src_file2.get_as_text()
				src_file2.close()
				var dest_path2 = 'res://aspects/trash/' + fname2
				var dest_file2 = FileAccess.open(dest_path2, FileAccess.ModeFlags.WRITE)
				if dest_file2:
					dest_file2.store_string(contents2)
					dest_file2.close()
					if dir2.has_method('remove_file'):
						var derr = dir2.remove_file(fname2)
						if derr == OK:
							removed = true
					elif dir2.has_method('remove'):
						var derr2 = dir2.remove(fname2)
						if derr2 == OK:
							removed = true
			dir2.list_dir_end()
		else:
			SynergyHelpers.show_message('Deletion not supported on this editor build. Please remove the file manually: ' + path)
	else:
		SynergyHelpers.show_message('Deletion handler only supports synergies and aspects: ' + path)

	if removed:
		SynergyHelpers.show_message('Deleted: ' + path)
		pending_delete_path = ""
		# refresh both lists
		_refresh_synergy_list()
		_refresh_aspect_list()
		# close dialog if open
		if confirm_dialog:
			confirm_dialog.hide()
	else:
		SynergyHelpers.show_message('Failed to delete: ' + path)


func _select_aspects_for_spec(spec:Dictionary) -> void:
	if not spec or not spec.has('component_aspects'):
		return
	var wanted = spec['component_aspects']
	if typeof(wanted) != TYPE_ARRAY:
		return
	# clear selection then select items that match by resource name or filename
	if aspect_list.has_method('deselect_all'):
		aspect_list.deselect_all()
	else:
		for i in range(aspect_list.get_item_count()):
			if aspect_list.is_selected(i):
				# use available deselect method
				if aspect_list.has_method('deselect'):
					aspect_list.deselect(i)
				elif aspect_list.has_method('set_item_selected'):
					aspect_list.set_item_selected(i, false)

	# prepare a lowercase set of wanted identifiers for case-insensitive matching
	var wanted_set = {}
	for w in wanted:
		var w_norm = str(w).strip_edges().to_lower()
		if w_norm != "":
			wanted_set[w_norm] = true

	# collectors for matched items
	var matched_paths := []
	var matched_indices := []

	for i in range(aspect_list.get_item_count()):
		var path = aspect_list.get_item_metadata(i)
		var r = ResourceLoader.load(path)
		var name_variant = ""
		# file base without extension
		var file_variant = path.get_file().split('.')[0]
		# prefer explicit get_name() method, fall back to property 'name' if present
		if r:
			if r.has_method('get_name'):
				name_variant = str(r.get_name())
			elif r.has('name'):
				name_variant = str(r.get('name'))
		# normalize for comparison
		var name_l = name_variant.strip_edges().to_lower()
		var file_l = file_variant.strip_edges().to_lower()
		print("[SynergyPlugin] aspect item", i, "name=", name_variant, "name_l=", name_l, "file=", file_variant, "file_l=", file_l)
		if wanted_set.has(name_l) or wanted_set.has(file_l):
			# select the item - prefer set_item_selected which preserves existing selection
			if aspect_list.has_method('set_item_selected'):
				aspect_list.set_item_selected(i, true)
			elif aspect_list.has_method('select'):
				aspect_list.select(i)
			# collect matched path for direct scaler population
			matched_paths.append(path)
			matched_indices.append(i)

	# remember matched indices for debounce snapshotting (don't rely on ItemList visual state)
	_last_matched_indices = matched_indices
	print('[SynergyPlugin] matched_indices=', matched_indices, ' matched_paths=', matched_paths)

	# Trigger suggestion flow so scalers populate from aspects if the synergy lacks them
	# If we found matched_paths, populate scalers directly from those paths to avoid relying on UI selection
	if matched_paths and matched_paths.size() > 0:
		# build comp_names from spec if present
		var comp_names = []
		if spec and spec.has('component_aspects'):
			comp_names = spec['component_aspects']
		_populate_scalers_from_paths(matched_paths, comp_names)
	else:
		# Trigger suggestion flow so scalers populate from aspects if the synergy lacks them
		_on_suggest_defaults()



func _find_existing_synergy_for_components(comp_aspects:Array) -> String:
	# Scan res://synergies and look for a synergy whose component_aspects matches the provided array (order-insensitive)
	print('[SynergyPlugin] _find_existing_synergy_for_components input comp_aspects=', comp_aspects)
	if typeof(comp_aspects) != TYPE_ARRAY:
		print('[SynergyPlugin] _find_existing_synergy_for_components: comp_aspects not array')
		return ""
	# normalize wanted set
	var wanted_set = {}
	for a in comp_aspects:
		var k = str(a).strip_edges().to_lower()
		if k != "":
			wanted_set[k] = true
	print('[SynergyPlugin] _find_existing_synergy_for_components wanted_set=', wanted_set)
	var dir = DirAccess.open('res://synergies')
	if not dir:
		return ""
	dir.list_dir_begin()
	var fname = dir.get_next()
	while fname != "":
		if not dir.current_is_dir():
			if fname.ends_with('.tres') or fname.ends_with('.res') or fname.ends_with('.json'):
				var path = 'res://synergies/' + fname
				var spec = _load_spec_from_path(path)
				if typeof(spec) == TYPE_DICTIONARY and spec.has('component_aspects'):
					var sa = spec['component_aspects']
					if typeof(sa) == TYPE_ARRAY and sa.size() == comp_aspects.size():
						var all_match = true
						for s in sa:
							var sk = str(s).strip_edges().to_lower()
							if not wanted_set.has(sk):
								all_match = false
								break
						if all_match:
							print('[SynergyPlugin] existing synergy match:', path)
							return path
		fname = dir.get_next()
	dir.list_dir_end()
	return ""


func _load_spec_from_path(path:String) -> Variant:
	# Load a synergy spec from .tres/.res resource or .json file. Returns Dictionary or null.
	if path.ends_with('.tres') or path.ends_with('.res'):
		var r = ResourceLoader.load(path)
		if r:
			if r.has_method('get_spec'):
				return r.get_spec()
			elif r.has('spec'):
				return r.get('spec')
			else:
				# try to access properties via to_dict if available
				return null
		return null
	elif path.ends_with('.json'):
		var f = FileAccess.open(path, FileAccess.ModeFlags.READ)
		if not f:
			return null
		var txt = f.get_as_text()
		f.close()
		var pr = SynergyHelpers.parse_json_text(txt)
		if pr.has('error') and pr['error'] == OK:
			return pr.get('result', null)
		return null
	return null


func _populate_scalers_from_paths(paths:Array, comp_names:Array) -> void:
	var gathered = {}
	for p in paths:
		var r = ResourceLoader.load(p)
		_add_scalers_from_res(r, gathered)
	var defaults = {}
	if gathered.size() > 0:
		for k in gathered.keys():
			var items = gathered[k]
			var merged = MergeRules.apply_rule(k, items, comp_names.size())
			defaults[k] = merged
		# merge into current_spec and refresh UI
		if typeof(current_spec) != TYPE_DICTIONARY:
			current_spec = {}
		current_spec["default_scalers"] = defaults
		_populate_mid_controls(current_spec)
		return
	# nothing gathered; do nothing
	return

func _get_selected_aspect_paths():
	var out = []
	for i in range(aspect_list.get_item_count()):
		if aspect_list.is_selected(i):
			out.append(aspect_list.get_item_metadata(i))
	return out


func _get_paths_for_component_names(comp_names:Array) -> Array:
	# Given an array of component names (strings), return the matching aspect resource paths
	# in the same order as comp_names where possible. Matching is case-insensitive and
	# will use the resource's get_name() or file base name as a fallback.
	var out = []
	if typeof(comp_names) != TYPE_ARRAY:
		return out
	# build a lookup from normalized name -> list of indices to allow multiple matching items
	var name_map = {}
	for i in range(aspect_list.get_item_count()):
		var path = aspect_list.get_item_metadata(i)
		if not path:
			continue
		var r = ResourceLoader.load(path)
		var label = ""
		if r and r.has_method('get_name'):
			label = str(r.get_name())
		else:
			label = path.get_file().split('.')[0]
		var key = label.strip_edges().to_lower()
		if not name_map.has(key):
			name_map[key] = []
		name_map[key].append(path)

	# for each requested component name, pick the first matching path in the map
	for cn in comp_names:
		var kk = str(cn).strip_edges().to_lower()
		if kk == "":
			continue
		if name_map.has(kk) and name_map[kk].size() > 0:
			out.append(name_map[kk][0])
	# return paths in requested order (some may be missing)
	return out


func _set_component_aspects_from_paths(paths:Array) -> void:
	# Build component_aspects names from resource paths and store on current_spec
	var comp_aspects = []
	for p in paths:
		var r = ResourceLoader.load(p)
		if r and r.has_method("get_name"):
			comp_aspects.append(r.get_name())
		else:
			var parts = p.get_file().split('.')
			comp_aspects.append(parts[0])
	if typeof(current_spec) != TYPE_DICTIONARY:
		current_spec = {}
	current_spec["component_aspects"] = comp_aspects
	# user-driven selection should clear loaded path so we operate programmatically
	loaded_synergy_path = ""


	

func _on_generate_proposal():
	# Use programmatic `current_spec.component_aspects` as the authoritative source if present.
	# Otherwise fall back to any stored matched indices or the visual selection.
	var comp_aspects = []
	if typeof(current_spec) == TYPE_DICTIONARY and current_spec.has('component_aspects') and typeof(current_spec['component_aspects']) == TYPE_ARRAY and current_spec['component_aspects'].size() > 0:
		comp_aspects = current_spec['component_aspects']
	else:
		# prefer previously-stored matched indices (from programmatic load), then UI selection
		if _last_matched_indices and _last_matched_indices.size() > 0:
			for i in _last_matched_indices:
				comp_aspects.append(aspect_list.get_item_metadata(i).get_file().split('.')[0])
			_last_matched_indices = []
		else:
			# build comp_aspects from user selection
			var sel_paths = _get_selected_aspect_paths()
			if sel_paths.size() > 0:
				_set_component_aspects_from_paths(sel_paths)
				comp_aspects = current_spec.get('component_aspects', [])

	# Build ordered paths for provided component names
	var paths = _get_paths_for_component_names(comp_aspects)
	print('[SynergyPlugin] _on_generate_proposal comp_aspects=', comp_aspects, ' resolved_paths=', paths)
	if paths.size() == 0:
		SynergyHelpers.show_message("Please select one or more aspects to generate a proposal.")
		var default_spec = {
			"id": "",
			"name": "",
			"component_aspects": [],
			"default_scalers": {},
			"extra_executors": []
		}
		# Clear loaded path and in-memory spec so mid UI shows empty/default values
		loaded_synergy_path = ""
		current_spec = default_spec
		# clear any previously matched indices used for debounce snapshotting
		_last_matched_indices = []
		_populate_mid_controls(default_spec)
		return
	# Build a basic proposal dict (comp_aspects already computed)
	# Normalize comp_aspects: ensure they are string names
	var comp_names = []
	for a in comp_aspects:
		comp_names.append(str(a))

	var id = SynergyHelpers.join_array(comp_names, "_").to_lower().replace(" ", "_")
	var name_parts = []
	for s in comp_names:
		name_parts.append(str(s).capitalize())
	var name = SynergyHelpers.join_array(name_parts, " ")

	var spec = {
		"id": id,
		"name": name,
		"component_aspects": comp_aspects,
		"default_scalers": {},
		"extra_executors": []
	}


	# If a composed synergy already exists for these components, load it instead
	print('[SynergyPlugin] _on_generate_proposal comp_aspects=', comp_names)
	var existing = _find_existing_synergy_for_components(comp_names)
	if existing != "":
		var loaded = _load_spec_from_path(existing)
		if typeof(loaded) == TYPE_DICTIONARY:
			current_spec = loaded
			loaded_synergy_path = existing
			_populate_mid_controls(current_spec)
			SynergyHelpers.show_message("Loaded existing synergy: " + existing)
			return

	# otherwise use generated proposal
	loaded_synergy_path = ""
	current_spec = spec
	_populate_mid_controls(spec)


func _on_aspect_activated(idx:int) -> void:
	# Double-clicked an aspect -> start edit flow for the selected aspect
	if idx < 0:
		return
	# ensure the item is selected
	if aspect_list:
		if aspect_list.has_method('set_item_selected'):
			aspect_list.set_item_selected(idx, true)
		elif aspect_list.has_method('select'):
			aspect_list.select(idx)
	# delegate to the edit handler
	_on_edit_aspect_pressed()


func _ensure_aspect_dialog() -> void:
	if aspect_dialog:
		return
	# create a simple AcceptDialog for creating/editing aspects
	aspect_dialog = AcceptDialog.new()
	aspect_dialog.title = "Create / Edit Aspect"
	var cont = VBoxContainer.new()
	aspect_dialog.add_child(cont)
	var id_row = HBoxContainer.new()
	var id_label = Label.new()
	id_label.text = "ID:"
	id_row.add_child(id_label)
	aspect_dialog_id_edit = LineEdit.new()
	id_row.add_child(aspect_dialog_id_edit)
	cont.add_child(id_row)
	var name_row = HBoxContainer.new()
	var name_label = Label.new()
	name_label.text = "Name:"
	name_row.add_child(name_label)
	aspect_dialog_name_edit = LineEdit.new()
	name_row.add_child(aspect_dialog_name_edit)
	cont.add_child(name_row)
	# Default scalers editor (ItemList + controls) inside the aspect dialog
	var scalers_label = Label.new()
	scalers_label.text = "Default Scalers"
	cont.add_child(scalers_label)
	aspect_scalers_list = ItemList.new()
	aspect_scalers_list.name = "AspectScalersList"
	aspect_scalers_list.custom_minimum_size = Vector2(0, 120)
	cont.add_child(aspect_scalers_list)
	var scalers_controls = HBoxContainer.new()
	var add_aspect_scaler_btn = Button.new()
	add_aspect_scaler_btn.text = "Add Scaler"
	add_aspect_scaler_btn.connect("pressed", Callable(self, "_on_aspect_add_scaler_pressed"))
	scalers_controls.add_child(add_aspect_scaler_btn)
	var edit_aspect_scaler_btn = Button.new()
	edit_aspect_scaler_btn.text = "Edit Scaler"
	edit_aspect_scaler_btn.connect("pressed", Callable(self, "_on_aspect_edit_scaler_pressed"))
	scalers_controls.add_child(edit_aspect_scaler_btn)
	var remove_aspect_scaler_btn = Button.new()
	remove_aspect_scaler_btn.text = "Remove Scaler"
	remove_aspect_scaler_btn.connect("pressed", Callable(self, "_on_aspect_remove_scaler_pressed"))
	scalers_controls.add_child(remove_aspect_scaler_btn)
	cont.add_child(scalers_controls)
	# connect confirm
	aspect_dialog.connect("confirmed", Callable(self, "_on_aspect_dialog_confirmed"))
	panel.add_child(aspect_dialog)


func _on_aspect_dialog_confirmed() -> void:
	# Create or update aspect resource based on dialog fields
	var idv = str(aspect_dialog_id_edit.text).strip_edges()
	var namev = str(aspect_dialog_name_edit.text).strip_edges()
	if idv == "":
		SynergyHelpers.show_message('Aspect ID cannot be empty.')
		return
	# determine path
	var path = 'res://aspects/' + idv + '.tres'
	# if editing existing aspect, override path
	if editing_aspect_path != "":
		path = editing_aspect_path
	# Build or update a typed Aspect resource if available
	var r = null
	# If editing, load existing resource so we preserve other fields
	if editing_aspect_path != "":
		r = ResourceLoader.load(editing_aspect_path)
	else:
		# prefer to instantiate the Aspect class if registered
		if ClassDB.class_exists('Aspect'):
			r = Aspect.new()
		else:
			# fallback to generic Resource so the file still saves
			r = Resource.new()
	# set name property via setter if present
	if r:
		if r.has_method('set_name'):
			r.set_name(namev)
		elif r.has('name'):
			r.set('name', namev)
	# collect default scalers from dialog data and set on resource
	var defaults = {}
	for entry in aspect_scalers_data:
		if typeof(entry) == TYPE_DICTIONARY and entry.has('key'):
			var kk = str(entry['key']).strip_edges()
			if kk != "":
				defaults[kk] = entry.get('value')
	# set default_scalers via typed API where available
	if r:
		if r.has_method('set_default_scalers'):
			r.set_default_scalers(defaults)
		elif r.has('default_scalers'):
			r.set('default_scalers', defaults)
	# Save resource
	var err = ResourceSaver.save(r, path)
	if err == OK:
		SynergyHelpers.show_message('Saved aspect: ' + path)
		editing_aspect_path = ""
		_refresh_aspect_list()
		# rebuild catalog and refresh informative panel immediately
		_build_aspect_scaler_catalog()
		_refresh_aspect_defaults_panel()
		if aspect_dialog:
			aspect_dialog.hide()
	else:
		SynergyHelpers.show_message('Failed to save aspect (ResourceSaver returned: ' + str(err) + ').')


func _on_edit_aspect_pressed() -> void:
	# Edit the currently-selected aspect using the dialog (same UX as Create)
	if not aspect_list:
		SynergyHelpers.show_message('Aspect list not available.')
		return
	var sel = []
	if aspect_list.has_method('get_selected_items'):
		for v in aspect_list.get_selected_items():
			sel.append(v)
	else:
		for i in range(aspect_list.get_item_count()):
			if aspect_list.is_selected(i):
				sel.append(i)
	if sel.size() == 0:
		SynergyHelpers.show_message('Select an aspect to edit.')
		return
	var idx = sel[0]
	var path = aspect_list.get_item_metadata(idx)
	if not path:
		SynergyHelpers.show_message('Selected aspect has no path metadata.')
		return
	var r = ResourceLoader.load(path)
	if not r:
		SynergyHelpers.show_message('Failed to load aspect resource: ' + path)
		return
	# ensure dialog exists and populate fields
	_ensure_aspect_dialog()
	editing_aspect_path = path
	# try to read friendly name from resource
	var name_val = ""
	if r.has_method('get_name'):
		name_val = str(r.get_name())
	elif r.has('name'):
		name_val = str(r.get('name'))
	aspect_dialog_name_edit.text = name_val
	# derive id from filename if possible
	aspect_dialog_id_edit.text = path.get_file().split('.')[0]
	# populate aspect scalers from resource if available
	aspect_scalers_data.clear()
	if r:
		var vals = null
		if r.has_method('get_default_scalers'):
			vals = r.get_default_scalers()
		elif r.has('default_scalers'):
			vals = r.get('default_scalers')
		if typeof(vals) == TYPE_DICTIONARY:
			for k in vals.keys():
				aspect_scalers_data.append({"key": k, "value": vals[k]})
	_refresh_aspect_scalers_list()
	aspect_dialog.popup_centered()


func _on_create_aspect_pressed() -> void:
	# Show a dialog to create a new Aspect resource (id + name)
	_ensure_aspect_dialog()
	editing_aspect_path = ""
	aspect_dialog_id_edit.text = ""
	aspect_dialog_name_edit.text = ""
	aspect_dialog.popup_centered()


func _on_delete_aspect_pressed() -> void:
	# Reuse the ConfirmDialog UI for aspect deletion confirmation
	if not aspect_list:
		SynergyHelpers.show_message('Aspect list not available.')
		return
	var sel = []
	if aspect_list.has_method('get_selected_items'):
		for v in aspect_list.get_selected_items():
			sel.append(v)
	else:
		for i in range(aspect_list.get_item_count()):
			if aspect_list.is_selected(i):
				sel.append(i)
	if sel.size() == 0:
		SynergyHelpers.show_message('Select an aspect to delete.')
		return
	var idx = sel[0]
	var path = aspect_list.get_item_metadata(idx)
	if not path:
		SynergyHelpers.show_message('Selected aspect has no path metadata.')
		return
	# Load resource and pretty-print JSON into confirm dialog
	var r = ResourceLoader.load(path)
	var preview = {}
	if r:
		# try to present as a small dict
		if r.has_method('get_default_scalers') and r.has_method('get_name'):
			preview['name'] = r.get_name()
		else:
			preview['path'] = path
	else:
		preview['path'] = path
	if synergy_json_edit:
		synergy_json_edit.text = _pretty_json(preview)
	# store pending path and popup confirmation
	pending_delete_path = path
	if confirm_dialog:
		# set context-appropriate title for aspect deletion
		confirm_dialog.title = "Delete Aspect?"
		# update confirm dialog label to reference 'aspect' instead of 'synergy'
		var lbl = confirm_dialog.get_node_or_null("BoxContainer/Label")
		if lbl:
			lbl.text = "Are you sure you want to delete the following aspect?"
		confirm_dialog.popup_centered()

func _on_suggest_defaults():
	# Simple heuristic: set amount and mana_cost multipliers based on number of aspects
	if typeof(current_spec) != TYPE_DICTIONARY or current_spec.size() == 0:
		SynergyHelpers.show_message("Generate a proposal first.")
		return
	var proposal = current_spec

	# Determine component names and aspect resource paths using programmatic spec first
	var comp_names = []
	if proposal.has("component_aspects") and typeof(proposal["component_aspects"]) == TYPE_ARRAY:
		comp_names = proposal["component_aspects"]

	var aspect_paths = []
	if comp_names.size() > 0:
		# resolve names -> paths (in-order) using the aspect list
		aspect_paths = _get_paths_for_component_names(comp_names)
	else:
		# fallback to UI selection
		aspect_paths = _get_selected_aspect_paths()

	var gathered = {}


	# load from resolved aspect_paths first
	if aspect_paths.size() > 0:
		for p in aspect_paths:
			var r = ResourceLoader.load(p)
			_add_scalers_from_res(r, gathered)
	else:
		# try to match by name across all aspects (case-insensitive)
		for i in range(aspect_list.get_item_count()):
			var path = aspect_list.get_item_metadata(i)
			var r = ResourceLoader.load(path)
			var label = ""
			if r and r.has_method("get_name"):
				label = r.get_name()
			else:
				var parts = path.get_file().split(".")
				label = parts[0]
			for cn in comp_names:
				if str(cn).strip_edges().to_lower() == str(label).strip_edges().to_lower():
					_add_scalers_from_res(r, gathered)

	# If we gathered some scaler values, average numeric ones and pick strings otherwise
	var defaults = {}
	if gathered.size() > 0:
		for k in gathered.keys():
			var items = gathered[k]
			# debug: log what we're about to merge so we can trace mode selection
			print("[SynergyPlugin] merging key=", k, " items=", items, " comp_count=", comp_names.size())
			# use merge rules if available
			var merged = MergeRules.apply_rule(k, items, comp_names.size())
			defaults[k] = merged

	proposal["default_scalers"] = defaults
	current_spec = proposal
	_populate_mid_controls(proposal)

func _on_save_synergy():
	if typeof(current_spec) != TYPE_DICTIONARY or current_spec.size() == 0:
		SynergyHelpers.show_message("Nothing to save. Generate or paste a proposal first.")
		return
	var spec = current_spec

	# Merge in any edits from the Mid UI (id/name/scalers/executors)
	_merge_mid_controls_into_spec(spec)
	# validate minimal fields
	if not spec.has("id") or not spec.has("component_aspects"):
		SynergyHelpers.show_message("Proposal missing required keys 'id' or 'component_aspects'")
		return
	# If we previously loaded a synergy from disk, overwrite that file
	if loaded_synergy_path != "":
		var path = loaded_synergy_path
		if path.ends_with('.tres') or path.ends_with('.res'):
			var s = Synergy.new()
			s.set_spec(spec)
			var err = ResourceSaver.save(s, path)
			if err == OK:
				SynergyHelpers.show_message("Overwrote synergy: " + path)
				_refresh_synergy_list()
				return
			else:
				SynergyHelpers.show_message("Failed to save typed Synergy resource (ResourceSaver returned: " + str(err) + ").")
				# fallthrough to JSON fallback below
		elif path.ends_with('.json'):
			var file = FileAccess.open(path, FileAccess.ModeFlags.WRITE)
			if file:
				file.store_string(SynergyHelpers.json_stringify(spec))
				file.close()
				SynergyHelpers.show_message("Overwrote JSON synergy: " + path)
				_refresh_synergy_list()
				return
			else:
				SynergyHelpers.show_message("Failed to open existing JSON synergy for write: " + path)

	# No loaded synergy to overwrite: save a new one based on id
	var id = spec["id"].to_lower()
	var path_new = "res://synergies/" + id + ".tres"
	var s2 = Synergy.new()
	s2.set_spec(spec)
	var err2 = ResourceSaver.save(s2, path_new)
	if err2 == OK:
		SynergyHelpers.show_message("Saved synergy: " + path_new)
		loaded_synergy_path = path_new
		_refresh_synergy_list()
	else:
		# Fallback: write JSON spec
		SynergyHelpers.show_message("Failed to save typed Synergy resource (ResourceSaver returned: " + str(err2) + "). Attempting JSON fallback...")
		var json_path = "res://synergies/" + id + ".json"
		var file = FileAccess.open(json_path, FileAccess.ModeFlags.WRITE)
		if file:
			file.store_string(SynergyHelpers.json_stringify(spec))
			file.close()
			SynergyHelpers.show_message("Saved JSON fallback: " + json_path)
			loaded_synergy_path = json_path
			_refresh_synergy_list()
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
	_aspect_scaler_mode = false
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


func _on_aspect_add_scaler_pressed() -> void:
	aspect_scalers_editing_index = -1
	_aspect_scaler_mode = true
	scaler_key_edit.text = ""
	scaler_value_edit.text = ""
	scaler_dialog.popup_centered()


func _on_aspect_edit_scaler_pressed() -> void:
	if aspect_scalers_list == null:
		SynergyHelpers.show_message('Aspect scalers UI not available')
		return
	var sel = aspect_scalers_list.get_selected_items()
	if sel.size() == 0:
		SynergyHelpers.show_message('Select an aspect scaler to edit.')
		return
	var idx = sel[0]
	aspect_scalers_editing_index = idx
	var entry = aspect_scalers_data[idx]
	_aspect_scaler_mode = true
	scaler_key_edit.text = str(entry.get('key', ''))
	scaler_value_edit.text = str(entry.get('value', ''))
	scaler_dialog.popup_centered()


func _on_aspect_remove_scaler_pressed() -> void:
	if aspect_scalers_list == null:
		SynergyHelpers.show_message('Aspect scalers UI not available')
		return
	var sel = aspect_scalers_list.get_selected_items()
	if sel.size() == 0:
		SynergyHelpers.show_message('Select an aspect scaler to remove.')
		return
	var idx = sel[0]
	aspect_scalers_data.remove_at(idx)
	_refresh_aspect_scalers_list()


func _on_scaler_dialog_confirmed() -> void:
	# Handles both synergy scaler edits and aspect scaler edits depending on _aspect_scaler_mode
	var key = scaler_key_edit.text.strip_edges()
	var val_s = scaler_value_edit.text.strip_edges()
	if key == "":
		SynergyHelpers.show_message("Scaler key cannot be empty.")
		return
	var val
	if val_s.is_valid_float():
		# detect integer vs float
		if val_s.find('.') == -1:
			val = int(val_s)
		else:
			val = float(val_s)
	else:
		val = val_s
	if _aspect_scaler_mode:
		if aspect_scalers_editing_index >= 0 and aspect_scalers_editing_index < aspect_scalers_data.size():
			aspect_scalers_data[aspect_scalers_editing_index] = {"key": key, "value": val}
		else:
			aspect_scalers_data.append({"key": key, "value": val})
		aspect_scalers_editing_index = -1
		_refresh_aspect_scalers_list()
		# update catalog and panel so designers see changes immediately
		_build_aspect_scaler_catalog()
		_refresh_aspect_defaults_panel()
		# hide scaler dialog
		scaler_dialog.hide()
		return
	# synergy scaler flow
	if scalers_editing_index >= 0 and scalers_editing_index < scalers_data.size():
		scalers_data[scalers_editing_index] = {"key": key, "value": val}
	else:
		scalers_data.append({"key": key, "value": val})
	scalers_editing_index = -1
	_refresh_scalers_list()
	# update preview and merge into current_spec
	var spec = current_spec
	if typeof(spec) != TYPE_DICTIONARY:
		spec = {}
	_merge_mid_controls_into_spec(spec)
	# hide scaler dialog
	scaler_dialog.hide()


func _on_remove_scaler_pressed() -> void:
	# Remove selected scaler from synergy scalers UI
	var sel = scalers_list.get_selected_items()
	if sel.size() == 0:
		SynergyHelpers.show_message('Select a scaler to remove.')
		return
	var idx = sel[0]
	scalers_data.remove_at(idx)
	_refresh_scalers_list()
func _collect_scalers_from_ui() -> Dictionary:
	var out = {}
	for entry in scalers_data:
		if typeof(entry) == TYPE_DICTIONARY and entry.has("key"):
			out[str(entry["key"]).strip_edges()] = entry.get("value")
	return out


func _refresh_aspect_scalers_list() -> void:
	# update the UI list inside the aspect dialog (if present)
	if aspect_dialog == null:
		return
	# ensure we have a list reference
	if not aspect_scalers_list:
		# try to find by name inside dialog
		aspect_scalers_list = aspect_dialog.get_node_or_null("AspectScalersList")
		if not aspect_scalers_list:
			return
	aspect_scalers_list.clear()
	for entry in aspect_scalers_data:
		if typeof(entry) == TYPE_DICTIONARY:
			var k = str(entry.get('key', ''))
			var v = entry.get('value')
			var vs = ""
			if typeof(v) == TYPE_FLOAT or typeof(v) == TYPE_INT:
				vs = str(v)
			else:
				vs = str(v)
			aspect_scalers_list.add_item(k + ": " + vs)


func _build_aspect_scaler_catalog() -> void:
	# Scan res://aspects and build a catalog mapping scaler_key -> { value_string -> count }
	aspect_scaler_catalog.clear()
	var dir = DirAccess.open('res://aspects')
	if not dir:
		return
	dir.list_dir_begin()
	var fname = dir.get_next()
	while fname != "":
		if not dir.current_is_dir():
			if fname.ends_with('.tres') or fname.ends_with('.res') or fname.ends_with('.json'):
				var path = 'res://aspects/' + fname
				var r = ResourceLoader.load(path)
				if r:
					var vals = null
					if r.has_method('get_default_scalers'):
						vals = r.get_default_scalers()
					elif r.has('default_scalers'):
						vals = r.get('default_scalers')
					if typeof(vals) == TYPE_DICTIONARY:
						for k in vals.keys():
							var v = vals[k]
							var vs = SynergyHelpers.json_stringify(v)
							if not aspect_scaler_catalog.has(k):
								aspect_scaler_catalog[k] = {}
							var bucket = aspect_scaler_catalog[k]
							if not bucket.has(vs):
								bucket[vs] = 0
							bucket[vs] += 1
		fname = dir.get_next()
	dir.list_dir_end()


func _refresh_aspect_defaults_panel() -> void:
	# Populate the informative ItemList showing unique default_scalers across aspects
	if not aspect_defaults_panel:
		return
	if not aspect_defaults_list:
		# find inside panel if available
		aspect_defaults_list = aspect_defaults_panel.get_node_or_null('AspectDefaultsList')
	if not aspect_defaults_list:
		return
	aspect_defaults_list.clear()
	# show each scaler key and the total number of aspects using it (no per-value breakdown)
	for k in aspect_scaler_catalog.keys():
		var bucket = aspect_scaler_catalog[k]
		# total number of aspects that set this key (sum of counts)
		var total = 0
		for vs in bucket.keys():
			total += int(bucket[vs])
		aspect_defaults_list.add_item(str(k) + " (used by " + str(total) + ")")


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


func _add_scalers_from_res(res, gathered:Dictionary) -> void:
	if not res:
		return
	var vals = null
	if res.has_method("get_default_scalers"):
		vals = res.get_default_scalers()
	elif res.has("default_scalers"):
		vals = res.get("default_scalers")
	if typeof(vals) != TYPE_DICTIONARY:
		return
	for k in vals.keys():
		if not gathered.has(k):
			gathered[k] = []
		gathered[k].append(vals[k])


func _pretty_json(v, indent_level := 0) -> String:
	var indent = ""
	for i in range(indent_level):
		indent += "\t"
	var out = ""
	match typeof(v):
		TYPE_DICTIONARY:
			out += "{\n"
			var keys = v.keys()
			for i in range(keys.size()):
				var k = keys[i]
				out += indent + "\t" + '"' + str(k) + '": '
				out += _pretty_json(v[k], indent_level + 1)
				if i < keys.size() - 1:
					out += ",\n"
				else:
					out += "\n"
			out += indent + "}"
			return out
		TYPE_ARRAY:
			out += "[\n"
			for i in range(v.size()):
				out += indent + "\t" + _pretty_json(v[i], indent_level + 1)
				if i < v.size() - 1:
					out += ",\n"
				else:
					out += "\n"
			out += indent + "]"
			return out
		TYPE_STRING:
			return '"' + str(v).replace('"', '\\"') + '"'
		TYPE_INT, TYPE_FLOAT:
			return str(v)
		TYPE_BOOL:
			return "true" if v else "false"
		TYPE_NIL:
			return "null"
	# fallback
	return str(v)


func _refresh_executors_list() -> void:
	executors_list.clear()
	for e in executors_data:
		var label = ""
		if typeof(e) == TYPE_DICTIONARY and e.has("executor_id"):
			label = str(e["executor_id"])
		else:
			label = SynergyHelpers.json_stringify(e)
		executors_list.add_item(label)


func _on_add_exec_pressed() -> void:
	editing_exec_index = -1
	_clear_executor_dialog()
	# pre-populate params from schema if available for selected executor
	if exec_id_option and exec_schemas and exec_id_option.get_item_count() > 0:
		var sel = exec_id_option.get_selected()
		var eid = exec_id_option.get_item_text(sel)
		if exec_schemas.has(eid):
			var schema = exec_schemas[eid]
			# schema: key -> { type, default, desc }
			for k in schema.keys():
				var ent = schema[k]
				var dv = ""
				if typeof(ent) == TYPE_DICTIONARY and ent.has("default"):
					dv = ent["default"]
				# stringify non-string defaults for presentation
				var dv_s = ""
				if typeof(dv) == TYPE_STRING:
					dv_s = dv
				else:
					dv_s = SynergyHelpers.json_stringify(dv)
				if params_container:
					params_container.add_child(_create_param_row(str(k), dv_s))
	# ensure trigger dropdown shows current executor options
	_populate_trigger_dropdown()
	executor_dialog.popup_centered()


func _on_exec_id_selected(idx:int) -> void:
	# When the selected executor id changes in the dropdown, refresh the params UI
	if _suppress_exec_id_callbacks:
		return
	if not exec_id_option:
		return
	if idx < 0 or idx >= exec_id_option.get_item_count():
		return
	var eid = exec_id_option.get_item_text(idx)
	_populate_params_for_executor(eid)


func _populate_params_for_executor(eid:String) -> void:
	# Clear existing param rows and add rows from schema defaults if available
	if not params_container:
		return
	for c in params_container.get_children():
		c.queue_free()
	if not exec_schemas:
		return
	if typeof(exec_schemas) != TYPE_DICTIONARY:
		return
	if not exec_schemas.has(eid):
		return
	var schema = exec_schemas[eid]
	for k in schema.keys():
		var ent = schema[k]
		var dv = ""
		if typeof(ent) == TYPE_DICTIONARY and ent.has("default"):
			dv = ent["default"]
		var dv_s = ""
		if typeof(dv) == TYPE_STRING:
			dv_s = dv
		else:
			dv_s = SynergyHelpers.json_stringify(dv)
		params_container.add_child(_create_param_row(str(k), dv_s))


func _populate_trigger_dropdown() -> void:
	if not exec_trigger_option:
		return
	# clear existing
	exec_trigger_option.clear()
	# add empty 'none' option
	exec_trigger_option.add_item("")
	# prefer using the populated exec_id_option if available
	if exec_id_option and exec_id_option.get_item_count() > 0:
		for i in range(exec_id_option.get_item_count()):
			exec_trigger_option.add_item(exec_id_option.get_item_text(i))
	else:
		# fallback to keys in exec_schemas
		if typeof(exec_schemas) == TYPE_DICTIONARY:
			for k in exec_schemas.keys():
				exec_trigger_option.add_item(str(k))

func _on_aspect_selected(idx:int) -> void:
	# Debounced handling: when user changes selection, start timer to regenerate proposal and defaults
	if _suppress_selection_callbacks:
		# Ignore selection changes triggered programmatically
		return
	# When the user selects an aspect, clear any selection in the synergy list so
	# the UI doesn't imply a previously-loaded synergy is still selected.
	if synergy_list:
		# diagnostics: show node class and which common methods are present
		var cls = ""
		if synergy_list.has_method('get_class'):
			cls = synergy_list.get_class()
		print('[SynergyPlugin] _on_aspect_selected: synergy_list class=', cls)
		var methods_to_check = ['get_selected_items', 'deselect_all', 'deselect', 'set_item_selected', 'clear_selection', 'clear']
		for m in methods_to_check:
			print('[SynergyPlugin] _on_aspect_selected: has_method(', m, ')=', synergy_list.has_method(m))
		# show currently selected indices (if available)
		var before_sel = []
		if synergy_list.has_method('get_selected_items'):
			for v in synergy_list.get_selected_items():
				before_sel.append(v)
		print('[SynergyPlugin] _on_aspect_selected: before selected=', before_sel)
		# Try preferred deselection: iterate selected indices
		var did_clear = false
		if synergy_list.has_method('get_selected_items'):
			for si in synergy_list.get_selected_items():
				if synergy_list.has_method('deselect'):
					synergy_list.deselect(si)
					did_clear = true
				elif synergy_list.has_method('set_item_selected'):
					synergy_list.set_item_selected(si, false)
					did_clear = true
		# fallback: call deselect_all if available
		if not did_clear and synergy_list.has_method('deselect_all'):
			synergy_list.deselect_all()
			if synergy_list.has_method('get_selected_items'):
				# clear implied selection list
				for v in synergy_list.get_selected_items():
					pass
				did_clear = true
		# fallback: try clear_selection
		if not did_clear and synergy_list.has_method('clear_selection'):
			synergy_list.clear_selection()
			did_clear = true
		# last resort: sweep all items and unset selected flag where possible
		if not did_clear:
			for i in range(synergy_list.get_item_count()):
				if synergy_list.has_method('set_item_selected'):
					synergy_list.set_item_selected(i, false)
		# diagnostics: show selection after attempts
		var after_sel = []
		if synergy_list.has_method('get_selected_items'):
			for v in synergy_list.get_selected_items():
				after_sel.append(v)
		print('[SynergyPlugin] _on_aspect_selected: after selected=', after_sel)
	if selection_timer:
		selection_timer.start()
	else:
		_on_debounced_aspect_change()


func _process(delta:float) -> void:
	# ensure we try to populate executor dropdown if the native module exported schemas
	# sometime after plugin initialization (module init ordering can vary between editor builds)
	_ensure_execs_loaded()

	# Poll selection state for the aspect list and auto-update when it changes.
	if not aspect_list:
		return
	var sel_packed = aspect_list.get_selected_items()
	# convert PackedInt32Array (or similar) into a regular Array for safe comparisons
	var sel = []
	for i in sel_packed:
		sel.append(i)
	# compare arrays (order-sensitive). If selection changed, regenerate
	if sel.size() != _prev_aspect_selection.size() or sel != _prev_aspect_selection:
		_prev_aspect_selection = sel.duplicate()
		# start debounce unless suppression is active
		if not _suppress_selection_callbacks:
			if selection_timer:
				selection_timer.start()
			else:
				_on_debounced_aspect_change()


func _on_edit_exec_pressed() -> void:
	var sel = executors_list.get_selected_items()
	if sel.size() == 0:
		SynergyHelpers.show_message("Select an executor to edit.")
		return
	var idx = sel[0]
	editing_exec_index = idx
	_clear_executor_dialog()
	_populate_executor_dialog_with_obj(executors_data[idx])
	executor_dialog.popup_centered()


func _on_executor_activated(idx:int) -> void:
	# When an executor item is double-clicked in the list, open the editor for that executor
	if idx < 0:
		return
	if idx >= executors_data.size():
		return
	editing_exec_index = idx
	_clear_executor_dialog()
	_populate_executor_dialog_with_obj(executors_data[idx])
	if executor_dialog:
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
	# Build executor object from structured dialog
	var out = {}
	# executor id from dropdown
	if exec_id_option and exec_id_option.get_item_count() > 0:
		var sel = exec_id_option.get_selected()
		out["executor_id"] = exec_id_option.get_item_text(sel)
	# trigger
	# trigger: support either the new OptionButton (exec_trigger_option) or legacy LineEdit (exec_trigger_edit)
	if exec_trigger_option:
		if exec_trigger_option.get_item_count() > 0:
			var sel_t = exec_trigger_option.get_selected()
			var t = str(exec_trigger_option.get_item_text(sel_t)).strip_edges()
			if t != "":
				out["trigger_on_executor"] = t
	elif exec_trigger_edit:
		var t = str(exec_trigger_edit.text).strip_edges()
		if t != "":
			out["trigger_on_executor"] = t
	# params
	var params = _gather_params_from_dialog()
	for k in params.keys():
		out[k] = params[k]

	if out.size() == 0:
		SynergyHelpers.show_message("No executor data provided.")
		return
	if editing_exec_index >= 0 and editing_exec_index < executors_data.size():
		executors_data[editing_exec_index] = out
	else:
		executors_data.append(out)
	editing_exec_index = -1
	_refresh_executors_list()

	# hide dialog
	if executor_dialog:
		executor_dialog.hide()


func _clear_executor_dialog() -> void:
	# clear dropdown selection (leave items), clear trigger and remove param rows
	if exec_id_option:
		if exec_id_option.get_item_count() > 0:
			exec_id_option.select(0)
	if exec_trigger_option:
		if exec_trigger_option.get_item_count() > 0:
			exec_trigger_option.select(0)
	elif exec_trigger_edit:
		exec_trigger_edit.text = ""
	if params_container:
		for c in params_container.get_children():
			c.queue_free()


func _on_add_param_pressed() -> void:
	if params_container:
		var row = _create_param_row("", "")
		params_container.add_child(row)


func _create_param_row(key:String, val:String) -> HBoxContainer:
	var row = HBoxContainer.new()
	row.size_flags_horizontal = 3
	var k = LineEdit.new()
	k.placeholder_text = "key"
	k.text = str(key)
	k.size_flags_horizontal = 3
	row.add_child(k)
	var v = LineEdit.new()
	v.placeholder_text = "value"
	v.text = str(val)
	v.size_flags_horizontal = 3
	row.add_child(v)
	var rem = Button.new()
	rem.text = "Remove"
	rem.size_flags_horizontal = 0
	# bind the remove button so handler knows which button to remove
	var cb = Callable(self, "_on_remove_param_pressed").bind(rem)
	rem.connect("pressed", cb)
	row.add_child(rem)
	return row


func _on_remove_param_pressed(btn) -> void:
	if not btn:
		return
	var row = btn.get_parent()
	if row and params_container:
		params_container.remove_child(row)
		row.queue_free()


func _gather_params_from_dialog() -> Dictionary:
	var out = {}
	if not params_container:
		return out
	for row in params_container.get_children():
		# expect HBoxContainer with two LineEdits then a button
		var children = row.get_children()
		if children.size() >= 2:
			var k = str(children[0].text).strip_edges()
			var v_s = str(children[1].text).strip_edges()
			if k == "":
				continue
			var v = null
			if v_s.is_valid_float():
				# is_valid_float returns true for numeric strings; detect integers (no '.') to cast to int
				if v_s.find(".") == -1:
					v = int(v_s)
				else:
					v = float(v_s)
			elif v_s.to_lower() == "true":
				v = true
			elif v_s.to_lower() == "false":
				v = false
			else:
				v = v_s
			out[k] = v
	return out


func _populate_executor_dialog_with_obj(obj) -> void:
	if not obj or typeof(obj) != TYPE_DICTIONARY:
		return
	_clear_executor_dialog()
	# select executor id if present and in dropdown
	if obj.has("executor_id") and exec_id_option:
		var want = str(obj["executor_id"])
		# programmatic selection should not trigger the live params callback; suppress it
		_suppress_exec_id_callbacks = true
		for i in range(exec_id_option.get_item_count()):
			if exec_id_option.get_item_text(i) == want:
				exec_id_option.select(i)
				break
		_suppress_exec_id_callbacks = false
	# trigger key
	if obj.has("trigger_on_executor"):
		var want_t = str(obj["trigger_on_executor"]).strip_edges()
		if exec_trigger_option:
			# find matching item
			for i in range(exec_trigger_option.get_item_count()):
				if exec_trigger_option.get_item_text(i) == want_t:
					exec_trigger_option.select(i)
					break
		elif exec_trigger_edit:
			exec_trigger_edit.text = want_t
	# other keys -> params
	for k in obj.keys():
		if k == "executor_id" or k == "trigger_on_executor":
			continue
		var v = obj[k]
		var s = ""
		if typeof(v) == TYPE_STRING:
			s = v
		else:
			s = SynergyHelpers.json_stringify(v)
		if params_container:
			params_container.add_child(_create_param_row(str(k), s))

	# fill any missing params from schema defaults
	if obj.has("executor_id") and exec_schemas and exec_schemas.has(str(obj["executor_id"])):
		var schema = exec_schemas[str(obj["executor_id"])]
		for sk in schema.keys():
			if obj.has(sk):
				continue
			var ent = schema[sk]
			var dv = ""
			if typeof(ent) == TYPE_DICTIONARY and ent.has("default"):
				dv = ent["default"]
			var dv_s = ""
			if typeof(dv) == TYPE_STRING:
				dv_s = dv
			else:
				dv_s = SynergyHelpers.json_stringify(dv)
			if params_container:
				params_container.add_child(_create_param_row(str(sk), dv_s))


func _on_debug_execs_pressed() -> void:
	# Diagnostic prints to help track whether executors were exported by the native module
	print("[SynergyPlugin][DEBUG] ProjectSettings has spellengine/executor_ids=", ProjectSettings.has_setting("spellengine/executor_ids"))
	if ProjectSettings.has_setting("spellengine/executor_ids"):
		var ids = ProjectSettings.get_setting("spellengine/executor_ids")
		var ids_count = 0
		if typeof(ids) == TYPE_ARRAY:
			ids_count = ids.size()
		print("[SynergyPlugin][DEBUG] executor_ids type=", typeof(ids), " count=", ids_count)
		print("[SynergyPlugin][DEBUG] executor_ids value=", ids)
	else:
		print("[SynergyPlugin][DEBUG] executor_ids not found in ProjectSettings")

	print("[SynergyPlugin][DEBUG] ProjectSettings has spellengine/executor_schemas=", ProjectSettings.has_setting("spellengine/executor_schemas"))
	if ProjectSettings.has_setting("spellengine/executor_schemas"):
		var schemas = ProjectSettings.get_setting("spellengine/executor_schemas")
		print("[SynergyPlugin][DEBUG] executor_schemas type=", typeof(schemas))
		# if it's a dictionary, show keys
		if typeof(schemas) == TYPE_DICTIONARY:
			print("[SynergyPlugin][DEBUG] executor_schemas keys=", schemas.keys())
		else:
			print("[SynergyPlugin][DEBUG] executor_schemas value=", schemas)

	# Dropdown state
	if exec_id_option:
		print("[SynergyPlugin][DEBUG] exec_id_option item_count=", exec_id_option.get_item_count())
		var items = []
		for i in range(exec_id_option.get_item_count()):
			items.append(exec_id_option.get_item_text(i))
		print("[SynergyPlugin][DEBUG] exec_id_option items=", items)
	else:
		print("[SynergyPlugin][DEBUG] exec_id_option not present in UI")

	# in-memory exec_schemas cached by plugin
	print("[SynergyPlugin][DEBUG] exec_schemas (cached) type=", typeof(exec_schemas))
	if typeof(exec_schemas) == TYPE_DICTIONARY:
		print("[SynergyPlugin][DEBUG] exec_schemas keys=", exec_schemas.keys())

	# Try to show if the C++ ExecutorRegistry class is registered to the scripting side
	var class_info = null
	# ClassDB is available as a global in GDScript
	if ClassDB.class_exists("ExecutorRegistry"):
		print("[SynergyPlugin][DEBUG] ClassDB reports ExecutorRegistry is registered to scripting")
	else:
		print("[SynergyPlugin][DEBUG] ClassDB reports ExecutorRegistry is NOT registered to scripting")

	# Quick hint for next step
	print("[SynergyPlugin][DEBUG] If executor_ids are missing, rebuild the native GDExtension so register_types.cpp can write ProjectSettings; then re-open the editor or press this Debug button again.")


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
	# update current_spec after merging
	current_spec = spec


func _on_debounced_aspect_change() -> void:
	# Called by selection_timer timeout (debounced). Generate proposal + suggest defaults.
	if _suppress_selection_callbacks:
		print('[SynergyPlugin] _on_debounced_aspect_change ignored due to suppression')
		return

	# Ensure any visible selection in the synergy list is cleared when the debounced
	# selection handler runs. Some editor builds route selection changes through the
	# debounce timer instead of the direct item_selected signal, so clear here too.
	if synergy_list:
		print('[SynergyPlugin] _on_debounced_aspect_change: clearing synergy list selection')
		if synergy_list.has_method('get_selected_items'):
			for v in synergy_list.get_selected_items():
				if synergy_list.has_method('deselect'):
					synergy_list.deselect(v)
				elif synergy_list.has_method('set_item_selected'):
					synergy_list.set_item_selected(v, false)
			# fallback
		elif synergy_list.has_method('deselect_all'):
			synergy_list.deselect_all()
		else:
			for i in range(synergy_list.get_item_count()):
				if synergy_list.has_method('set_item_selected'):
					synergy_list.set_item_selected(i, false)

	# Update programmatic spec from the current UI selection so the UI becomes the input to current_spec
	var sel_paths = _get_selected_aspect_paths()
	if sel_paths.size() > 0:
		_set_component_aspects_from_paths(sel_paths)

	_on_generate_proposal()
	_on_suggest_defaults()


func _cancel_selection_debounce() -> void:
	if selection_timer:
		if selection_timer.is_stopped() == false:
			selection_timer.stop()
	# update previous selection snapshot to current so a stray process poll won't re-trigger immediately
	# Prefer the last matched indices collected while programmatically selecting aspects; fall back to UI state
	if _last_matched_indices and _last_matched_indices.size() > 0:
		_prev_aspect_selection = _last_matched_indices.duplicate()
		# clear stored matched indices after snapshotting
		_last_matched_indices = []
		return
	# try to snapshot the current selection indices (works across ItemList versions)
	_prev_aspect_selection = []
	if aspect_list:
		var sel = []
		if aspect_list.has_method('get_selected_items'):
			var packed = aspect_list.get_selected_items()
			for v in packed:
				sel.append(v)
		else:
			for i in range(aspect_list.get_item_count()):
				if aspect_list.is_selected(i):
					sel.append(i)
		_prev_aspect_selection = sel
