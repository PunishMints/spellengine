extends Node

func approx_equal(a, b, eps=1e-6):
	return abs(a - b) <= eps

func run_tests() -> Dictionary:
	print("[Tests] Test Beginning")
	var results = {"passed": [], "failed": []}
	var engine = SpellEngine.new()

	# create a spell with a single component
	var spell = Spell.new()
	var comp = SpellComponent.new()
	comp.set_cost(10.0)
	comp.set_aspects_contributions({"fire": 1.0})
	var comps = []
	comps.append(comp)
	spell.set_components(comps)

	# create context with a caster that supplies a caster scaler override
	var ctx = SpellContext.new()
	var caster = SpellCaster.new()
	caster.set_scaler("fire", "mana_cost", 1.2)
	ctx.set_caster(caster)

	# set engine to multiply merge mode for mana_cost (MERGE_MULTIPLY == 2)
	engine.set_default_merge_mode("mana_cost", 2)

	# run helper
	var costs = engine.get_adjusted_mana_costs(spell, ctx)
	var total = costs.get("total_mana", 0.0)

	# expected: base cost 10 * (caster_scaler 1.2 * aspect_default 1.3) = 15.6
	var expected = 10.0 * (1.2 * 1.3)
	if approx_equal(total, expected, 1e-3):
		results.passed.append("mana_scaler_applied")
	else:
		results.failed.append({"mana_scaler_applied": {"expected": expected, "got": total}})

	return results
