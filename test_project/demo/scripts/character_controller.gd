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
@onready var caster = $Caster
@onready var cam : Camera3D = $Camera3D
@onready var placement_preview : MeshInstance3D = $PlacementPreview
var PauseMenuScene = preload("res://demo/scenes/ui/pause_menu.tscn")
var _registered_spell_actions : Array = []

var engine = SpellEngine.new()

# Control executor stack: last-in wins. Each control is a Dictionary with keys:
# {id: String, input(event):Callable?, physics_process(delta):Callable?, confirm:Callable?, cancel:Callable?, meta:Dictionary}
var control_stack : Array = []

# Debug helpers to avoid spamming logs every frame
var _last_registered_count: int = -1
var _native_controls_active_printed: bool = false

# Camera state
var _cam_yaw: float = 0.0
var _cam_pitch: float = -0.15
var _cam_target_yaw: float = 0.0
var _cam_target_pitch: float = -0.15
var _pause_ui: CanvasLayer = null
var _paused: bool = false
var _ui_root: Node = null
var _mana_template: Node = null
var _mana_entries: Dictionary = {}
var _health_bar: Node = null
var _dead: bool = false

func _ready():
	print("[Controller][_ready] character_controller ready on node:", get_path())
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

	# wire up demo UI if present
	_setup_ui()

	# Try to auto-wire caster if not already set (allow controller to work even without DemoUi)
	if not caster or not is_instance_valid(caster):
		var root = get_tree().current_scene if get_tree().current_scene else get_tree().get_root()
		if root and root.has_node("Caster"):
			caster = root.get_node("Caster")
			print("[Controller][_ready] auto-wired caster from scene root:", caster.get_path())
		else:
			var found_c = _find_caster_in_node(root)
			if found_c:
				caster = found_c
				print("[Controller][_ready] auto-wired caster by search:", caster.get_path())

	# SpellSlot registration is handled by _setup_ui() (it runs registration and a deferred retry).


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
	if caster and caster is Resource and caster.get("params") != null:
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
	# Do not handle raw left/right mouse button presses here - use InputMap actions instead.

	if control_stack.size() > 0:
		# If a control is active, forward non-button events (motion, keys, etc.) to it.
		# Left/Right mouse button presses are handled via InputMap polling in _process.
		if event is InputEventMouseButton:
			return

		var top = control_stack[control_stack.size()-1]
		if top.has("input") and typeof(top["input"]) == TYPE_CALLABLE:
			var cid = top.has("id") if str(top["id"]) else "<unknown>"
			print("[Controller][_unhandled_input] forwarding event to control=", cid)
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
	# handlers may contain these keys: "input", "physics_process", "confirm", "cancel", "meta"
	# Prevent duplicate control IDs
	for c in control_stack:
		if c.has("id") and c["id"] == control_id:
			return false

	var entry = {
		"id": control_id,
		"input": handlers.get("input", null),
		"physics_process": handlers.get("physics_process", null),
		"confirm": handlers.get("confirm", null),
		"cancel": handlers.get("cancel", null),
		"meta": handlers.get("meta", null),
	}

	control_stack.append(entry)
	print("[Controller][request_control] PUSH id=", control_id, " new_stack_size=", control_stack.size())

	# If a preview node was supplied in meta, wire it up (and make it visible)
	if entry["meta"] and typeof(entry["meta"]) == TYPE_DICTIONARY:
		if entry["meta"].has("preview_node") and entry["meta"]["preview_node"]:
			placement_preview = entry["meta"]["preview_node"]
			placement_preview.visible = true

	return true

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
	if caster and caster is Resource:
		var p = caster.get("params")
		if typeof(p) == TYPE_DICTIONARY and p.has(key):
			return p.get(key)
	return fallback


