extends Node

func approx_equal(a, b, eps=1e-6):
	return abs(a - b) <= eps

func _make_spell_with_component(base_cost:float, contribs:Dictionary):
	var spell = Spell.new()
	var comp = SpellComponent.new()
	comp.set_cost(base_cost)
	comp.set_aspects_contributions(contribs)
	var comps = []
	comps.append(comp)
	spell.set_components(comps)
	return spell

# Top-level test helpers so they can be called by Callable(self, "name").bind(...)
func single_aspect_merge_mode(engine:SpellEngine, spell:Spell, base:float, mode:int) -> Dictionary:
	var ctx = SpellContext.new()
	var caster = SpellCaster.new()
	caster.set_scaler("fire", "mana_cost", 1.2)
	ctx.set_caster(caster)
	engine.set_default_merge_mode("mana_cost", mode)
	var costs = engine.get_adjusted_mana_costs(spell, ctx)
	var total = costs.get("total_mana", 0.0)
	# compute expected merged scaler
	var caster_scaler = 1.2
	var aspect_default = 1.3
	var merged = 1.0
	match mode:
		0:
			merged = aspect_default # overwrite
		1:
			merged = caster_scaler + aspect_default
		2:
			merged = caster_scaler * aspect_default
		3:
			merged = min(caster_scaler, aspect_default)
		4:
			merged = max(caster_scaler, aspect_default)
	var expected = base * merged
	if approx_equal(total, expected, 1e-4):
		return {"ok": true}
	else:
		return {"ok": false, "expected": expected, "got": total}

func single_aspect_no_caster(engine:SpellEngine, spell:Spell, base:float) -> Dictionary:
	var ctx = SpellContext.new()
	# no caster
	engine.set_default_merge_mode("mana_cost", 2) # multiply
	var costs = engine.get_adjusted_mana_costs(spell, ctx)
	var total = costs.get("total_mana", 0.0)
	var expected = base * (1.0 * 1.3)
	if approx_equal(total, expected, 1e-4):
		return {"ok": true}
	else:
		return {"ok": false, "expected": expected, "got": total}

func multi_aspect_overwrite(engine:SpellEngine, spell2:Spell, base2:float) -> Dictionary:
	var ctx = SpellContext.new()
	# no caster scalers
	engine.set_default_merge_mode("mana_cost", 0) # overwrite
	var costs = engine.get_adjusted_mana_costs(spell2, ctx)
	var total = costs.get("total_mana", 0.0)
	# expected multiplier = 0.6*1.3 + 0.4*0.8 = 0.78 + 0.32 = 1.1
	var expected = base2 * (0.6*1.3 + 0.4*0.8)
	# also check per-aspect split
	var per = costs.get("per_component", {})
	var comp_costs = {}
	for k in per.keys():
		comp_costs = per[k]
	var fire_share = comp_costs.get("fire", 0.0)
	var wind_share = comp_costs.get("wind", 0.0)
	var expected_fire = total * 0.6
	var expected_wind = total * 0.4
	if approx_equal(total, expected, 1e-4) and approx_equal(fire_share, expected_fire, 1e-4) and approx_equal(wind_share, expected_wind, 1e-4):
		return {"ok": true}
	else:
		return {"ok": false, "expected_total": expected, "got_total": total, "fire": {"expected": expected_fire, "got": fire_share}, "wind": {"expected": expected_wind, "got": wind_share}}

func merge_mode_multiplier_test(engine:SpellEngine) -> Dictionary:
	var ctx = SpellContext.new()
	engine.set_default_merge_mode("mana_cost", 2) # multiply
	engine.set_merge_mode_mana_multiplier(2, 1.5) # multiply mode has 1.5 extra multiplier
	var spell = _make_spell_with_component(10.0, {"fire": 1.0})
	var caster = SpellCaster.new()
	caster.set_scaler("fire", "mana_cost", 1.2)
	ctx.set_caster(caster)
	var costs = engine.get_adjusted_mana_costs(spell, ctx)
	var total = costs.get("total_mana", 0.0)
	var pre = 10.0 * (1.2 * 1.3) # pre-mode multiplier
	var expected = pre * 1.5
	if approx_equal(total, expected, 1e-4):
		return {"ok": true}
	else:
		return {"ok": false, "expected": expected, "got": total}

func no_contribs_uses_first_aspect(engine:SpellEngine) -> Dictionary:
	# if contribs empty and casting_aspects provided, first aspect used
	var spell = Spell.new()
	var comp = SpellComponent.new()
	comp.set_cost(5.0)
	var comps = [comp]
	spell.set_components(comps)
	var ctx = SpellContext.new()
	# specify aspects manually
	var arr = ["fire"]
	var p = {"aspects": arr}
	ctx.set_params(p)
	engine.set_default_merge_mode("mana_cost", 2)
	var costs = engine.get_adjusted_mana_costs(spell, ctx)
	var total = costs.get("total_mana", 0.0)
	var expected = 5.0 * (1.0 * 1.3)
	if approx_equal(total, expected, 1e-4):
		return {"ok": true}
	else:
		return {"ok": false, "expected": expected, "got": total}


