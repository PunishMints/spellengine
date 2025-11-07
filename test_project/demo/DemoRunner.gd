extends Node

func _ready():
	print("[DemoRunner] Starting demo...")

	# Create a SpellCaster and give it three aspects with 100 mana each: fire, water, wind
	var caster = SpellCaster.new()
	caster.name = "Caster"
	caster.set_assigned_aspects(["fire", "water", "wind"])
	caster.set_mana("fire", 100.0)
	caster.set_mana("water", 100.0)
	caster.set_mana("wind", 100.0)



	# scale fire damage and mana cost by 1.5x from this caster
	add_child(caster)

	# Create one demo targets
	var t1 = preload("res://demo/scripts/DemoTarget.gd").new()
	t1.name = "Target1"
	add_child(t1)


	# Create a damage component (fire) - caster has no fire aspect so the engine will fallback
	var damage_comp = SpellComponent.new()
	damage_comp.set_executor_id("damage_v1")
	damage_comp.set_cost(20.0)
	damage_comp.set_base_params({"amount": 50.0})
	damage_comp.set_aspects_contributions({"water": 1})


	# Create a knockback component that is split between wind and healing
	var knock_comp = SpellComponent.new()
	knock_comp.set_executor_id("knockback_v1")
	knock_comp.set_cost(10.0)
	knock_comp.set_base_params({"force": 400.0, "speed": 500.0, "area": 10})
	knock_comp.set_aspects_contributions({"water": 0.5, "wind": 0.5})


	# Assemble Spell and execute it via SpellEngine
	var spell = Spell.new()
	spell.set_components([damage_comp, knock_comp])

	var ctx = SpellContext.new()
	ctx.set_caster(caster)
	ctx.set_targets([t1])
	# derive aspects from the composed spell and set them into the context
	ctx.derive_and_set_aspects(spell)
	var derived = ctx.derive_aspects_list(spell)
	print("[DemoRunner] Derived aspects for spell: " + str(derived))
	# optionally specify aspects directly instead of deriving: ctx.set_params({"aspects": ["healing","wind"]})

	var engine = SpellEngine.new()
	engine.set_default_merge_mode("mana_cost", 2)
	engine.set_verbose_composition(true)
	engine.execute_spell(spell, ctx)

	# show remaining mana after cast to verify mana_cost scaling
	print("[DemoRunner] Remaining mana - fire: " + str(caster.get_mana("fire")) + ", water: " + str(caster.get_mana("water")) + ", wind: " + str(caster.get_mana("wind")))

	print("[DemoRunner] Demo execution complete")

	# Use a SpellCaster node placed in the scene (preferred for designer workflow),
	# otherwise create one at runtime.
	if has_node("Caster"):
		caster = get_node("Caster")
		print("[DemoRunner] Using scene SpellCaster at", caster.get_path())
	else:
		caster = SpellCaster.new()
		caster.name = "Caster"
		# default aspects if created at runtime
		caster.set_assigned_aspects(["fire", "water", "wind"])
		caster.set_mana("fire", 100.0)
		caster.set_mana("water", 100.0)
		caster.set_mana("wind", 100.0)
		# scale fire damage and mana cost by 1.5x from this caster
		add_child(caster)
