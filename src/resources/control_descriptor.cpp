#include "spellengine/control_descriptor.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

String ControlDescriptor::get_control_id() const {
    return control_id;
}

void ControlDescriptor::set_control_id(const String &p) {
    control_id = p;
}

String ControlDescriptor::get_control_type() const {
    return control_type;
}

void ControlDescriptor::set_control_type(const String &p) {
    control_type = p;
}
Dictionary ControlDescriptor::get_params() const {
    return params;
}

void ControlDescriptor::set_params(const Dictionary &d) {
    params = d;
}

void ControlDescriptor::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_control_id"), &ControlDescriptor::get_control_id);
    ClassDB::bind_method(D_METHOD("set_control_id", "id"), &ControlDescriptor::set_control_id);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "control_id"), "set_control_id", "get_control_id");

    ClassDB::bind_method(D_METHOD("get_control_type"), &ControlDescriptor::get_control_type);
    ClassDB::bind_method(D_METHOD("set_control_type", "type"), &ControlDescriptor::set_control_type);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "control_type"), "set_control_type", "get_control_type");

    ClassDB::bind_method(D_METHOD("get_params"), &ControlDescriptor::get_params);
    ClassDB::bind_method(D_METHOD("set_params", "params"), &ControlDescriptor::set_params);
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "params"), "set_params", "get_params");
}
