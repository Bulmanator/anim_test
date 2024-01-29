#version 460 core

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

layout(binding = 1, std430, row_major)
buffer Globals {
    mat4 proj;
    mat4 view;
};

layout(binding = 4, std430)
buffer Materials {
    Material materials[];
};

vec3 lightp = vec3(0, -3, 5);

void main() {
    vec4 base_colour = vec4(materials[material_index].r, materials[material_index].g,
            materials[material_index].b, materials[material_index].a);

    vec3 dir = normalize(lightp - frag_pos);

    float ambient  = 0.05;
    float diffuse  = max(dot(frag_normal, dir), 0.0);

    framebuffer = (ambient + diffuse) * base_colour;
}
