// Summon executor: instantiates a PackedScene at a given transform
#pragma once

#include "spellengine/executor_base.hpp"

using namespace godot;

class SummonExecutor : public IExecutor {
    GDCLASS(SummonExecutor, IExecutor)

protected:
    static void _bind_methods();

public:
    virtual void execute(Ref<SpellContext> ctx, Ref<SpellComponent> component, const Dictionary &resolved_params) override;
    virtual String get_executor_id() const override;
    virtual Dictionary get_param_schema() const override;
};
