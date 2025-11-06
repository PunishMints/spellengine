extends Node

func _ready():
	print("Running tests...")
	var results = {"passed": [], "failed": []}

	# run extended suite
	var suite2 = preload("res://tests/test_spell_engine_ext.gd")
	var t2 = suite2.new()
	add_child(t2)
	var state = t2.run_all_tests()
	var r2 = state
	# Avoid referencing the GDScriptFunctionState symbol directly (may not be in scope).
	if typeof(state) == TYPE_OBJECT:
		var cls = ""
		if state.has_method("get_class"):
			cls = state.get_class()
		else:
			cls = state.get_class()
		if cls == "GDScriptFunctionState":
			r2 = state.get_result()
	results.passed += r2.passed
	results.failed += r2.failed

	print("Passed:", results.passed)
	if results.failed.size() > 0:
		print("Failed:")
		for f in results.failed:
			print(f)
		# try to exit with non-zero code for CI if available
	else:
		print("All tests passed")
