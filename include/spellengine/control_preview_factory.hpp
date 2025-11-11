#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

class ControlPreviewFactory : public Object {
    GDCLASS(ControlPreviewFactory, Object);

protected:
    static void _bind_methods();

public:
    ControlPreviewFactory();
    ~ControlPreviewFactory();

    // Create a lightweight preview Node3D for the given control type.
    // Caller takes ownership of the returned Node*.
    Node *create_preview(const String &type, const Dictionary &params = Dictionary());
};
