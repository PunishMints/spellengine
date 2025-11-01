extends Node

var health := 100.0

func apply_damage(amount, element):
	health -= float(amount)
	print("[DemoTarget] %s took %s %s damage, health now %s" % [name, amount, element, health])

func apply_knockback(force, speed, caster):
	print("[DemoTarget] %s knocked back by force %s speed %s from %s" % [name, force, speed, caster.name if caster else "<none>"])

func apply_heal(amount):
	health += float(amount)
	print("[DemoTarget] %s healed by %s, health now %s" % [name, amount, health])
