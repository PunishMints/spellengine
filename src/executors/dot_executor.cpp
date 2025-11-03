#include "spellengine/dot_executor.hpp"
#include "spellengine/status_effect.hpp"

#include "spellengine/executor_registry.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void DotExecutor::execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) {
    if (!ctx.is_valid() || !component.is_valid()) return;

    UtilityFunctions::print(String("DotExecutor: executing for component: ") + component->get_executor_id());
    UtilityFunctions::print(String("DotExecutor: received params:"));
    UtilityFunctions::print(resolved_params);

    Dictionary params = resolved_params;
    double amount = 0.0;
    double interval = 1.0;
    double duration = 5.0;
    String aspect = "";

    if (params.has("amount_per_tick")) amount = (double)params["amount_per_tick"];
    if (params.has("tick_interval")) interval = (double)params["tick_interval"];
    if (params.has("duration")) duration = (double)params["duration"];
    if (params.has("aspect")) aspect = (String)params["aspect"];

    Array targets = ctx->get_targets();
    for (int i = 0; i < targets.size(); ++i) {
        Variant v = targets[i];
        Node *node = Object::cast_to<Node>(v);
        if (node) {
            StatusEffect *eff = memnew(StatusEffect);
            // Add to the scene tree first so any timers started in configure() run correctly
            node->add_child(eff);
            // Attach metadata describing this executor invocation
            Dictionary meta;
            meta["executor_id"] = get_executor_id();
            meta["triggering_component"] = component->get_executor_id();
            meta["phase"] = String("dot");
            // propagate cast_id if present in params so targets can group by cast
            if (params.has("cast_id")) meta["cast_id"] = params["cast_id"];
            // also include the aspect so targets can label effects
            if (aspect != "") meta["aspect"] = aspect;
            eff->set_metadata(meta);
            eff->configure(amount, interval, duration, aspect);
        }
    }
}

void DotExecutor::_bind_methods() {
}

String DotExecutor::get_executor_id() const {
    return String("dot_v1");
}

// Register factory for automatic registration at module init
REGISTER_EXECUTOR_FACTORY(DotExecutor)
