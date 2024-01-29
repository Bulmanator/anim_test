#version 460 core

#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

struct Vertex {
    float x, y, z;
    float u, v;
    float nx, ny, nz;

    uint8_t indices[4];
    float w[3];

    uint material_index;
};

layout(binding = 0, std430)
buffer Vertices {
    Vertex vertices[];
};

layout(binding = 1, std430, row_major)
buffer Globals {
    mat4 proj;
    mat4 view;
};

layout(binding = 2, std430, row_major)
buffer Bones {
    mat4 bones[];
};

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) out vec3 frag_pos;
layout(location = 3) flat out uint material_index;

void main() {
    Vertex vertex = vertices[gl_VertexIndex];

    vec3 local_position = vec3(vertex.x, vertex.y, vertex.z);
    vec4 position = vec4(0, 0, 0, 0);

    for (int it = 0; it < 3; ++it) {
        position += vertex.w[it] * (bones[vertex.indices[it]] * vec4(local_position, 1.0));
    }

    // I don't think this is worth it, just store the 4th weight in the vertex
    float w4 = 1 - vertex.w[0] - vertex.w[1] - vertex.w[2];
    position += (w4 * (bones[vertex.indices[3]] * vec4(local_position, 1.0)));

    gl_Position = proj * view * vec4(position.xyz, 1.0);

    frag_uv        = vec2(vertex.u,  vertex.v);
    frag_normal    = vec3(vertex.nx, vertex.ny, vertex.nz);
    frag_pos       = vec3(vertex.x, vertex.y, vertex.z);
    material_index = vertex.material_index;
}
