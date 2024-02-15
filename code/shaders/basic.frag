#version 460 core

#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec3 frag_pos;
layout(location = 3) flat in uint material_index;

layout(location = 0) out vec4 framebuffer;

struct Material {
    uint  base_colour;
    float roughness;
    float metallic;
    float ior;
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

layout(binding = 3) uniform sampler   u_sampler;
layout(binding = 4) uniform texture2D u_texture;

vec3 lightp = vec3(-8, -3, 5);

void main() {
    Material material = materials[material_index];

    vec4 base_colour = unpackUnorm4x8(material.base_colour);
    vec3 dir = normalize(lightp - frag_pos);

    vec3 view_dir      = normalize(setup.view_p - frag_pos);
    vec3 reflected_dir = reflect(-dir, frag_normal);

    float ambient  = 0.05;
    float diffuse  = max(dot(frag_normal, dir), 0.0);
    float specular = pow(max(dot(view_dir, reflected_dir), 0.0), 256);

    framebuffer = (ambient + diffuse + specular) * texture(sampler2D(u_texture, u_sampler), frag_uv) * base_colour;
}