func _process(delta:float) -> void:
	# Update camera target & smoothing every frame (runs in editor because script is @tool)
	if not cam:
		return

	# keep mouse capture in sync with focus/pause state
	_update_mouse_capture()

	# Also log select/cancel actions even when no control is active so we can verify InputMap is firing
	if control_stack.size() == 0:
		if has_meta("spellengine_controls_active") and get_meta("spellengine_controls_active"):
			# If spellengine controls are active (native C++ gizmos), skip handling select here
			# Let the native control system handle input while active
			# Print this only once when the state becomes active to avoid spamming logs
			if not _native_controls_active_printed:
				print("[Controller][_process] native controls active; skipping select handling")
				_native_controls_active_printed = true
		else:
			# ensure we reset the one-shot flag when native controls are not active
			_native_controls_active_printed = false
			if InputMap.has_action("select") and Input.is_action_just_pressed("select"):
				print("[Controller][_process] select action pressed (no active control)")
			# Debug: dump registered spell actions only when the count changes to reduce spam
			var current_count = _registered_spell_actions.size()
			if current_count != _last_registered_count:
				print("[Controller][_process] registered_spell_actions_count:", current_count)
				for r in _registered_spell_actions:
					if typeof(r) == TYPE_DICTIONARY and r.has("action"):
						print("[Controller][_process] registered action:", r["action"], "slot:", r.has("slot_node") and r["slot_node"] or "<no-slot>")
				_last_registered_count = current_count
			if InputMap.has_action("cancel") and Input.is_action_just_pressed("cancel"):
				print("[Controller][_process] cancel action pressed (no active control)")

			# Check registered spell hotkeys (simple key_press triggers)
			for reg in _registered_spell_actions:
				if typeof(reg) == TYPE_DICTIONARY and reg.has("action"):
					var an = reg["action"]
					if InputMap.has_action(an) and Input.is_action_just_pressed(an):
						print("[Controller] spell action pressed:", an)
						_start_spell_from_template(reg["spell"], reg["control"])

	if control_stack.size() > 0:
		var top = control_stack[control_stack.size()-1]
		# Confirm (select)
		if InputMap.has_action("select") and Input.is_action_just_pressed("select"):
			print("[Controller][_process] Input action 'select' pressed -> confirming control id=", top.has("id") if str(top["id"]) else "<unknown>")
			if top.has("confirm") and typeof(top["confirm"]) == TYPE_CALLABLE:
				top["confirm"].call()
			elif top.has("input") and typeof(top["input"]) == TYPE_CALLABLE:
				# fallback: synthesize a left-click event for input handler
				var ev = InputEventMouseButton.new()
				ev.button_index = MOUSE_LEFT
				ev.pressed = true
				top["input"].call(ev)
			release_control(top.has("id") if str(top["id"]) else "")

		# Cancel
		if InputMap.has_action("cancel") and Input.is_action_just_pressed("cancel"):
			print("[Controller][_process] Input action 'cancel' pressed -> cancelling control id=", top.has("id") if str(top["id"]) else "<unknown>")
			if top.has("cancel") and typeof(top["cancel"]) == TYPE_CALLABLE:
				top["cancel"].call()
			elif top.has("input") and typeof(top["input"]) == TYPE_CALLABLE:
				var ev2 = InputEventMouseButton.new()
				ev2.button_index = MOUSE_RIGHT
				ev2.pressed = true
				top["input"].call(ev2)
			release_control(top.has("id") if str(top["id"]) else "")

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

	# Update UI (health/mana) each frame if we have a caster and UI
	_update_ui_from_caster()
	# Update UI (health/mana) each frame if we have a caster and UI
	_update_ui_from_caster()


