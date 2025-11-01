#include "spellengine/status_effect.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void StatusEffect::configure(double amount, double interval, double duration, const String &p_element) {
    amount_per_tick = amount;
    tick_interval = interval;
    remaining_time = duration;
    element = p_element;

    Timer *t = memnew(Timer);
    t->set_wait_time(tick_interval);
    t->set_one_shot(false);
    add_child(t);
    t->connect("timeout", Callable(this, "_on_tick"));
    t->start();
}

void StatusEffect::_on_tick() {
    if (remaining_time <= 0.0) {
        queue_free();
        return;
    }

    Node *parent = get_parent();
    if (parent) {
        Variant amt = amount_per_tick;
        Variant elem = element;
        if (parent->has_method("apply_damage")) {
            parent->call("apply_damage", amt, elem);
        } else {
            UtilityFunctions::print(String("StatusEffect tick on ") + parent->get_name());
        }
    }

    remaining_time -= tick_interval;
}

void StatusEffect::_bind_methods() {
    ClassDB::bind_method(D_METHOD("configure", "amount", "interval", "duration", "element"), &StatusEffect::configure);
    ClassDB::bind_method(D_METHOD("_on_tick"), &StatusEffect::_on_tick);
}
