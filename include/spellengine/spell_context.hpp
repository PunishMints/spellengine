// SpellContext: holds runtime data for a spell execution
#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

class Spell;

class SpellContext : public RefCounted {
    GDCLASS(SpellContext, RefCounted)

protected:
    static void _bind_methods();

private:
    Node *caster = nullptr;
    Array targets;
    Dictionary params;
    Dictionary results;

public:
    void set_caster(Node *p_caster);
    Node *get_caster() const;

    void set_targets(const Array &p_targets);
    Array get_targets() const;

    void set_params(const Dictionary &p_params);
    Dictionary get_params() const;

    void set_results(const Dictionary &p_results);
    Dictionary get_results() const;

    // Derive aspect distribution from a composed Spell (normalized weights per aspect)
    Dictionary derive_aspect_distribution(Ref<Spell> spell) const;

    // Convenience: return an Array of aspect ids sorted by descending contribution
    Array derive_aspects_list(Ref<Spell> spell) const;

    // Derive aspects and set them into this context's params under key "aspects"
    void derive_and_set_aspects(Ref<Spell> spell);
};
