// Synergy resource: designer-defined modifiers and extra executors for aspect combinations
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

class Synergy : public Resource {
    GDCLASS(Synergy, Resource)

protected:
    static void _bind_methods();

private:
    Dictionary spec;

public:
    Dictionary get_spec() const;
    void set_spec(const Dictionary &p);
};
