#@tool must be the first token in the file; move it before extends
@tool

extends CharacterBody3D

# Demo Character Controller for SpellEngine
# - integrates a `Caster` resource for affinity/mana/health
# - basic WASD movement with sprint (Shift) and jump (Space)
# - control executor API: request_control/release_control, plus a placement example

const MOUSE_LEFT = 1
const MOUSE_RIGHT = 2

@export var speed: float = 4.0
@export var sprint_multiplier: float = 1.8
@export var jump_velocity: float = 4.5
@export var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")
@export var caster: Resource

# Camera (third-person) defaults — can be overridden by caster.params (see _get_camera_param)
@export var cam_distance: float = 4.0
@export var cam_height: float = 1.6
@export var cam_sensitivity: Vector2 = Vector2(0.003, 0.003)
@export var cam_smooth_speed: float = 12.0
@export var cam_pitch_min: float = -1.2
@export var cam_pitch_max: float = 0.5
@export var cam_shoulder_offset: Vector2 = Vector2(0.0, 0.0)
@export var cam_invert_x: bool = false
@export var capture_mouse: bool = true

# camera & preview nodes (expected in the scene)
@onready var cam : Camera3D = $Camera3D
@onready var placement_preview : MeshInstance3D = $PlacementPreview
var PauseMenuScene = preload("res://demo/pause_menu.tscn")

# Control executor stack: last-in wins. Each control is a Dictionary with keys:
# {id: String, input(event):Callable?, physics_process(delta):Callable?, confirm:Callable?, cancel:Callable?, meta:Dictionary}
var control_stack : Array = []

# Camera state
var _cam_yaw: float = 0.0
var _cam_pitch: float = -0.15
var _cam_target_yaw: float = 0.0
var _cam_target_pitch: float = -0.15
var _pause_ui: CanvasLayer = null
var _paused: bool = false

func _ready():
	if placement_preview:
		placement_preview.visible = false

	# initialize camera orientation from current transform
	if cam:
		# point the camera roughly behind the character initially
		_cam_yaw = global_transform.basis.get_euler().y
		_cam_target_yaw = _cam_yaw
		_cam_pitch = -0.15
		_cam_target_pitch = _cam_pitch

	# Ensure mouse capture state is correct on start
	_update_mouse_capture()

	# Ensure input actions exist so Escape toggles pause
	_ensure_input_actions()

	# process mode is configured in the project/engine settings; no runtime changes here


func _physics_process(delta:float) -> void:
	# Do not run physics in the editor (prevents the character falling while editing)
	if Engine.is_editor_hint():
		return

	# When engine is paused, stop gameplay physics but keep this node processing for pause UI/input
	if get_tree().paused:
		return

	# If a control executor is active, delegate physics to it
	if control_stack.size() > 0:
		var top = control_stack[control_stack.size()-1]
		if top.has("physics_process") and typeof(top["physics_process"]) == TYPE_CALLABLE:
			top["physics_process"].call(delta)
		return

	# Normal character movement
	handle_movement(delta)

	# physics-level camera collision/updates could go here if needed