func _update_mouse_capture() -> void:
	# Manage mouse visibility and capture: capture when not paused and capture_mouse is enabled
	if Engine.is_editor_hint():
		# don't change mouse mode while in the editor
		return

	var desired_mode = Input.MOUSE_MODE_VISIBLE
	# If native controls are active (ControlInputController set a meta on this node), keep cursor visible
	if has_meta("spellengine_controls_active") and get_meta("spellengine_controls_active"):
		desired_mode = Input.MOUSE_MODE_VISIBLE
	else:
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
	# debug: list menu children
	var ch = []
	for c in menu.get_children():
		ch.append(c.get_class() + ":" + str(c.name))

	# connect signals so the controller handles resume/quit
	# Prefer connecting the actual Button nodes directly (more robust than relying on a scene script)
	var resume_btn = null
	var quit_btn = null
	if menu.has_node("Panel/VBoxContainer/ResumeButton"):
		resume_btn = menu.get_node("Panel/VBoxContainer/ResumeButton")
	if menu.has_node("Panel/VBoxContainer/QuitButton"):
		quit_btn = menu.get_node("Panel/VBoxContainer/QuitButton")
	if resume_btn:
		var _ok_r = resume_btn.connect("pressed", Callable(self, "_on_resume_pressed"))
		# try to give it focus so it receives input while paused
		resume_btn.grab_focus()
	if quit_btn:
		var _ok_q = quit_btn.connect("pressed", Callable(self, "_on_quit_pressed"))

	_pause_ui = menu


func _ensure_input_actions() -> void:
	# Ensure the 'pause' InputMap action exists and map it to Escape so users can toggle pause
	if not InputMap.has_action("pause"):
		InputMap.add_action("pause")
	# Ensure select/cancel actions exist for input mappings (left/right mouse by default)
	if not InputMap.has_action("select"):
		InputMap.add_action("select")
		var ev_sel = InputEventMouseButton.new()
		ev_sel.button_index = MOUSE_LEFT
		InputMap.action_add_event("select", ev_sel)
	if not InputMap.has_action("cancel"):
		InputMap.add_action("cancel")
		var ev_can = InputEventMouseButton.new()
		ev_can.button_index = MOUSE_RIGHT
		InputMap.action_add_event("cancel", ev_can)

	# Also ensure demo UI update actions exist (non-essential)
	if not InputMap.has_action("ui_debug"):
		InputMap.add_action("ui_debug")



func _register_caster_spell_slots() -> void:
	# Register SpellSlot children attached to the caster and map their input_action to InputMap actions.
	# This function now only supports designer-specified InputMap action names stored on the SpellSlot node.
	if not caster:
		return

	# clear prior registrations to avoid duplicates when called multiple times
	_registered_spell_actions.clear()

	var children = caster.get_children()
	print("[Controller][_register_caster_spell_slots] caster has child_count=", children.size())
	for child in children:
		var cname = "<unknown>"
		if typeof(child) == TYPE_OBJECT:
			cname = child.get_class()
		print("[Controller][_register_caster_spell_slots] child:", child.name, "class:", cname, "type:", typeof(child))
		# If this child has a script attached that defines SpellSlot, try to read its properties
		var template_val = null
		var input_val = null
		if typeof(child) == TYPE_OBJECT:
			# use get() which returns null if the property doesn't exist or is unset
			template_val = child.get("template")
			input_val = child.get("input_action")
		print("[Controller][_register_caster_spell_slots] -> template:", template_val, "input_action:", input_val)

		# Accept nodes that are explicitly SpellSlot or that expose the expected properties
		if typeof(child) == TYPE_OBJECT and (child.get_class() == "SpellSlot" or template_val != null or input_val != null):
			var tmpl = template_val
			if tmpl == null:
				print("[Controller][_register_caster_spell_slots] slot has no template, skipping:", child.name)
				continue

			var action_name = ""
			if input_val != null:
				action_name = str(input_val)

			print("[Controller][_register_caster_spell_slots] found SpellSlot:", child.name, "action:", action_name)

			if action_name == "":
				# no action assigned to this slot; skip registration
				print("[Controller][_register_caster_spell_slots] slot has empty input_action, skipping:", child.name)
				continue

			# ensure an InputMap action placeholder exists so designers can bind it in project settings
			if not InputMap.has_action(action_name):
				InputMap.add_action(action_name)
				print("[Controller] created action placeholder for slot:", action_name)

			_registered_spell_actions.append({"action": action_name, "spell": tmpl, "control": null, "slot_node": child})
			print("[Controller] registered caster slot action:", action_name, "for slot", child.name)




