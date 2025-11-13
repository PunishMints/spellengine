#include "spellengine/control_preview_factory.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/mesh.hpp>

using namespace godot;

ControlPreviewFactory::ControlPreviewFactory() {}
ControlPreviewFactory::~ControlPreviewFactory() {}

void ControlPreviewFactory::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_preview", "type", "params"), &ControlPreviewFactory::create_preview, DEFVAL(Dictionary()));
}

Node *ControlPreviewFactory::create_preview(const String &type, const Dictionary &params) {
    Node3D *root = memnew(Node3D());
    MeshInstance3D *mi = memnew(MeshInstance3D());

    Ref<Mesh> mesh;
    if (type == "box" || type == "cube" || type == "position") {
        Ref<BoxMesh> cm = memnew(BoxMesh());
        mesh = cm;
    } else if (type == "sphere") {
        Ref<SphereMesh> sm = memnew(SphereMesh());
        mesh = sm;
    } else if (type.find("choose_vector") != -1 || type == "vector") {
        // Create an ImmediateMesh-backed MeshInstance3D for drawing a line preview
        Ref<ImmediateMesh> im = memnew(ImmediateMesh());
        mi->set_mesh(im);
        Ref<StandardMaterial3D> mat = memnew(StandardMaterial3D());
        mat->set_albedo(Color(0.2, 0.8, 0.4, 0.9));
        mat->set_emission(Color(0.4, 1.0, 0.6));
        mi->set_material_override(mat);
        root->add_child(mi);
        return root;
    } else {
        Ref<SphereMesh> sm = memnew(SphereMesh());
        mesh = sm;
    }

    mi->set_mesh(mesh);

    Ref<StandardMaterial3D> mat = memnew(StandardMaterial3D());
    mat->set_albedo(Color(0.2, 0.6, 1.0, 0.6));
    mat->set_emission(Color(0.4, 0.8, 1.0));
    mi->set_material_override(mat);

    root->add_child(mi);
    return root;
}
