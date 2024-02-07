#version 460 core

#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec3 frag_pos;
layout(location = 3) flat in uint material_index;

layout(location = 0) out vec4 framebuffer;

struct Material {
    float r, g, b, a;
    float roughness;
    float metallic;
    float ior;

    float anisotropic;
    float anisotropic_rotation;

    float clear_coat;
    float clear_coat_roughness;

    float sheen;
    float sheen_roughness;
};

layout(push_constant, scalar, row_major)
uniform R_Setup {
    mat4 view_proj;

    vec3  view_p;
    float time;

    float dt;
    uint  window_width;
    uint  window_height;

    float unused;
} setup;

layout(binding = 2, std430)
readonly buffer Materials {
    Material materials[];
};

vec3 lightp = vec3(-8, -3, 5);

void main() {
    vec4 base_colour = vec4(materials[material_index].r, materials[material_index].g,
            materials[material_index].b, materials[material_index].a);

    vec3 dir = normalize(lightp - frag_pos);

    vec3 view_dir      = normalize(setup.view_p - frag_pos);
    vec3 reflected_dir = reflect(-dir, frag_normal);

    float ambient  = 0.05;
    float diffuse  = max(dot(frag_normal, dir), 0.0);
    float specular = pow(max(dot(view_dir, reflected_dir), 0.0), 256);

    framebuffer = (ambient + diffuse + specular) * base_colour;
}