func handle_movement(delta:float) -> void:
	var input_dir = Vector3.ZERO
	# prefer explicit actions but fallback to ui_* for flexibility
	var forward = 0.0
	var strafe = 0.0
	if InputMap.has_action("move_forward") or InputMap.has_action("move_back") or InputMap.has_action("move_right") or InputMap.has_action("move_left"):
		forward = (Input.get_action_strength("move_forward") if InputMap.has_action("move_forward") else 0.0) - (Input.get_action_strength("move_back") if InputMap.has_action("move_back") else 0.0)
		strafe = (Input.get_action_strength("move_right") if InputMap.has_action("move_right") else 0.0) - (Input.get_action_strength("move_left") if InputMap.has_action("move_left") else 0.0)
	else:
		forward = Input.get_action_strength("ui_up") - Input.get_action_strength("ui_down")
		strafe = Input.get_action_strength("ui_right") - Input.get_action_strength("ui_left")

	# Build direction relative to camera yaw
	var cam_basis = cam.global_transform.basis
	var forward_dir = -cam_basis.z.normalized()
	var right_dir = cam_basis.x.normalized()
	input_dir += forward_dir * forward
	input_dir += right_dir * strafe
	if input_dir.length() > 1:
		input_dir = input_dir.normalized()

	var move_speed = speed
	var is_sprinting = InputMap.has_action("sprint") and Input.is_action_pressed("sprint")
	if is_sprinting:
		move_speed *= sprint_multiplier

	velocity.x = input_dir.x * move_speed
	velocity.z = input_dir.z * move_speed

	# gravity/jump — derive per-character gravity from caster.params when present
	var gravity_dir := Vector3.DOWN
	var gravity_strength := gravity
	if caster and caster is Resource and caster.has("params"):
		var gvec = caster.params.get("gravity_vector", null)
		if typeof(gvec) == TYPE_VECTOR3:
			gravity_dir = gvec.normalized()
		var gstr = caster.params.get("gravity_strength", null)
		if typeof(gstr) in [TYPE_INT, TYPE_FLOAT]:
			gravity_strength = float(gstr)

	# apply gravity vector
	if not is_on_floor():
		velocity += gravity_dir * gravity_strength * delta

	# jump applies velocity along the opposite of gravity direction
	var did_jump = InputMap.has_action("jump") and Input.is_action_just_pressed("jump")
	if did_jump:
		var jump_dir = -gravity_dir.normalized()
		# replace velocity component along jump_dir with jump speed
		velocity = velocity - velocity.project(jump_dir) + jump_dir * jump_velocity

	move_and_slide()

# Input routing: if a control executor is active, forward events to it
func _unhandled_input(event) -> void:
	if control_stack.size() > 0:
		var top = control_stack[control_stack.size()-1]
		if top.has("input") and typeof(top["input"]) == TYPE_CALLABLE:
			top["input"].call(event)
			# consume the event so the rest of the game doesn't handle it
			get_viewport().set_input_as_handled()
		return

	# Camera look handling when no control executor active
	if event is InputEventMouseMotion:
		# read sensitivity (caster may override)
		var sens = _get_camera_param("camera_sensitivity", cam_sensitivity)
		var rel = event.relative
		# horizontal mouse: allow inversion and caster override
		var x_sign = 1.0 if bool(_get_camera_param("camera_invert_x", cam_invert_x)) else -1.0
		var y_sign = 1.0 if bool(_get_camera_param("camera_invert_y", cam_invert_x)) else -1.0
		_cam_target_yaw += rel.x * sens.x * x_sign
		_cam_target_pitch -= rel.y * sens.y * y_sign
		_cam_target_pitch = clamp(_cam_target_pitch, _get_camera_param("camera_pitch_min", cam_pitch_min), _get_camera_param("camera_pitch_max", cam_pitch_max))
		# consume so the editor/game doesn't also use it
		get_viewport().set_input_as_handled()


# Public API for control executors
func request_control(control_id:String, handlers:Dictionary) -> bool:
	# handlers: input(event) callable, physics_process(delta) callable, confirm() callable, cancel() callable
	for c in control_stack:
		if c["id"] == control_id:
			return false # already present
	var entry = {
		"id": control_id,
		"input": handlers.get("input", null),
		"physics_process": handlers.get("physics_process", null),
		"confirm": handlers.get("confirm", null),
		"cancel": handlers.get("cancel", null),
		"meta": handlers.get("meta", {})
	}
	control_stack.append(entry)
	# show preview if provided
	if entry["meta"].has("preview_node") and entry["meta"]["preview_node"]:
		entry["meta"]["preview_node"].visible = true
	return true

func release_control(control_id:String) -> void:
	for i in range(control_stack.size()-1, -1, -1):
		if control_stack[i]["id"] == control_id:
			# hide preview if present
			if control_stack[i]["meta"].has("preview_node") and control_stack[i]["meta"]["preview_node"]:
				control_stack[i]["meta"]["preview_node"].visible = false
			control_stack.remove_at(i)
			return

