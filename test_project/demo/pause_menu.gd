extends CanvasLayer

signal resume_pressed
signal quit_pressed

@onready var panel: Panel = $Panel
@onready var resume_btn: Button = $Panel/VBoxContainer/ResumeButton
@onready var quit_btn: Button = $Panel/VBoxContainer/QuitButton

func _ready() -> void:
	# Connect buttons to emit signals so the parent can handle pause/unpause
	resume_btn.connect("pressed", Callable(self, "_on_resume_pressed"))
	quit_btn.connect("pressed", Callable(self, "_on_quit_pressed"))
	# Pause processing is configured in the project settings; no runtime changes here.
	visible = false
	# Debug info to help trace input while paused
	print("[PauseMenu] _ready() path=", get_path(), " process_mode=", process_mode)

func _unhandled_input(event) -> void:
	# Log mouse/button/key presses so we can see if the UI receives input while paused
	if event is InputEventMouseButton:
		var mb = event as InputEventMouseButton
		if mb.pressed:
			print("[PauseMenu] MouseButton pressed idx=", mb.button_index, " at=", mb.position)
	if event is InputEventKey:
		var k = event as InputEventKey
		if k.pressed:
			print("[PauseMenu] Key pressed scancode=", k.scancode)

func show_menu(paused: bool) -> void:
	visible = paused
	# optionally grab focus to prevent game UI interaction
	if paused:
		panel.grab_focus()

func _on_resume_pressed() -> void:
	print("[PauseMenu] resume pressed; emitting signal")
	emit_signal("resume_pressed")

func _on_quit_pressed() -> void:
	print("[PauseMenu] quit pressed; emitting signal")
	emit_signal("quit_pressed")
