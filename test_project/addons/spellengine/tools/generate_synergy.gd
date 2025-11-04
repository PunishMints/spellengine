@tool
# Utility functions for generating Synergy resources from proposals

func save_synergy_from_spec(spec:Dictionary, out_path:String) -> int:
	var s = Synergy.new()
	# set spec via setter to ensure binding
	s.set_spec(spec)
	var err = ResourceSaver.save(s, out_path)
	if err == OK:
		return err
	# fallback: write JSON next to resource
	var json_path = out_path.get_base_dir() + "/" + out_path.get_file().get_basename() + ".json"
	var f = FileAccess.open(json_path, FileAccess.ModeFlags.WRITE)
	if f:
		var j = JSON.new()
		if j.has_method("stringify"):
			f.store_string(j.stringify(spec))
		else:
			f.store_string(str(spec))
		f.close()
	return err
