#ifndef SPELLENGINE_PREVIEW_NODE_HPP
#define SPELLENGINE_PREVIEW_NODE_HPP

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/color.hpp>

using namespace godot;

class PreviewNode : public Node3D {
    GDCLASS(PreviewNode, Node3D)

public:
    enum PreviewType {
        PREVIEW_NONE = 0,
        PREVIEW_BOX,
        PREVIEW_SPHERE,
        PREVIEW_CYLINDER,
        PREVIEW_LINE
    };

protected:
    static void _bind_methods();

private:
    int preview_type = PREVIEW_NONE;
    Vector3 size = Vector3(1, 1, 1);
    Color color = Color(0.8, 0.2, 0.2, 0.5);

public:
    PreviewNode();
    ~PreviewNode();

    void set_preview_type(int p_type);
    int get_preview_type() const { return preview_type; }

    void set_size(const Vector3 &p_size);
    Vector3 get_size() const { return size; }

    void set_color(const Color &p_color);
    Color get_color() const { return color; }

    void set_enabled(bool p_enabled);
    bool is_enabled() const;

    void update_preview();
};

#endif // SPELLENGINE_PREVIEW_NODE_HPP
