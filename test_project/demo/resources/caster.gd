@tool
class_name Caster
extends Resource

# Basic caster data for demo purposes
@export var affinity: String = "neutral"
@export var max_mana: int = 100
@export var mana: int = 100
@export var max_health: int = 100
@export var health: int = 100
@export var params: Dictionary = {}

func use_mana(amount:int) -> bool:
	if amount <= 0:
		return true
	if mana >= amount:
		mana -= amount
		return true
	return false

func recover_mana(amount:int) -> void:
	mana = clamp(mana + amount, 0, max_mana)

func take_damage(amount:int) -> void:
	health = clamp(health - amount, 0, max_health)

func is_alive() -> bool:
	return health > 0
