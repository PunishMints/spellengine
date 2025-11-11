# Utility helpers for saving/loading Spell -> SpellTemplate resources
# Provides editor-friendly functions to persist composed spells as resources
# and to load SpellTemplate resources and convert them back into runtime Spell objects.

func save_spell_as_template(spell: Spell, path: String, input_action: String = "", input_mode: String = "press_and_confirm", input_options: Dictionary = {}) -> int:
	# spell is expected to be a `Spell` (Ref-counted) instance with components that are SpellComponent Resources
	# path should be a res:// path ending with .tres
	if not spell:
		print("save_spell_as_template: invalid spell")
		return ERR_INVALID_PARAMETER
	# Create a SpellTemplate resource if available
	var tmpl = null
	if ClassDB.class_exists("SpellTemplate"):
		tmpl = SpellTemplate.new()
	else:
		# fallback to plain Resource so the file still saves
		tmpl = Resource.new()

	# copy basic fields
	if tmpl and tmpl.has_method("set_components") and spell.has_method("get_components"):
		tmpl.set_components(spell.get_components())
	if tmpl and tmpl.has_method("set_name") and spell.get("name") != null:
		tmpl.set_name(spell.get("name"))
	# input trigger data
	if tmpl and tmpl.has_method("set_input_action"):
		tmpl.set_input_action(input_action)
	if tmpl and tmpl.has_method("set_input_mode"):
		tmpl.set_input_mode(input_mode)
	if tmpl and tmpl.has_method("set_input_options"):
		tmpl.set_input_options(input_options)
	# If provided, copy an inputs array (list of input steps) from the spell if present
	if tmpl and tmpl.has_method("set_input_steps"):
		# try to read 'input_steps' from the spell object or from an optional property
		var steps = []
		if spell.get and spell.get("input_steps") != null:
			steps = spell.get("input_steps")
		elif spell.has_method("get_input_steps"):
			steps = spell.get_input_steps()
		# if caller provided input_options keyed under "inputs" use that as a convenience
		if input_options and typeof(input_options) == TYPE_DICTIONARY and input_options.has("inputs"):
			steps = input_options["inputs"]
		if steps != null and typeof(steps) == TYPE_ARRAY:
			tmpl.set_input_steps(steps)

	var err = ResourceSaver.save(tmpl, path)
	if err != OK:
		print("save_spell_as_template: ResourceSaver.save returned", err)
	else:
		print("Saved SpellTemplate to:", path)
	return err


func load_spell_from_template(path: String) -> Spell:
	# Loads a SpellTemplate resource and constructs a runtime Spell object with components copied
	if not FileAccess.file_exists(path):
		print("load_spell_from_template: path not found:", path)
		return null
	var r = ResourceLoader.load(path, "", ResourceLoader.CacheMode.CACHE_MODE_IGNORE)
	if not r:
		print("load_spell_from_template: failed to load resource:", path)
		return null
	# If resource exposes get_components, use that
	var sp = null
	if ClassDB.class_exists("Spell"):
		sp = Spell.new()
	else:
		print("load_spell_from_template: Spell class not found")
		return null
	if r.has_method("get_components") and sp.has_method("set_components"):
		sp.set_components(r.get_components())
	# store source path for debugging
	if sp.has_method("set_source_template"):
		sp.set_source_template(path)
	return sp


func read_input_trigger_from_template(path: String) -> Dictionary:
	# Returns a small dict: {action:String, mode:String, options:Dictionary}
	var out = {"action":"", "mode":"", "options":{}}
	var r = ResourceLoader.load(path, "", ResourceLoader.CacheMode.CACHE_MODE_IGNORE)
	if not r:
		return out
	if r.has_method("get_input_action"):
		out["action"] = r.get_input_action()
	if r.has_method("get_input_mode"):
		out["mode"] = r.get_input_mode()
	if r.has_method("get_input_options"):
		out["options"] = r.get_input_options()
	# also expose the full inputs array if present
	if r.has_method("get_input_steps"):
		out["inputs"] = r.get_input_steps()
	return out


func read_input_steps_from_template(path: String) -> Array:
	var r = ResourceLoader.load(path, "", ResourceLoader.CacheMode.CACHE_MODE_IGNORE)
	if not r:
		return []
	if r.has_method("get_input_steps"):
		return r.get_input_steps()
	return []
