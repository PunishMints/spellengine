extends RigidBody3D

# Simple fireball behavior: apply direction as initial velocity and auto-free after lifetime
@export var speed: float = 20.0
@export var lifetime: float = 5.0

var _life_timer := 0.0

func _ready():
	set_physics_process(true)

	# Give the projectile an initial forward velocity so it is visible in the scene.
	# Use the node's -Z axis as forward (Godot convention) so instantiation orientation matters.

func _physics_process(delta: float) -> void:
	_life_timer += delta
	if _life_timer >= lifetime:
		queue_free()

func apply_force_custom(force: Vector3) -> void:
	# Apply a central impulse to the rigid body so physics handles motion.
	# Use apply_impulse with zero offset to approximate a central impulse in Godot 4.
	if has_method("apply_impulse"):
		# apply_impulse(offset: Vector3, impulse: Vector3)
		apply_impulse(Vector3.ZERO, force)
	else:
		# Fallback: set linear_velocity directly
		linear_velocity = force
