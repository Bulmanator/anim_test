#version 460 core

#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
    vec3 position;
    uint16_t u, v; // should really be float16_t
    uint8_t nx, ny, nz, nw;

    uint material_index;

    uint8_t indices[4];
    uint8_t weights[4];
};

layout(push_constant, scalar, row_major)
uniform R_Setup {
    mat4 view_proj;

    vec3  view_p;
    float time;

    float dt;
    uint  window_width;
    uint  window_height;

    float unused[9];
} setup;

layout(binding = 0, scalar)
buffer Vertices {
    Vertex vertices[];
};

layout(binding = 1, std430, row_major)
buffer Bones {
    mat4 bones[];
};

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) out vec3 frag_pos;
layout(location = 3) flat out uint material_index;

void main() {
    Vertex vertex = vertices[gl_VertexIndex];

    vec3 local_position = vertex.position;
    vec4 position = vec4(0, 0, 0, 0);

    for (int it = 0; it < 4; ++it) {
        position += (vertex.weights[it] / 255.0) * (bones[vertex.indices[it]] * vec4(local_position, 1.0));
    }

    gl_Position = setup.view_proj * vec4(position.xyz, 1.0);

    frag_uv        = vec2(vertex.u,  vertex.v) / 65535.0;
    frag_normal    = (vec3(vertex.nx, vertex.ny, vertex.nz) / 127.0) - 1.0;
    frag_pos       = position.xyz;
    material_index = vertex.material_index;
}
