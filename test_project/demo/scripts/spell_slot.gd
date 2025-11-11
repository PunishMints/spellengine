extends Node

class_name SpellSlot

# A designer-facing node to attach a SpellTemplate to a Caster.
# SpellSlot now holds the InputMap action name that will trigger this spell at the character level.

@export var template: Resource
@export var input_action: String = "" # Name of the InputMap action that will trigger this slot
@export var label: String = ""

func _ready():
    # No-op; controller will query this node on startup and when needed.
    pass