# Helper to run a single scenario (top-level to avoid nested lambdas issues)
@warning_ignore("shadowed_variable_base_class")
func run_case(results:Dictionary, name:String, func_to_run:Callable):
	var state = await func_to_run.call()
	var res = state
	# If the call returned a GDScriptFunctionState (async), wait for completion
	if typeof(state) == TYPE_OBJECT:
		var cls = ""
		if state.has_method("get_class"):
			cls = state.get_class()
		else:
			cls = state.get_class()
		if cls == "GDScriptFunctionState":
			await state
			res = state.get_result()

	if typeof(res) == TYPE_DICTIONARY and res.has("ok"):
		if res.ok:
			results.passed.append(name)
		else:
			results.failed.append({name: res})
	else:
		# assume truthy -> pass
		if res:
			results.passed.append(name)
		else:
			results.failed.append({name: res})


func fire_synergy_extra_executor(engine:SpellEngine) -> Dictionary:
	# Async test: executes a damage component which has fire synergy that triggers a DOT extra executor
	var caster = SpellCaster.new()
	caster.set_assigned_aspects(["fire"])
	caster.set_mana("fire", 100.0)
	caster.set_mana("wind", 100.0)

	add_child(caster)

	var t1 = preload("res://demo/DemoTarget.gd").new()
	t1.name = "TestTarget"
	add_child(t1)

	var comp = SpellComponent.new()
	comp.set_executor_id("damage_v1")
	comp.set_cost(5.0)
	comp.set_base_params({"amount": 10.0})
	comp.set_aspects_contributions({"fire": 0.5, "wind": 0.5})

	var spell = Spell.new()
	spell.set_components([comp])

	var ctx = SpellContext.new()
	ctx.set_caster(caster)
	ctx.set_targets([t1])
	ctx.derive_and_set_aspects(spell)

	engine.set_default_merge_mode("mana_cost", 2) # multiply

	var initial_mana = caster.get_mana("fire")
	var initial_health = t1.health

	engine.execute_spell(spell, ctx)

	# wait for DOT to complete by awaiting the target's dot_complete signal
	await t1.dot_complete

	# Print each recorded damage tick for debugging
	var dlog = t1.get_damage_log()
	print("[Test] Damage log for " + t1.name + ":")
	for e in dlog:
		# e contains {"index": int, "amount": float, "aspect": aspect}
		var idx = e.get("index", -1)
		var amt = e.get("amount", 0.0)
		var asp = e.get("aspect", "")
		print("[Test]  tick %s -> amount=%s aspect=%s" % [str(idx), str(amt), str(asp)])

	var final_mana = caster.get_mana("fire")
	var final_health = t1.health

	# compute expected values (consider synergy default_scalers: amount * 1.3, mana_cost * 1.3)
	var aspect_mul = 1.3
	var expected_instant = 10.0 * aspect_mul
	var expected_tick = 2.0 * aspect_mul
	var expected_ticks = 3 # duration 0.15 / 0.05 -> 3 ticks
	var expected_dot_total = expected_tick * expected_ticks
	var expected_total_damage = expected_instant + expected_dot_total

	var expected_main_cost = 5.0 * aspect_mul
	var expected_extra_cost = 1.0
	var expected_total_mana_spent = expected_main_cost + expected_extra_cost

	var damage_taken = initial_health - final_health
	var mana_spent = initial_mana - final_mana

	if approx_equal(damage_taken, expected_total_damage, 1e-3) and approx_equal(mana_spent, expected_total_mana_spent, 1e-3):
		return {"ok": true}
	else:
		return {"ok": false, "expected_damage": expected_total_damage, "got_damage": damage_taken, "expected_mana": expected_total_mana_spent, "got_mana": mana_spent}

func run_all_tests() -> Dictionary:
	var results = {"passed": [], "failed": []}
	# Aspects are auto-loaded by the module at startup from res://aspects

	# 1) Test all merge modes with caster + aspect scalers present (single-aspect)
	var base = 10.0
	var contribs = {"fire": 1.0}
	var spell = _make_spell_with_component(base, contribs)
	for i in range(0,5):
		var engine = SpellEngine.new()
		var cb = Callable(self, "single_aspect_merge_mode").bind(engine, spell, base, i)
		run_case(results, "single_aspect_mode_%d" % i, cb)

	# 2) Test without caster scaler (caster is absent) -> caster_scaler defaults to 1.0
	var engine_no = SpellEngine.new()
	var cb_no = Callable(self, "single_aspect_no_caster").bind(engine_no, spell, base)
	run_case(results, "single_no_caster", cb_no)

	# 3) Multi-aspect component distribution and weighted multiplier
	var base2 = 20.0
	var contribs2 = {"fire": 0.6, "wind": 0.4}
	var spell2 = _make_spell_with_component(base2, contribs2)
	var engine_multi = SpellEngine.new()
	var cb_multi = Callable(self, "multi_aspect_overwrite").bind(engine_multi, spell2, base2)
	run_case(results, "multi_aspect_overwrite", cb_multi)

	# 4) Merge-mode mana multiplier interaction
	var engine_mm = SpellEngine.new()
	var cb_mm = Callable(self, "merge_mode_multiplier_test").bind(engine_mm)
	run_case(results, "merge_mode_multiplier", cb_mm)

	# 5) Edge: zero shares or empty contribs -> expect base behavior
	var engine_nc = SpellEngine.new()
	var cb_nc = Callable(self, "no_contribs_uses_first_aspect").bind(engine_nc)
	run_case(results, "no_contribs_first_aspect", cb_nc)

	# 6) Fire synergy: extra executor (DOT) should be invoked and charge extra mana
	var engine_fire = SpellEngine.new()
	var cb_fire = Callable(self, "fire_synergy_extra_executor").bind(engine_fire)
	run_case(results, "fire_synergy_extra_executor", cb_fire)

	return results