# Convenience: request a simple placement control that lets the player pick a point on the ground
func request_placement(control_id:String, max_distance:float, confirm_cb:Callable, cancel_cb=null) -> bool:
	if not cam:
		return false
	# ensure preview exists
	if placement_preview:
		placement_preview.visible = true

	var handlers = {}

	handlers["physics_process"] = func(_delta:float) -> void:
		_update_placement_preview(max_distance)

	handlers["input"] = func(event) -> void:
		if event is InputEventMouseButton:
			var mb = event as InputEventMouseButton
			if mb.pressed and mb.button_index == MOUSE_LEFT:
				# confirm
				if confirm_cb:
					confirm_cb.call(_get_current_placement())
				release_control(control_id)
			elif mb.pressed and mb.button_index == MOUSE_RIGHT:
				# cancel
				if cancel_cb:
					cancel_cb.call()
				release_control(control_id)

	handlers["confirm"] = func() -> void:
		if confirm_cb:
			confirm_cb.call(_get_current_placement())
		release_control(control_id)

	handlers["cancel"] = func() -> void:
		if cancel_cb:
			cancel_cb.call()
		release_control(control_id)

	handlers["meta"] = {"preview_node": placement_preview}

	return request_control(control_id, handlers)

func _update_placement_preview(max_distance:float) -> void:
	if not cam or not placement_preview:
		return
	var from = cam.project_ray_origin(get_viewport().get_mouse_position())
	var dir = cam.project_ray_normal(get_viewport().get_mouse_position())
	# intersect against y=0 plane (ground) when possible
	if abs(dir.y) > 0.0001:
		var t = -from.y / dir.y
		if t > 0 and t < max_distance:
			var p = from + dir * t
			placement_preview.global_transform.origin = p
			return
	# fallback: point in front of camera at max_distance
	placement_preview.global_transform.origin = from + dir * max_distance

func _get_current_placement() -> Vector3:
	if placement_preview:
		return placement_preview.global_transform.origin
	return Vector3.ZERO


func _get_camera_param(key:String, fallback):
	# Helper: read camera-related params from caster.params if present, otherwise fallback
	if caster and caster is Resource and caster.has("params"):
		if caster.params.has(key):
			return caster.params.get(key)
	return fallback


func _process(delta:float) -> void:
	# Update camera target & smoothing every frame (runs in editor because script is @tool)
	if not cam:
		return

	# keep mouse capture in sync with focus/pause state
	_update_mouse_capture()

	# Pause toggle via InputMap action 'pause' (mapped to Escape in your project). Only when running (not in editor).
	if not Engine.is_editor_hint():
		if InputMap.has_action("pause") and Input.is_action_just_pressed("pause"):
			var new_paused = not get_tree().paused
			_set_paused(new_paused)

	# Derive effective parameters (caster can override)
	var distance: float = float(_get_camera_param("camera_distance", cam_distance))
	var height: float = float(_get_camera_param("camera_height", cam_height))
	var smooth: float = float(_get_camera_param("camera_smooth_speed", cam_smooth_speed))
	var shoulder: Vector2 = _get_camera_param("camera_shoulder_offset", cam_shoulder_offset)

	# Smooth yaw/pitch
	_cam_yaw = lerp_angle(_cam_yaw, _cam_target_yaw, clamp(delta * smooth, 0.0, 1.0))
	_cam_pitch = lerp(_cam_pitch, _cam_target_pitch, clamp(delta * smooth, 0.0, 1.0))

	# Build rotation quaternion from yaw then pitch
	var qyaw = Quaternion(Vector3.UP, _cam_yaw)
	var qpitch = Quaternion(Vector3.RIGHT, _cam_pitch)
	var q = qyaw * qpitch

	# Compute desired camera world position (behind the character)
	var target_origin = global_transform.origin + Vector3(0, height, 0)
	# rotate offsets using a Basis built from the quaternion
	var b = Basis(q)
	var cam_offset = b * Vector3(0, 0, -distance)
	# apply optional shoulder offset in camera local X/Y
	var shoulder_offset_world = b * Vector3(shoulder.x, shoulder.y, 0)
	var desired_cam_pos = target_origin + cam_offset + shoulder_offset_world

	# Smooth camera movement
	var cur = cam.global_transform
	cur.origin = cur.origin.lerp(desired_cam_pos, clamp(delta * smooth, 0.0, 1.0))
	cam.global_transform = cur

	# Look at the target origin
	cam.look_at(target_origin, Vector3.UP)