func _find_caster_in_node(n:Node) -> Node:
	# Recursively search for a node that exposes get_assigned_aspects (SpellCaster)
	for child in n.get_children():
		if typeof(child) == TYPE_OBJECT and child.has_method("get_assigned_aspects"):
			return child
		var found = _find_caster_in_node(child)
		if found:
			return found
	return null


func _setup_ui() -> void:
	# Attempt to find a DemoUi node in the current scene
	var root = get_tree().current_scene if get_tree().current_scene else get_tree().get_root()
	if root and root.has_node("DemoUi"):
		_ui_root = root.get_node("DemoUi")
		print("[Controller] found DemoUi at", _ui_root.get_path())
		# attempt to auto-wire caster from the scene if not explicitly set on this controller
		if not caster:
			# prefer a node literally named 'Caster'
			if root.has_node("Caster"):
				caster = root.get_node("Caster")
				print("[Controller] auto-wired caster from scene node:", caster.get_path())
			else:
				# recursive search for a node that exposes get_assigned_aspects (SpellCaster)
				var found_c = _find_caster_in_node(root)
				if found_c:
					caster = found_c
					print("[Controller] auto-wired caster by search:", caster.get_path())
		# Find ManaContainer and a template ManaPool inside it
		if _ui_root.has_node("MainBar/ManaContainer/AspectMana"):
			_mana_template = _ui_root.get_node("MainBar/ManaContainer/AspectMana")
			# keep template hidden (we'll duplicate it)
			_mana_template.visible = false
			print("[Controller] Mana template found")
		# find health bar if present
		if _ui_root.has_node("MainBar/StatContainer/HealthBar/HealthBG/Health"):
			_health_bar = _ui_root.get_node("MainBar/StatContainer/HealthBar/HealthBG/Health")
			print("[Controller] health bar found", _health_bar.get_path())
		# initialize mana entries based on caster aspects immediately
		_populate_mana_entries()
		# Also attempt a deferred registration in case child scripts haven't finished initialising yet
		call_deferred("_register_caster_spell_slots")
	else:
		print("[Controller] DemoUi not found in current scene")


