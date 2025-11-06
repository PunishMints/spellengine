#include "spellengine/damage_executor.hpp"

#include "spellengine/executor_registry.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void DamageExecutor::execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) {
    if (!ctx.is_valid() || !component.is_valid()) return;

    Dictionary params = resolved_params;
    double amount = 0.0;
    String aspect = "";
    if (params.has("amount")) amount = (double)params["amount"];
    if (params.has("aspect")) aspect = (String)params["aspect"];

    Array targets = ctx->get_targets();
    for (int i = 0; i < targets.size(); ++i) {
        Variant v = targets[i];
        Object *obj = Object::cast_to<Object>(v);
        Node *node = Object::cast_to<Node>(v);
        if (node) {
            if (node->has_method("apply_damage")) {
                Dictionary meta;
                meta["executor_id"] = String("damage_v1");
                meta["phase"] = String("instant");
                if (params.has("cast_id")) meta["cast_id"] = params["cast_id"];
                if (aspect != "") meta["aspect"] = aspect;
                node->call("apply_damage", Variant(amount), Variant(aspect), meta);
            } else {
                UtilityFunctions::print(String("DamageExecutor: target has no apply_damage: ") + node->get_name());
            }
        } else if (obj) {
            if (obj->has_method("apply_damage")) {
                Dictionary meta;
                meta["executor_id"] = String("damage_v1");
                meta["phase"] = String("instant");
                if (params.has("cast_id")) meta["cast_id"] = params["cast_id"];
                if (aspect != "") meta["aspect"] = aspect;
                obj->call("apply_damage", Variant(amount), Variant(aspect), meta);
            }
        }
    }
}

void DamageExecutor::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_param_schema"), &DamageExecutor::get_param_schema);
}

String DamageExecutor::get_executor_id() const {
    return String("damage_v1");
}

Dictionary DamageExecutor::get_param_schema() const {
    Dictionary schema;
    Dictionary e;
    e["type"] = "float";
    e["default"] = 0.0;
    e["desc"] = "Amount of damage to apply";
    schema["amount"] = e;

    e = Dictionary();
    e["type"] = "string";
    e["default"] = String("");
    e["desc"] = "Aspect label (optional)";
    schema["aspect"] = e;

    return schema;
}

// Register factory for automatic registration at module init
REGISTER_EXECUTOR_FACTORY(DamageExecutor)