func _update_mouse_capture() -> void:
	# Manage mouse visibility and capture: capture when not paused and capture_mouse is enabled
	if Engine.is_editor_hint():
		# don't change mouse mode while in the editor
		return

	var desired_mode = Input.MOUSE_MODE_VISIBLE
	# Use SceneTree paused state (ignore window focus per request)
	if capture_mouse and not get_tree().paused:
		desired_mode = Input.MOUSE_MODE_CAPTURED

	if Input.get_mouse_mode() != desired_mode:
		Input.set_mouse_mode(desired_mode)


func _ensure_pause_ui() -> void:
	if _pause_ui:
		return
	# Instance the reusable Pause menu scene from res://demo/pause_menu.tscn
	var root = get_tree().current_scene if get_tree().current_scene else get_tree().get_root()
	var menu = PauseMenuScene.instantiate()
	if not menu:
		return
	root.add_child(menu)
	menu.visible = false
	print("[Controller] Pause UI instantiated at", menu.get_path())
	# print process/pause modes for debugging
	print("[Controller] menu.process_mode=", menu.process_mode)

	# connect signals so the controller handles resume/quit
	# Prefer connecting the actual Button nodes directly (more robust than relying on a scene script)
	var resume_btn = null
	var quit_btn = null
	if menu.has_node("Panel/VBoxContainer/ResumeButton"):
		resume_btn = menu.get_node("Panel/VBoxContainer/ResumeButton")
	if menu.has_node("Panel/VBoxContainer/QuitButton"):
		quit_btn = menu.get_node("Panel/VBoxContainer/QuitButton")
	print("[Controller] pause menu buttons found resume=", resume_btn != null, " quit=", quit_btn != null)
	if resume_btn:
		var ok_r = resume_btn.connect("pressed", Callable(self, "_on_resume_pressed"))
		print("[Controller] connect resume_btn.pressed ->", ok_r)
		print("[Controller] resume connected=", resume_btn.is_connected("pressed", Callable(self, "_on_resume_pressed")))
	if quit_btn:
		var ok_q = quit_btn.connect("pressed", Callable(self, "_on_quit_pressed"))
		print("[Controller] connect quit_btn.pressed ->", ok_q)
		print("[Controller] quit connected=", quit_btn.is_connected("pressed", Callable(self, "_on_quit_pressed")))

	_pause_ui = menu


func _ensure_input_actions() -> void:
	# Ensure the 'pause' InputMap action exists and map it to Escape so users can toggle pause
	if not InputMap.has_action("pause"):
		InputMap.add_action("pause")


func _on_pause_changed(new_paused: bool) -> void:
	# ensure the pause UI scene exists and show/hide it
	_ensure_pause_ui()
	if _pause_ui:
		# call the scene's API if present, then toggle visibility
		if _pause_ui.has_method("show_menu"):
			_pause_ui.call("show_menu", new_paused)
		_pause_ui.visible = new_paused


func _on_resume_pressed() -> void:
	print("[Controller] _on_resume_pressed called")
	_set_paused(false)


func _on_quit_pressed() -> void:
	print("[Controller] _on_quit_pressed called")
	get_tree().quit()


func _set_paused(value: bool) -> void:
	# Set engine pause and keep local mirror in sync.
	print("[Controller] _set_paused ->", value)
	get_tree().paused = value
	_paused = value
	_on_pause_changed(_paused)
	_update_mouse_capture()

# Small helper to cancel top control (e.g., ESC key can call this)
func cancel_top_control() -> void:
	if control_stack.size() == 0:
		return
	var top = control_stack[control_stack.size()-1]
	if top.has("cancel") and typeof(top["cancel"]) == TYPE_CALLABLE:
		top["cancel"].call()
	release_control(top["id"])
