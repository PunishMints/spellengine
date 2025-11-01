// Public header for StatusEffect node used by DOT and timed effects
#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

class StatusEffect : public Node {
    GDCLASS(StatusEffect, Node)

protected:
    static void _bind_methods();

private:
    double amount_per_tick = 0.0;
    double tick_interval = 1.0;
    double remaining_time = 0.0;
    String element = "";

public:
    void configure(double amount, double interval, double duration, const String &p_element);
    void _on_tick();
};
