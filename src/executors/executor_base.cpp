#include "spellengine/executor_base.hpp"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void IExecutor::_bind_methods() {
    // Expose parameter schema query to GDScript for editor tooling
    ClassDB::bind_method(D_METHOD("get_param_schema"), &IExecutor::get_param_schema);
}
