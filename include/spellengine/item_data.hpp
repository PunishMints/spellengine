// Moved public header for ItemData so it can be included from include/spellengine
#pragma once

#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource.hpp>

using namespace godot;

class ItemData : public Resource {
    GDCLASS(ItemData, Resource)

protected:
    static void _bind_methods();

private:
    String name = "Default Item";
    String description = "This is a default item description.";
    int price = 0;

    String get_name() const;
    void set_name(const String &p_name);

    String get_description() const;
    void set_description(const String &p_description);

    int get_price() const;
    void set_price(int p_price);

    void spawn_stuff();
    void spawn_custom_scene(Ref<PackedScene> p_scene);
    void print_node_name(Node *just_a_node);
    void print_resource_name(Ref<Resource> res);
    void print_all_node_names(const TypedArray<Node> &arr);
    void modify_all_node_names(const TypedArray<Node> &arr, const String &new_name);
    void modify_all_node_names_wrong_way(TypedArray<Node> arr, String new_name);
    TypedArray<int> create_10k_numbers(int min, int max);
};