func _populate_mana_entries() -> void:
	# Clear existing entries (except template)
	_mana_entries.clear()
	if not _ui_root or not _mana_template:
		printerr("[Controller] _populate_mana_entries: UI root or mana template missing")
		return
	# The demo UI places the AspectMana template under MainBar/ManaContainer
	var container = _ui_root.get_node("MainBar/ManaContainer") if _ui_root.has_node("MainBar/ManaContainer") else null
	if not container:
		printerr("[Controller] _populate_mana_entries: MainBar/ManaContainer not found")
		return
	# Require a scene Caster node with the canonical API
	if not caster:
		printerr("[Controller] _populate_mana_entries: caster is not assigned on the controller; expected @onready var caster = $Caster")
		return
	if typeof(caster) != TYPE_OBJECT or not caster.has_method("get_assigned_aspects"):
		printerr("[Controller] _populate_mana_entries: caster does not expose get_assigned_aspects(); check your Caster node implementation")
		return
	var aspects = caster.get_assigned_aspects()
	if typeof(aspects) != TYPE_ARRAY or aspects.size() == 0:
		printerr("[Controller] _populate_mana_entries: caster.get_assigned_aspects() returned empty or non-array:", aspects)
		return
	# Build UI strictly from the returned aspects
	for a in aspects:
		var node = _mana_template.duplicate()
		container.add_child(node)
		node.visible = true
		# Attempt to load corresponding Aspect resource to fetch color
		var aspect_res_path = "res://aspects/%s.tres" % str(a)
		var asp_res = null
		if ResourceLoader.exists(aspect_res_path):
			asp_res = ResourceLoader.load(aspect_res_path)
		if asp_res:
			var col = null
			# Prefer the bound getter if present on the Aspect resource
			if typeof(asp_res) == TYPE_OBJECT and asp_res.has_method("get_color"):
				col = asp_res.get_color()
			# Fallback: try reading the property directly from the resource (works if TRES contains 'color' even before rebuild)
			if col == null:
				# Object.get() returns null if missing
				col = asp_res.get("color") if typeof(asp_res) == TYPE_OBJECT else null
			if col != null:
				# build a simple two-stop gradient (darker -> color)
				var g = Gradient.new()
				var dark = Color(col.r * 0.6, col.g * 0.6, col.b * 0.6, col.a)
				g.add_point(0.0, dark)
				g.add_point(1.0, col)
				var gt = GradientTexture1D.new()
				gt.gradient = g
				gt.width = 128
				# Debug: show incoming event info
				# assign to the Polygon2D child named 'ManaPool' inside the Aspect template
				if node.has_node("ManaPool"):
					var pool = node.get_node("ManaPool")
					# Polygon2D uses 'texture' property
					pool.texture = gt
				else:
					printerr("[Controller] Aspect template missing 'ManaPool' child:", node)
			else:
				printerr("[Controller] Aspect resource missing get_color() or returned null:", aspect_res_path)
		else:
			printerr("[Controller] Aspect resource not found:", aspect_res_path)
		# find the two labels inside the template: ManaLabels/Aspect and ManaLabels/Mana
		var lbl_aspect = null
		var lbl_mana = null
		if node.has_node("ManaLabels/Aspect"):
			lbl_aspect = node.get_node("ManaLabels/Aspect")
		if node.has_node("ManaLabels/Mana"):
			lbl_mana = node.get_node("ManaLabels/Mana")
		# Determine display name: prefer Aspect resource's name if available
		var disp = str(a)
		if asp_res and typeof(asp_res) == TYPE_OBJECT and asp_res.has_method("get_name"):
			disp = asp_res.get_name()
		elif disp.length() > 0:
			disp = disp.capitalize()
		if lbl_aspect:
			lbl_aspect.text = disp
		if lbl_mana:
			lbl_mana.text = "0"
		# Store display name and references for later UI updates
		print("[Controller] Making Mana Pool: ", disp)
		_mana_entries[str(a)] = {"node": node, "label_aspect": lbl_aspect, "label_mana": lbl_mana, "display": disp, "aspect_res": asp_res}
	print("[Controller] populated mana entries:", _mana_entries.keys())


func _update_ui_from_caster() -> void:
	if not caster or not _ui_root:
		return
	# Health
	var cur_health = null
	var max_health = null
	if typeof(caster) == TYPE_OBJECT and caster.has_method("is_alive"):
		# native SpellCaster or similar
		if caster.has_method("get_health"):
			cur_health = caster.get_health()
		if caster.has_method("get_max_health"):
			max_health = caster.get_max_health()
		# fallback to properties
		if cur_health == null and caster.get("health") != null:
			cur_health = caster.health
		if max_health == null and caster.get("max_health") != null:
			max_health = caster.max_health
	elif caster is Resource:
		if caster.get("health") != null:
			cur_health = caster.health
		if caster.get("max_health") != null:
			max_health = caster.max_health

	if cur_health != null and max_health != null and _health_bar:
		var frac = 0.0
		if max_health > 0:
			frac = float(cur_health) / float(max_health)
			# HealthBar is a Sprite2D scaled horizontally, adjust its scale.x
			var s = _health_bar.scale
			s.x = clamp(frac, 0.0, 1.0)
			_health_bar.scale = s
		# death handling
		if cur_health <= 0 and not _dead:
			_on_death()

	# Mana per aspect
	for a in _mana_entries.keys():
		# Canonical flow: call caster.get_mana(aspect) on the scene Caster node
		var val = 0
		if typeof(caster) != TYPE_OBJECT or not caster.has_method("get_mana"):
			printerr("[Controller] _update_ui_from_caster: caster missing get_mana(aspect) method; cannot read per-aspect mana")
		else:
			val = caster.get_mana(str(a))
		# update labels: show friendly display name and current mana
		var info = _mana_entries[a]
		if not info:
			continue
		var display = a
		if info.has("display") and info["display"]:
			display = info["display"]
		if info.has("label_aspect") and info["label_aspect"]:
			info["label_aspect"].text = str(display)
		if info.has("label_mana") and info["label_mana"]:
			info["label_mana"].text = str(val)


