// Damage executor: applies instant damage to targets
#pragma once

#include "spellengine/executor_base.hpp"

using namespace godot;

class DamageExecutor : public IExecutor {
    GDCLASS(DamageExecutor, IExecutor)

protected:
    static void _bind_methods();

public:
    virtual void execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) override;
    virtual String get_executor_id() const override;
    virtual Dictionary get_param_schema() const override;
};
