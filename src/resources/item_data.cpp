#include "spellengine/item_data.hpp"

#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

String ItemData::get_name() const {
    return name;
}

void ItemData::set_name(const String &p_name) {
    name = p_name;
}

String ItemData::get_description() const {
    return description;
}
void ItemData::set_description(const String &p_description) {
    description = p_description;
}

int ItemData::get_price() const {
    return price;
}
void ItemData::set_price(int p_price) {
    price = p_price;
}

void ItemData::spawn_stuff() {
    Node *my_node = memnew(Node);
    my_node->set_name(name);
    print_line("Spawned stuff!");
}

void ItemData::spawn_custom_scene(Ref<PackedScene> p_scene) {
    Node *instance = nullptr;
    if (p_scene.is_valid()) {
        instance = p_scene->instantiate();
    }
    if (instance) {
        print_line("Valid instance was given!");
    } else {
        print_line("You did NOT give a valid instance!");
    }
}

void ItemData::print_node_name(Node *just_a_node) {
    String n = just_a_node->get_name();
    print_line(n);
}

void ItemData::print_resource_name(Ref<Resource> res) {
    String n = res->get_name();
    print_line(n);
}

void ItemData::print_all_node_names(const TypedArray<Node> &arr) {
    for (Variant const &item : arr) {
        Node *actual_node = Object::cast_to<Node>(item);
        print_line(actual_node->get_name());
    }
}

void ItemData::modify_all_node_names(const TypedArray<Node> &arr, const String &new_name) {
    for (Variant const &item : arr) {
        Node *actual_node = Object::cast_to<Node>(item);
        actual_node->set_name(new_name);
    }
}

void ItemData::modify_all_node_names_wrong_way(TypedArray<Node> arr, String new_name) {
    for (Variant item : arr) {
        Node *actual_node = Object::cast_to<Node>(item);
        actual_node->set_name(new_name);
    }
}

TypedArray<int> ItemData::create_10k_numbers(int min, int max) {
    TypedArray<int> arr;
    arr.resize(10000);

    Ref<RandomNumberGenerator> rng = memnew(RandomNumberGenerator);
    rng->randomize();
    for (int i = 0; i < 10000; i++) {
        arr[i] = rng->randi_range(min, max);
    }
    return arr;
}

void ItemData::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_name"), &ItemData::get_name);
    ClassDB::bind_method(D_METHOD("set_name", "name"), &ItemData::set_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");

    ClassDB::bind_method(D_METHOD("get_description"), &ItemData::get_description);
    ClassDB::bind_method(D_METHOD("set_description", "description"), &ItemData::set_description);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "description"), "set_description", "get_description");

    ClassDB::bind_method(D_METHOD("get_price"), &ItemData::get_price);
    ClassDB::bind_method(D_METHOD("set_price", "price"), &ItemData::set_price);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "price"), "set_price", "get_price");

    ClassDB::bind_method(D_METHOD("spawn_stuff"), &ItemData::spawn_stuff);
    ClassDB::bind_method(D_METHOD("spawn_custom_scene", "scene_to_spawn"), &ItemData::spawn_custom_scene);
    ClassDB::bind_method(D_METHOD("print_node_name", "node"), &ItemData::print_node_name);
    ClassDB::bind_method(D_METHOD("print_resource_name", "resource"), &ItemData::print_resource_name);
    ClassDB::bind_method(D_METHOD("print_all_node_names", "arr"), &ItemData::print_all_node_names);
    ClassDB::bind_method(D_METHOD("modify_all_node_names", "arr", "new_name"), &ItemData::modify_all_node_names);
    ClassDB::bind_method(D_METHOD("modify_all_node_names_wrong_way", "arr", "new_name"), &ItemData::modify_all_node_names_wrong_way);
    ClassDB::bind_method(D_METHOD("create_10k_numbers", "min", "max"), &ItemData::create_10k_numbers);
}
