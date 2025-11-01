// SpellCaster: Node that holds per-aspect mana pools and assigned aspects
#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>

using namespace godot;

class SpellCaster : public Node {
    GDCLASS(SpellCaster, Node)

protected:
    static void _bind_methods();

private:
    // aspect_id -> mana amount
    Dictionary aspect_mana;
    // assigned aspects (Array of String ids)
    Array assigned_aspects;
    // aspect_id -> Dictionary(scaler_key -> numeric multiplier)
    Dictionary aspect_scalers;

public:
    // Mana accessors
    double get_mana(const String &aspect) const;
    void set_mana(const String &aspect, double amount);
    void add_mana(const String &aspect, double amount);
    bool can_deduct(const String &aspect, double amount) const;
    bool deduct_mana(const String &aspect, double amount);

    // Assigned aspects
    Array get_assigned_aspects() const;
    void set_assigned_aspects(const Array &a);

    // Aspect scalers (for caster stats that scale spell params)
    Dictionary get_aspect_scalers() const;
    void set_aspect_scalers(const Dictionary &s);
    double get_scaler(const String &aspect, const String &key) const;
    void set_scaler(const String &aspect, const String &key, double value);
};