func _on_death() -> void:
	_dead = true
	print("[Controller] Character has died")
	# Stop movement and controls
	velocity = Vector3.ZERO
	control_stack.clear()
	# Optionally show a death message on UI if present
	if _ui_root and _ui_root.has_node("StatContainer/CanvasGroup"):
		var cg = _ui_root.get_node("StatContainer/CanvasGroup")
		if cg.has_node("DeathLabel"):
			cg.get_node("DeathLabel").visible = true
	# release mouse
	_update_mouse_capture()


func _start_spell_from_template(spell_res, _control_desc) -> void:
	if not spell_res:
		return
	print("[Controller][_start_spell_from_template] starting spell from template:", spell_res.get_name())
	# Build runtime Spell from template's components
	var sp = Spell.new()
	if spell_res.has_method("get_components"):
		sp.set_components(spell_res.get_components())
	# create context
	var ctx = SpellContext.new()
	ctx.set_caster(caster)
	ctx.set_targets([])
	ctx.derive_and_set_aspects(sp)
	# If the control descriptor indicates we need to start a frontend control (e.g., choose_vector),
	# prefer calling resolve_controls so the engine will prompt for controls before executing remaining components.
	if engine and engine.has_method("resolve_controls"):
		print("[Controller] calling engine.resolve_controls() to run controls before execution")
		# pass self as parent so any ControlManager-created gizmos can be parented under the controller
		engine.resolve_controls(sp, ctx, self, Callable(self, "_on_control_resolved"))
	elif engine and engine.has_method("execute_spell"):
		print("[Controller] engine lacks resolve_controls; falling back to engine.execute_spell()")
		engine.execute_spell(sp, ctx)
	else:
		print("[Controller] engine not available; fallback - executing components locally where possible")

func _on_control_resolved(out) -> void:
	# Called when engine.resolve_controls finishes. ControlOrchestrator will execute the spell
	# after successful control resolution, so this callback is primarily for logging and error
	# handling in the demo controller.
	print("[Controller][_on_control_resolved] control resolution result:", out)
	if typeof(out) == TYPE_DICTIONARY and out.has("ok") and out["ok"]:
		# controls resolved and spell executed by orchestrator
		return
	else:
		# Log error details for debugging
		printerr("[Controller][_on_control_resolved] control resolution failed:", out)


func _on_pause_changed(new_paused: bool) -> void:
	# ensure the pause UI scene exists and show/hide it
	_ensure_pause_ui()
	if _pause_ui:
		# call the scene's API if present, then toggle visibility
		if _pause_ui.has_method("show_menu"):
			_pause_ui.call("show_menu", new_paused)
		_pause_ui.visible = new_paused


func _on_resume_pressed() -> void:
	_set_paused(false)


func _on_quit_pressed() -> void:
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
		print("[Controller][cancel_top_control] calling cancel on top id=", top.has("id") if str(top["id"]) else "<unknown>")
		top["cancel"].call()
	release_control(top["id"])


func release_control(control_id:String) -> bool:
	# Remove a control by id and perform cleanup (hide preview when no controls remain)
	for i in range(control_stack.size()-1, -1, -1):
		var c = control_stack[i]
		if c.has("id") and c["id"] == control_id:
			# call cancel hook if present (but don't double-call if caller already invoked it)
			if c.has("meta") and typeof(c["meta"]) == TYPE_DICTIONARY and c["meta"].has("preview_node"):
				var pn = c["meta"]["preview_node"]
				if pn and pn is Node:
					pn.visible = false
			control_stack.remove_at(i)
			# hide the shared placement_preview if no controls left
			if control_stack.size() == 0 and placement_preview:
				placement_preview.visible = false
			return true
	# not found
	return false
