// Resource describing an interactive control for a Spell (input trigger + options)
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

class ControlDescriptor : public Resource {
    GDCLASS(ControlDescriptor, Resource)

protected:
    static void _bind_methods();

private:
    String control_id = ""; // logical id for the control (e.g. "aim", "target")
    String control_type = ""; // executor-style id (choose_vector_v1, choose_position_v1...)
    // Trigger details (input bindings) removed: input mapping is handled by SpellSlot/InputMap.
    Dictionary params; // targeter/editor params (max_distance, allow_ground, etc.)

public:
    String get_control_id() const;
    void set_control_id(const String &p);

    String get_control_type() const;
    void set_control_type(const String &p);



    Dictionary get_params() const;
    void set_params(const Dictionary &d);
};
