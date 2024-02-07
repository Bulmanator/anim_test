#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_syswm.h>

#define CORE_IMPL 1
#include "core.h"

static void *zmalloc(size_t size) {
    void *result = malloc(size);
    memset(result, 0, size);

    return result;
}

#include "math.cpp"
#include "vulkan.cpp"

#include "render.h"

#include "file_formats.h"
#include "animation.h"

// Animation file data
//

#pragma pack(push, 1)

// Mesh file
//

// 24 bytes
//
struct Vertex3 {
    Vec3F position;
    U16   uv[2];
    U8    normal[4];

    // this doesn't really need to be a U32 but the struct will likely be
    // padded anyway because 22 bytes is a weird size (i.e. for a U16)
    //
    U32 material_index;
};

// 32 bytes
//
// U8 bone weights seem fine at the moment, however, the test mesh is a robot which is designed to be
// rigid so probably want to test on something which would have more compression etc.
//
struct SkinnedVertex3 {
    union {
        struct {
            Vec3F position;
            U16   uv[2];
            U8    normal[4];

            U32 material_index;
        };

        Vertex3 vertex;
    };

    U8 bone_indices[4];
    U8 bone_weights[4];
};

struct A_Material {
    Vec4F base_colour;
    F32 roughness;
    F32 metallic;
    F32 ior;

    F32 anisotropic;
    F32 anisotropic_rotation;

    F32 clear_coat;
    F32 clear_coat_roughness;

    F32 sheen;
    F32 sheen_roughness;

    Str8 name;
};

struct A_Mesh {
    B32 has_bones;

    Str8 string_table;

    U32 num_vertices;

    U32 num_indices;
    U16 *indices;

    union {
        Vertex3 *vertices;
        SkinnedVertex3 *skinned_vertices;
    };

    U32 num_materials;
    A_Material *materials;
};

#pragma pack(pop)

#if 0
static B32 MeshFileLoad(A_Mesh *mesh, const char *path) {
    B32 result = false;

    FILE *f = fopen(path, "rb");
    if (f) {
        fread(&mesh->has_bones,    sizeof(B8),  1, f);
        fread(&mesh->num_faces,    sizeof(U16), 1, f);
        fread(&mesh->num_vertices, sizeof(U16), 1, f);

        U64 idx_size = sizeof(U16) * 3 * mesh->num_faces;
        U64 vtx_size = mesh->num_vertices * (mesh->has_bones ? sizeof(SkinnedVertex3) : sizeof(Vertex3));

        mesh->indices  = (U16 *) zmalloc(idx_size);
        mesh->vertices = (Vertex3 *) zmalloc(vtx_size); // while cast to Vertex3 still valid for skinned_vertices because of union

        fread(mesh->indices,  idx_size, 1, f);
        fread(mesh->vertices, vtx_size, 1, f);

        fclose(f);

        result = true;
    }

    return result;
}
#else

#pragma pack(push, 1)

// @todo: this is very incomplete, need to think more seriously about the layout of the mesh file!
//

#define AMTM_MAGIC   FourCC('A', 'M', 'T', 'M')
#define AMTM_VERSION 1

struct AMTM_Header {
    U32 magic;
    U32 version;

    U32 flags;

    U32 num_meshes;
    U32 num_vertices;
    U32 num_indices;

    U32 num_materials;
    U32 num_textures;

    U32 string_table_count;

    U32 pad[7];
};

StaticAssert(sizeof(AMTM_Header) == 64);

struct AMTM_Vertex {
    F32 position[3];
    F32 uv[2];
    F32 normal[3];

    U32 material_index;
};

struct AMTM_SkinnedVertex {
    union {
        struct {
            F32 position[3];
            F32 uv[2];
            F32 normal[3];

            U32 material_index;
        };

        AMTM_Vertex vertex;
    };

    U8  bone_indices[4];
    F32 bone_weights[4];
};

struct AMTM_Material {
    Vec4F colour;

    F32 roughness;
    F32 metallic;
    F32 ior;

    F32 anisotropic;
    F32 anisotropic_rotation;

    F32 clear_coat;
    F32 clear_coat_roughness;

    F32 sheen;
    F32 sheen_roughness;

    U16 name_count;
    U16 name_offset;
};

#pragma pack(pop, 1)

#if 1
Function B32 MeshFileLoad(Arena *arena, A_Mesh *mesh, Str8 path) {
    B32 result = false;

    TempArena temp = TempGet(0, 0);
    char *zpath    = ArenaPush(temp.arena, char, path.count + 1);

    memcpy(zpath, path.data, path.count);

    FILE *handle = fopen(zpath, "rb");
    if (handle) {
        Str8 data;

        fseek(handle, 0, SEEK_END);
        data.count = ftell(handle);
        fseek(handle, 0, SEEK_SET);

        data.data = ArenaPush(temp.arena, U8, data.count);

        fread(data.data, data.count, 1, handle);
        fclose(handle);

        AMTM_Header *header = cast(AMTM_Header *) data.data;

        if (header->magic != AMTM_MAGIC) {
            printf("[ERR] :: Not a AMTM file\n");
            return result;
        }

        if (header->version != AMTM_VERSION) {
            printf("[ERR] :: Unknown AMTM file version\n");
            return result;
        }

        // @incomplete: rough file layout for mesh
        //
        // Header
        // String Table
        // Mesh
        // Materials
        // Vertices
        // Indices
        //

        // Setup the string table
        //
        Str8 string_table;
        string_table.count = header->string_table_count;
        string_table.data  = cast(U8 *) (header + 1);

        mesh->string_table.count = string_table.count;
        mesh->string_table.data  = ArenaPushCopy(arena, string_table.data, U8, string_table.count);

        // @incomplete: load dummy mesh, as we don't split on export there is only one and its kinda
        // useless to us
        //
        // so skip past it for now
        //
        U64 mesh_offset = 12;

        // Load material info
        //
        mesh->num_materials = header->num_materials;
        mesh->materials     = ArenaPush(arena, A_Material, mesh->num_materials);

        AMTM_Material *material = cast(AMTM_Material *) (string_table.data + string_table.count + mesh_offset);
        for (U32 it = 0; it < mesh->num_materials; ++it) {
            A_Material *dst = &mesh->materials[it];

            dst->base_colour = material->colour;

            dst->roughness = material->roughness;
            dst->metallic  = material->metallic;
            dst->ior       = material->ior;

            dst->anisotropic          = material->anisotropic;
            dst->anisotropic_rotation = material->anisotropic_rotation;

            dst->clear_coat           = material->clear_coat;
            dst->clear_coat_roughness = material->clear_coat_roughness;

            dst->sheen           = material->sheen;
            dst->sheen_roughness = material->sheen_roughness;

            dst->name.count = material->name_count;
            dst->name.data  = &mesh->string_table.data[material->name_offset];

            material++;
        }

        // Load vertex and index data
        //
        mesh->num_vertices = header->num_vertices;
        mesh->num_indices  = header->num_indices;

        mesh->has_bones = (header->flags & 0x1) != 0;

        U16 *indices = 0;

        if (mesh->has_bones) {
            mesh->skinned_vertices = ArenaPush(arena, SkinnedVertex3, mesh->num_vertices);

            // material has now iterated over all of the material info so now points to the beginning
            // of the vertex array
            //
            AMTM_SkinnedVertex *vertices = cast(AMTM_SkinnedVertex *) material;

            for (U32 it = 0; it < mesh->num_vertices; ++it) {
                AMTM_SkinnedVertex *src = &vertices[it];
                SkinnedVertex3     *dst = &mesh->skinned_vertices[it];

                dst->position.x = src->position[0];
                dst->position.y = src->position[1];
                dst->position.z = src->position[2];

                dst->uv[0] = cast(U16) (src->uv[0] * U16_MAX);
                dst->uv[1] = cast(U16) (src->uv[1] * U16_MAX);

                dst->normal[0] = cast(U8) ((src->normal[0] * 127.0f) + 127.5f);
                dst->normal[1] = cast(U8) ((src->normal[1] * 127.0f) + 127.5f);
                dst->normal[2] = cast(U8) ((src->normal[2] * 127.0f) + 127.5f);
                dst->normal[3] = 254; // 1.0 unused in shader, here for padding

                dst->material_index = src->material_index;

                dst->bone_indices[0] = src->bone_indices[0];
                dst->bone_indices[1] = src->bone_indices[1];
                dst->bone_indices[2] = src->bone_indices[2];
                dst->bone_indices[3] = src->bone_indices[3];

                dst->bone_weights[0] = cast(U8) (src->bone_weights[0] * U8_MAX);
                dst->bone_weights[1] = cast(U8) (src->bone_weights[1] * U8_MAX);
                dst->bone_weights[2] = cast(U8) (src->bone_weights[2] * U8_MAX);
                dst->bone_weights[3] = cast(U8) (src->bone_weights[3] * U8_MAX);

                // doubt this is gonna work
                U32 sum = dst->bone_weights[0] + dst->bone_weights[1] + dst->bone_weights[2] + dst->bone_weights[3];
                Assert(sum <= 255);
            }

            indices = cast(U16 *) (vertices + mesh->num_vertices);
        }
        else {
            mesh->vertices = ArenaPush(arena, Vertex3, mesh->num_vertices);
            Assert(!"INCOMPLETE MUST BE SKINNED MESH");
        }

        Assert(indices != 0);

        mesh->indices = ArenaPushCopy(arena, indices, U16, mesh->num_indices);

        // @incomplete: not loading textures either!!!!
        //

        result = true;
    }

    return result;
}
#else

static B32 MeshFileLoad(A_Mesh *mesh, const char *path) {
    B32 result = false;

    FILE *f = fopen(path, "rb");
    if (f) {
        U32 magic, version;
        fread(&magic,   sizeof(U32), 1, f);
        fread(&version, sizeof(U32), 1, f);

        if (magic != 0x4D544D41) {
            printf("[ERR] :: Not an AMTM file\n");
            return result;
        }

        if (version != 1) {
            printf("[ERR] :: Unsupported file version\n");
            return result;
        }

        U32 flags, num_meshes, total_vertices, total_indices;
        U32 num_materials, num_textures, string_table_count;

        fread(&flags,              sizeof(U32), 1, f);
        fread(&num_meshes,         sizeof(U32), 1, f);
        fread(&total_vertices,     sizeof(U32), 1, f);
        fread(&total_indices,      sizeof(U32), 1, f);
        fread(&num_materials,      sizeof(U32), 1, f);
        fread(&num_textures,       sizeof(U32), 1, f);
        fread(&string_table_count, sizeof(U32), 1, f);

        fseek(f, 7 * sizeof(U32), SEEK_CUR); // past pad

        printf("Header {\n");
        printf("  - flags              = 0x%x\n", flags);
        printf("  - num_meshes         = %d\n", num_meshes);
        printf("  - total_vertices     = %d\n", total_vertices);
        printf("  - total_indices      = %d\n", total_indices);
        printf("  - num_materials      = %d\n", num_materials);
        printf("  - num_textures       = %d\n", num_textures);
        printf("  - string_table_count = %d\n", string_table_count);
        printf("}\n");

        if (num_meshes != 1) {
            // @incomplete: the exporter doesn't handle multiple/split large meshes yet
            //
            printf("[ERR] :: Only single mesh files are currently supported\n");
            return result;
        }

        mesh->string_table.count = string_table_count;
        mesh->string_table.data  = (U8 *) malloc(string_table_count);

        fread(mesh->string_table.data, mesh->string_table.count, 1, f);

        mesh->num_materials = num_materials;
        mesh->materials     = (A_Material *) zmalloc(num_materials * sizeof(A_Material));

        U32 base_vertex;
        U32 base_index;
        U32 num_indices;

        fread(&base_vertex, sizeof(U32), 1, f);
        fread(&base_index,  sizeof(U32), 1, f);
        printf("base vertex = %d, base index = %d\n", base_vertex, base_index);
        fread(&num_indices, sizeof(U32), 1, f);

        if (num_indices != total_indices) {
            printf("[ERR] :: Indices count mismatch (%d wanted, %d got)\n", total_indices, num_indices);
            return result;
        }

        for (U32 it = 0; it < num_materials; ++it) {
            A_Material *material = &mesh->materials[it];

            fread(&material->base_colour,          sizeof(Vec4F), 1, f);
            fread(&material->roughness,            sizeof(F32),   1, f);
            fread(&material->metallic,             sizeof(F32),   1, f);
            fread(&material->ior,                  sizeof(F32),   1, f);
            fread(&material->anisotropic,          sizeof(F32),   1, f);
            fread(&material->anisotropic_rotation, sizeof(F32),   1, f);
            fread(&material->clear_coat,           sizeof(F32),   1, f);
            fread(&material->clear_coat_roughness, sizeof(F32),   1, f);
            fread(&material->sheen,                sizeof(F32),   1, f);
            fread(&material->sheen_roughness,      sizeof(F32),   1, f);

            U16 offset, count;
            fread(&count,  sizeof(U16), 1, f);
            fread(&offset, sizeof(U16), 1, f);

            material->name.count = count;
            material->name.data  = &mesh->string_table.data[offset];

            printf("Material[%d] = %.*s\n", it, (U32) material->name.count, material->name.data);
        }

        // @incomplete: exporter currently doesn't write texture data out!
        //
        // :texture_data
        //
        assert(num_textures == 0);

        U32 vtx_size;
        if (flags & 0x1) { // HAS_SKELETON
            vtx_size = sizeof(SkinnedVertex3);
        }
        else {
            vtx_size = sizeof(Vertex3);
        }

        mesh->num_vertices = total_vertices;
        mesh->vertices     = (Vertex3 *) malloc(total_vertices * vtx_size);

        printf("Reading %d bytes (%d * %d)\n", total_vertices * vtx_size, vtx_size, total_vertices);

        fread(mesh->vertices, vtx_size, total_vertices, f);

        for (U32 it = 0; it < 25; ++it) {
            printf("Vertex[%d] = %d (%f)\n", it, mesh->vertices[it].material_index, *(F32 *) &mesh->vertices[it].material_index);
        }

        mesh->num_indices = total_indices;
        mesh->indices     = (U16 *) malloc(total_indices * sizeof(U16));

        fread(mesh->indices, sizeof(U16), total_indices, f);

        // :texture_data
        //

        result = true;
        fclose(f);
    }

    return result;
}
#endif
#endif

static B32 SkeletonFileLoad(Arena *arena, A_Skeleton *skeleton, Str8 path) {
    B32 result = false;

    TempArena temp = TempGet(0, 0);

    // path may or may not already be zero terminated, do this to make sure it is
    //
    // at the moment we know it is always zero terminated, however, in the future this might not be the case
    //
    char *zpath = ArenaPush(temp.arena, char, path.count + 1);
    memcpy(zpath, path.data, path.count); // @todo: add MemoryCopy or something function to core

    FILE *handle = fopen(zpath, "rb");
    if (handle) {
        Str8 data;

        fseek(handle, 0, SEEK_END);
        data.count = ftell(handle);
        fseek(handle, 0, SEEK_SET);

        data.data = ArenaPush(temp.arena, U8, data.count, ARENA_FLAG_NO_ZERO);

        fread(data.data, data.count, 1, handle);
        fclose(handle);

        AMTS_Skeleton amts = { 0 };
        AMTS_SkeletonFromData(&amts, data);

        if (amts.version == AMTS_VERSION) {
            // we have a version we recognise
            //
            Str8 string_table;
            string_table.count = amts.string_table.count;
            string_table.data  = ArenaPushCopy(arena, amts.string_table.data, U8, string_table.count);

            skeleton->string_table = string_table;
            skeleton->framerate    = amts.framerate;

            skeleton->num_bones      = amts.num_bones;
            skeleton->num_animations = amts.num_tracks;

            skeleton->bones = ArenaPush(arena, A_Bone, skeleton->num_bones);

            Assert(sizeof(AMTS_Sample) == sizeof(A_Sample));

            for (U32 it = 0; it < skeleton->num_bones; ++it) {
                AMTS_BoneInfo *src = &amts.bones[it];
                A_Bone *dst = &skeleton->bones[it];

                dst->parent_index = src->parent_index;

                dst->name.count = src->name_count;
                dst->name.data  = &string_table.data[src->name_offset];

                A_Sample inv_bind_pose;

                memcpy(&dst->bind_pose, &src->bind_pose,     sizeof(AMTS_Sample));
                memcpy(&inv_bind_pose,  &src->inv_bind_pose, sizeof(AMTS_Sample));

                dst->inv_bind_pose = A_SampleToM4x4F(&inv_bind_pose);
            }

            A_Sample *samples = ArenaPushCopy(arena, amts.samples, A_Sample, amts.total_samples);

            skeleton->animations = ArenaPush(arena, A_Animation, skeleton->num_animations);
            for (U32 it = 0; it < skeleton->num_animations; ++it) {
                AMTS_TrackInfo *track     = &amts.tracks[it];
                A_Animation    *animation = &skeleton->animations[it];

                animation->name.count = track->name_count;
                animation->name.data  = &string_table.data[track->name_offset];

                animation->time       = 0;
                animation->time_scale = 1;

                animation->num_frames = track->num_frames;
                animation->samples    = samples;

                samples += (animation->num_frames * skeleton->num_bones);
            }
        }

        result = true;
    }

    TempRelease(&temp);

    return result;
}

static Str8 FileReadAll(const char *path) {
    Str8 result = {};

    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        result.count = ftell(f);
        fseek(f, 0, SEEK_SET);

        result.data = (U8 *) malloc(result.count);

        fread(result.data, result.count, 1, f);
        fclose(f);
    }

    return result;
}

#if OS_WINDOWS
static U64 U64TicksGet() {
    U64 result;

    LARGE_INTEGER i;
    QueryPerformanceCounter(&i);

    result = i.QuadPart;
    return result;
}

static F64 F64ElapsedTimeGet(U64 start, U64 end) {
    F64 result;

    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);

    result = (F64) (end - start) / (F64) f.QuadPart;
    return result;
}
#elif OS_LINUX
static U64 U64TicksGet() {
    U64 result;

    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);

    result = (1000000000 * t.tv_sec) + t.tv_nsec;
    return result;
}

static F64 F64ElapsedTimeGet(U64 start, U64 end) {
    F64 result = (F64) (end - start) / 1000000000.0;
    return result;
}
#endif

#if OS_WINDOWS
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void) hInstance;
    (void) hPrevInstance;
    (void) lpCmdLine;
    (void) nCmdShow;

    {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            Assert(AllocConsole() == TRUE);
        }

        HANDLE std_handle = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

        Assert(std_handle != INVALID_HANDLE_VALUE);

        SetStdHandle(STD_OUTPUT_HANDLE, std_handle);
        SetStdHandle(STD_ERROR_HANDLE,  std_handle);

        freopen("CON", "w", stdout);
        freopen("CON", "w", stderr);
    }
#elif OS_LINUX
int main(int argc, char **argv) {
    (void) argv;
    (void) argc;
#endif


#if 0
    if (argc < 3) {
        printf("usage: %s <file>.mesh <file>.anim\n", argv[0]);
        return 1;
    }
    const char *mesh_path = argv[1];
    const char *skel_path = argv[2];
#else
    //const char *mesh_path = "../test/mesh/ninja_female_01.mesh";
    //const char *skel_path = "../test/skeleton/ninja_female_01.anim";

    const char *mesh_path = "../test/Characters_Mako.amtm";
    const char *skel_path = "../test/Characters_Mako.amts";
#endif

    // reserve a 64 gib arena
    //
    Arena *arena = ArenaAlloc(GB(64));

    A_Mesh mesh = {};
    {
        Str8 path;
        path.count = strlen(mesh_path);
        path.data  = cast(U8 *) mesh_path;

        if (!MeshFileLoad(arena, &mesh, path)) {
            printf("[error] :: failed to load mesh\n");
            return 1;
        }

        printf("Mesh info:\n");
        printf("   - %d indices\n",  mesh.num_indices);
        printf("   - %d vertices\n", mesh.num_vertices);
    }

    A_Skeleton skeleton = {};
    {
        Str8 path;
        path.count = strlen(skel_path);
        path.data  = cast(U8 *) skel_path;

        if (!SkeletonFileLoad(arena, &skeleton, path)) {
            printf("[error] :: failed to load skeleton\n");
            return 1;
        }
    }

    printf("Skeleton info:\n");
    printf("    - %d bones\n", skeleton.num_bones);
    printf("    - %d animations\n", skeleton.num_animations);

    printf("\nAnimations:\n");
    for (U32 it = 0; it < skeleton.num_animations; ++it) {
        A_Animation *animation = &skeleton.animations[it];
        printf("  [%d]: %.*s\t(%d frames)\n", it, (U32) animation->name.count, animation->name.data, animation->num_frames);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[ERR] :: failed to initialise SDL2 (%s)\n", SDL_GetError());
        return 1;
    }

    int wx = SDL_WINDOWPOS_CENTERED;
    int wy = SDL_WINDOWPOS_CENTERED;

    SDL_Window *window = SDL_CreateWindow("Animation", wx, wy, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        printf("[ERR] :: failed to create window (%s)\n", SDL_GetError());
        return 1;
    }

    SDL_SetWindowMinimumSize(window, 1280, 720);
    SDL_SetWindowMaximumSize(window, 1280, 720);

    SDL_SysWMinfo wm_info = {};
    SDL_VERSION(&wm_info.version);

    if (!SDL_GetWindowWMInfo(window, &wm_info)) {
        printf("[ERR] :: failed to get window data (%s)\n", SDL_GetError());
        return 1;
    }

    VK_Context _vk = {};
    VK_Context *vk = &_vk;

    vk->flags = VK_CONTEXT_FLAG_DEBUG;

    if (!VK_ContextInitialise(vk)) {
        printf("[error] :: failed to initialise vulkan\n");
        return 1;
    }

    VK_Device *device = vk->device;

    S32 window_width = 1280, window_height = 720;
    SDL_Vulkan_GetDrawableSize(window, &window_width, &window_height);

    VK_Swapchain _swapchain = {};
    VK_Swapchain *swapchain = &_swapchain;

#if OS_WINDOWS
    swapchain->surface.win.hinstance = wm_info.info.win.hinstance;
    swapchain->surface.win.hwnd      = wm_info.info.win.window;
#elif OS_LINUX
    swapchain->surface.wl.display = wm_info.info.wl.display;
    swapchain->surface.wl.surface = wm_info.info.wl.surface;
#endif

    swapchain->surface.width  = window_width;
    swapchain->surface.height = window_height;

    swapchain->vsync = true;

    if (!VK_SwapchainCreate(device, swapchain)) {
        printf("[ERR] :: failed to create swapchain\n");
        return 1;
    }

    VK_Pipeline pipeline = {};

    {
        // Create a pipeline, this is extremely basic at the moment
        //
        // @todo: allow fixed function to be configured
        // @todo: allow render target setup to be configured
        // @todo: parse pipeline layout from the spir-v source
        //
        VkDescriptorSetLayoutBinding bindings[3] = {};

        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        bindings[2].binding         = 2;
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo create_info = {};
        create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        create_info.bindingCount = 3;
        create_info.pBindings    = bindings;

        VK_CHECK(vk->CreateDescriptorSetLayout(device->handle, &create_info, 0, &pipeline.set_layout));
    }

    {
        assert(sizeof(R_Setup) == 128);

        VkPushConstantRange push_constant_range = { 0 };
        push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constant_range.offset     = 0;
        push_constant_range.size       = sizeof(R_Setup);

        VkPipelineLayoutCreateInfo create_info = {};
        create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        create_info.setLayoutCount         = 1;
        create_info.pSetLayouts            = &pipeline.set_layout;
        create_info.pushConstantRangeCount = 1;
        create_info.pPushConstantRanges    = &push_constant_range;

        VK_CHECK(vk->CreatePipelineLayout(device->handle, &create_info, 0, &pipeline.layout));
    }

    {
        Str8 code = FileReadAll("shaders/basic.vert.spv");

        VkShaderModuleCreateInfo create_info = {};
        create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.count;
        create_info.pCode    = (U32 *) code.data;

        VK_CHECK(vk->CreateShaderModule(device->handle, &create_info, 0, &pipeline.vs));

        free(code.data);

        code = FileReadAll("shaders/basic.frag.spv");

        create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.count;
        create_info.pCode    = (U32 *) code.data;

        VK_CHECK(vk->CreateShaderModule(device->handle, &create_info, 0, &pipeline.fs));

        free(code.data);
    }

    {
        // fixed functions stuff + render attachment setup
        //

        VkPipelineShaderStageCreateInfo stages[2] = {};

        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = pipeline.vs;
        stages[0].pName  = "main";

        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = pipeline.fs;
        stages[1].pName  = "main";

        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo ia = {};
        ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vp = {};
        vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.viewportCount = 1;
        vp.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo rs = {};
        rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.frontFace   = VK_FRONT_FACE_CLOCKWISE;
        rs.cullMode    = VK_CULL_MODE_BACK_BIT;
        rs.lineWidth   = 1.35f;

        VkPipelineMultisampleStateCreateInfo ms = {};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo ds = {};
        ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable  = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp   = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState blend_attachment = {};
        blend_attachment.blendEnable    = VK_FALSE; // @todo: add blending
        blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo om = {};
        om.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        om.attachmentCount = 1;
        om.pAttachments    = &blend_attachment;

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state = {};
        dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = 2;
        dynamic_state.pDynamicStates    = dynamic_states;

        // render attachment info
        //
        VkFormat format = swapchain->surface.format.format;

        VkPipelineRenderingCreateInfo rendering_info = {};
        rendering_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_info.colorAttachmentCount    = 1;
        rendering_info.pColorAttachmentFormats = &format;
        rendering_info.depthAttachmentFormat   = VK_FORMAT_D32_SFLOAT;

        VkGraphicsPipelineCreateInfo create_info = {};
        create_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        create_info.pNext               = &rendering_info;
        create_info.stageCount          = 2;
        create_info.pStages             = stages;
        create_info.pVertexInputState   = &vi;
        create_info.pInputAssemblyState = &ia;
        create_info.pViewportState      = &vp;
        create_info.pRasterizationState = &rs;
        create_info.pMultisampleState   = &ms;
        create_info.pDepthStencilState  = &ds;
        create_info.pColorBlendState    = &om;
        create_info.pDynamicState       = &dynamic_state;
        create_info.layout              = pipeline.layout;

        VK_CHECK(vk->CreateGraphicsPipelines(device->handle, 0, 1, &create_info, 0, &pipeline.handle));
    }

    // for vertex data
    //
    // @todo: staging buffer!
    //

    VK_Buffer vb = {};
    vb.size        = mesh.num_vertices * sizeof(SkinnedVertex3);
    vb.host_mapped = true;
    vb.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VK_BufferCreate(device, &vb);

    memcpy(vb.data, mesh.skinned_vertices, mesh.num_vertices * sizeof(SkinnedVertex3));

    VK_Buffer ib = {};
    ib.size        = mesh.num_indices * sizeof(U16);
    ib.host_mapped = true;
    ib.usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    VK_BufferCreate(device, &ib);

    memcpy(ib.data, mesh.indices, mesh.num_indices * sizeof(U16));

    VK_Buffer bb = {};
    bb.size        = skeleton.num_bones * sizeof(Mat4x4F);
    bb.host_mapped = true;
    bb.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VK_BufferCreate(device, &bb);

    VK_Buffer mb = {};
    mb.size        = mesh.num_materials * (sizeof(A_Material) - 16); // don't want string name @hack:!!!!
    mb.host_mapped = true;
    mb.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VK_BufferCreate(device, &mb);

    {
        // @hack: fix this to be less hacky, we take of the string name from the entire material size,
        // we should just have a material type for rendering specifically which only has the parameters
        // we need. some of the material properties will come from textures so don't need to be stored in the
        // material, but instead a texture index needs to be there for lookup. we need to sort out bindless
        // textures for that
        //
        U64 stride = sizeof(A_Material);
        U64 material_size = stride - 16;

        U8 *from = (U8 *) mesh.materials;
        U8 *to   = (U8 *) mb.data;

        for (U32 it = 0; it < mesh.num_materials; ++it) {
            memcpy(to, from, material_size);

            from += stride;
            to   += material_size;
        }
    }

    B32 running = true;

    // camera @todo: make this a parameterized structure
    Vec3F p = { 0, 8, 0 };

    B32 w = false, s = false, a = false, d = false;
    B32 space = false, lshift = false;

    F32 pitch = 0, yaw = 0;

    U32 index = 0;
    U32 animation_index = 0;

    // for timing
    F32 delta_time = 0;
    F32 total_time = 0;
    U64 start = U64TicksGet();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = false; }
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_w: { w = true; } break;
                    case SDLK_s: { s = true; } break;
                    case SDLK_a: { a = true; } break;
                    case SDLK_d: { d = true; } break;
                    case SDLK_LSHIFT: { lshift = true; } break;
                    case SDLK_SPACE: { space = true; } break;
                    case SDLK_ESCAPE: { SDL_SetRelativeMouseMode(SDL_FALSE); } break;
                    case SDLK_f: {
                        index += 1;
                        if (index >= skeleton.animations[animation_index].num_frames) {
                            index = 0;
                        }
                    }
                    break;
                    case SDLK_n: {
                        animation_index += 1;
                        index = 0;

                        if (animation_index >= skeleton.num_animations) {
                            animation_index = 0;
                        }
                    }
                    break;
                    case SDLK_t: {
                        if (e.key.keysym.mod & KMOD_LCTRL) {
                            skeleton.animations[animation_index].time_scale *= 0.5f;
                        }
                        else {
                            skeleton.animations[animation_index].time_scale *= 2.0f;
                        }
                    }
                    break;
                }
            }
            else if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                    case SDLK_w: { w = false; } break;
                    case SDLK_s: { s = false; } break;
                    case SDLK_a: { a = false; } break;
                    case SDLK_d: { d = false; } break;
                    case SDLK_LSHIFT: { lshift = false; } break;
                    case SDLK_SPACE: { space = false; } break;
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
            }
            else if (e.type == SDL_MOUSEMOTION) {
                #define MOUSE_SENSITIVITY 0.075f

                if (SDL_GetRelativeMouseMode()) {
                    yaw   += MOUSE_SENSITIVITY * delta_time * e.motion.xrel;
                    pitch += MOUSE_SENSITIVITY * delta_time * e.motion.yrel;
                }

                if (pitch < M_PI) { pitch = (F32) M_PI; }
                if (pitch > (2 * M_PI)) { pitch = 2.0f * (F32) M_PI; }
            }
            else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    // :note must get the unscaled coordinates, the width and height provided
                    // by the event are pre-divided so are too small for higher scale factors
                    //
                    S32 ww, wh;
                    SDL_Vulkan_GetDrawableSize(window, &ww, &wh);

                    swapchain->surface.width  = ww;
                    swapchain->surface.height = wh;

                    VK_SwapchainCreate(device, swapchain);
                }
                else if (e.window.event == SDL_WINDOWEVENT_SHOWN) {
                    // :note in unscaled sizes
                    //
                    SDL_SetWindowMinimumSize(window, 640, 360);
                    SDL_SetWindowMaximumSize(window, 1920, 1080);
                }
                else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    // The hoops you have to jump through to make a window in wayland that is forced
                    // into floating mode but also is allowed to be resized
                    //
                    SDL_SetWindowResizable(window, SDL_TRUE);
                }
            }
        }

        VK_Frame *frame = VK_NextFrameAcquire(device, swapchain);
        if (!frame) {
            printf("[FATAL] :: failed to acquire vulkan frame\n");
            break;
        }

        Mat4x4F prot = M4x4FRotationX(pitch);
        Mat4x4F yrot = M4x4FRotationZ(yaw);

        Mat4x4F rot = M4x4FMul(yrot, prot);

        // the local axes of the camera transform
        //
        Vec3F x = M4x4FColumnExtract(rot, 0);
        Vec3F y = M4x4FColumnExtract(rot, 1);
        Vec3F z = M4x4FColumnExtract(rot, 2);

#define MOVE_SPEED 4.7f

        if (w) { p = V3FAdd(p, V3FMulF32(z, -MOVE_SPEED * delta_time)); }
        else if (s) { p = V3FAdd(p, V3FMulF32(z, MOVE_SPEED * delta_time)); }

        if (space) { p = V3FAdd(p, V3FMulF32(y, -MOVE_SPEED * delta_time)); }
        else if (lshift) { p = V3FAdd(p, V3FMulF32(y, MOVE_SPEED* delta_time)); }

        if (a) { p = V3FAdd(p, V3FMulF32(x, -MOVE_SPEED * delta_time)); }
        else if (d) { p = V3FAdd(p, V3FMulF32(x, MOVE_SPEED * delta_time)); }

        F32 aspect = (F32) swapchain->surface.width / (F32) swapchain->surface.height;

        Mat4x4FInv proj = M4x4FPerspectiveProjection(2.1445069205f, aspect, 0.01f, 1000.0f);
        Mat4x4FInv view = M4x4FCameraViewProjection(x, y, z, p);

        R_Setup setup;

        setup.view_proj     = M4x4FMul(proj.fwd, view.fwd);
        setup.view_p        = p;
        setup.time          = total_time;
        setup.dt            = delta_time;
        setup.window_width  = swapchain->surface.width;
        setup.window_height = swapchain->surface.height;

        // Prepare animation ....
        //
        TempArena temp;
        DeferLoop(temp = TempGet(0, 0), TempRelease(&temp)) {
            // @todo: would love to calculate these directly into the mapped buffer, however,
            // mapped gpu memory may be in write-combined memory and you _really_ don't want
            // to be reading from that. as the calculations require lookups of the parent samples
            // this is a no go
            //
            Mat4x4F  *bone_matrices = ArenaPush(temp.arena, Mat4x4F,  skeleton.num_bones);
            A_Sample *samples       = ArenaPush(temp.arena, A_Sample, skeleton.num_bones);

            A_AnimationEvaluate(samples, &skeleton, animation_index, delta_time);
            A_AnimationBoneMatricesGet(bone_matrices, &skeleton, samples);

            memcpy(bb.data, bone_matrices, skeleton.num_bones * sizeof(Mat4x4F));
        }

        VkCommandBuffer cmds = VK_CommandBufferPush(vk, frame);

        // transition swapchain image to colour output optimal
        //
        VkImageMemoryBarrier2 image_barrier = {};
        image_barrier.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        image_barrier.srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        image_barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        image_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        image_barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        image_barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        image_barrier.image         = swapchain->images.handles[frame->image_index];

        image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_barrier.subresourceRange.levelCount = 1;
        image_barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dependency_info = {};
        dependency_info.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers    = &image_barrier;

        vk->CmdPipelineBarrier2(cmds, &dependency_info);

        // begin rendering
        //
        VkRenderingAttachmentInfo colour_attachment = {};
        colour_attachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colour_attachment.imageView   = swapchain->images.views[frame->image_index];
        colour_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colour_attachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colour_attachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

        colour_attachment.clearValue.color.float32[0] = 0.15f;
        colour_attachment.clearValue.color.float32[1] = 0.15f;
        colour_attachment.clearValue.color.float32[2] = 0.15f;
        colour_attachment.clearValue.color.float32[3] = 1.0f;

        VkRenderingAttachmentInfo depth_attachment = {};
        depth_attachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView   = swapchain->depth.view;
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

        depth_attachment.clearValue.depthStencil.depth = 1.0f;

        VkRenderingInfo rendering_info = {};
        rendering_info.sType                    = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.extent.width  = swapchain->surface.width;
        rendering_info.renderArea.extent.height = swapchain->surface.height;
        rendering_info.layerCount               = 1;
        rendering_info.colorAttachmentCount     = 1;
        rendering_info.pColorAttachments        = &colour_attachment;
        rendering_info.pDepthAttachment         = &depth_attachment;

        vk->CmdBeginRendering(cmds, &rendering_info);

        vk->CmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);

        VkViewport viewport = {};
        viewport.width    = (F32) swapchain->surface.width;
        viewport.height   = (F32) swapchain->surface.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.extent.width  = (U32) viewport.width;
        scissor.extent.height = (U32) viewport.height;

        vk->CmdSetViewport(cmds, 0, 1, &viewport);
        vk->CmdSetScissor(cmds, 0, 1, &scissor);

        vk->CmdBindIndexBuffer(cmds, ib.handle, 0, VK_INDEX_TYPE_UINT16);

        // push new descriptor for the vertex data
        //
        VkDescriptorSet set;
        {
            VkDescriptorSetAllocateInfo alloc_info = {};
            alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool     = frame->descriptor_pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts        = &pipeline.set_layout;

            VK_CHECK(vk->AllocateDescriptorSets(device->handle, &alloc_info, &set));
        }

        {
            VkDescriptorBufferInfo buffer_infos[3] = {};

            buffer_infos[0].buffer = vb.handle;
            buffer_infos[0].offset = 0;
            buffer_infos[0].range  = VK_WHOLE_SIZE;

            buffer_infos[1].buffer = bb.handle;
            buffer_infos[1].offset = 0;
            buffer_infos[1].range  = VK_WHOLE_SIZE;

            buffer_infos[2].buffer = mb.handle;
            buffer_infos[2].offset = 0;
            buffer_infos[2].range  = VK_WHOLE_SIZE;

            VkWriteDescriptorSet writes[4] = {};

            writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet          = set;
            writes[0].dstBinding      = 0;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[0].pBufferInfo     = &buffer_infos[0];

            writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet          = set;
            writes[1].dstBinding      = 1;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[1].pBufferInfo     = &buffer_infos[1];

            writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet          = set;
            writes[2].dstBinding      = 2;
            writes[2].dstArrayElement = 0;
            writes[2].descriptorCount = 1;
            writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[2].pBufferInfo     = &buffer_infos[2];

            vk->UpdateDescriptorSets(device->handle, 3, writes, 0, 0);
        }

        vk->CmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1, &set, 0, 0);
        vk->CmdPushConstants(cmds, pipeline.layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(R_Setup), &setup);

        vk->CmdDrawIndexed(cmds, mesh.num_indices, 1, 0, 0, 0);

        // end rendering
        //
        vk->CmdEndRendering(cmds);

        // transition image to present src optimal
        //
        // @todo: gather both of these transitions into a single dependency info, we can issue them together
        //
        image_barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        image_barrier.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        image_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        image_barrier.dstAccessMask = 0;
        image_barrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        image_barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        vk->CmdPipelineBarrier2(cmds, &dependency_info);

        vk->EndCommandBuffer(cmds);

        VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit_info = {};
        submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount   = 1;
        submit_info.pWaitSemaphores      = &frame->acquire;
        submit_info.pWaitDstStageMask    = &wait_stages;
        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &cmds;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &frame->render;

        VK_CHECK(vk->QueueSubmit(device->graphics_queue.handle, 1, &submit_info, frame->fence));

        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = &frame->render;
        present_info.swapchainCount     = 1;
        present_info.pSwapchains        = &swapchain->handle;
        present_info.pImageIndices      = &frame->image_index;

        VkResult success = vk->QueuePresentKHR(device->graphics_queue.handle, &present_info);
        if (success == VK_ERROR_OUT_OF_DATE_KHR) {
            VK_SwapchainCreate(device, swapchain);
        }
        else if (success == VK_ERROR_SURFACE_LOST_KHR) {
            // :surface_lost
            //
            vk->DeviceWaitIdle(device->handle);

            for (U32 it = 0; it < swapchain->images.count; ++it) {
                vk->DestroyImageView(device->handle, swapchain->images.views[it], 0);
            }

            vk->DestroySwapchainKHR(device->handle, swapchain->handle, 0);
            vk->DestroySurfaceKHR(vk->instance, swapchain->surface.handle, 0);

            swapchain->surface.handle = VK_NULL_HANDLE;
            swapchain->handle         = VK_NULL_HANDLE;

            VK_SwapchainCreate(device, swapchain);
        }
        else if (success == VK_SUBOPTIMAL_KHR) {
            // :suboptimal_present
            //
            VkSurfaceCapabilitiesKHR surface_caps;
            VK_CHECK(vk->GetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical, swapchain->surface.handle, &surface_caps));

            if (swapchain->surface.width  != surface_caps.currentExtent.width ||
                swapchain->surface.height != surface_caps.currentExtent.height)
            {
                VK_SwapchainCreate(device, swapchain);
            }
        }
        else {
            assert(success == VK_SUCCESS);
        }

        U64 end = U64TicksGet();
        delta_time = (F32) F64ElapsedTimeGet(start, end);
        delta_time = Clamp(0, delta_time, 0.2f);

        total_time += delta_time;

        start = end;
    }

    return 0;
}

// animation.c
//
Mat4x4F A_SampleToM4x4F(A_Sample *sample) {
    // @speed: simd?
    //
    Mat4x4F result = Q4FToM4x4F(sample->orientation);

    result.m[0][0] *= sample->scale.x;
    result.m[0][1] *= sample->scale.x;
    result.m[0][2] *= sample->scale.x;

    result.m[1][0] *= sample->scale.y;
    result.m[1][1] *= sample->scale.y;
    result.m[1][2] *= sample->scale.y;

    result.m[2][0] *= sample->scale.z;
    result.m[2][1] *= sample->scale.z;
    result.m[1][2] *= sample->scale.z;

    result = M4x4FTranslateV3F(result, sample->position);

    return result;
}

A_Sample A_SampleLerp(A_Sample *a, A_Sample *b, F32 t) {
    A_Sample result;

    result.position = V3FLerp(a->position, b->position, t);
    result.scale    = V3FLerp(a->scale,    b->scale,    t);

    if (Q4FDot(a->orientation, b->orientation) < 0) {
        // double cover
        //
        // @todo: we need to properly interpret the mode and dot with the rest pose orientation
        // instead this allows for order-indpendent blending
        //
        result.orientation = Q4FNLerp(a->orientation, Q4FNeg(b->orientation), t);
    }
    else {
        result.orientation = Q4FNLerp(a->orientation, b->orientation, t);
    }

    return result;
}

A_Sample *A_AnimationSamplesForFrame(A_Animation *animation, U32 num_bones, U32 frame_index) {
    A_Sample *result = &animation->samples[num_bones * frame_index];
    return result;
}

void A_AnimationEvaluate(A_Sample *output_samples, A_Skeleton *skeleton, U32 animation_index, F32 dt) {
    Assert(animation_index < skeleton->num_animations);

    A_Animation *animation = &skeleton->animations[animation_index];

    F32 framerate     = cast(F32) skeleton->framerate;
    F32 inv_framerate = 1.0f / framerate;

    F32 total_time   = inv_framerate * animation->num_frames;
    animation->time += (animation->time_scale * dt);

    if (animation->time >= total_time) {
        animation->time -= cast(U32) (animation->time / total_time) * total_time;
    }

    // @todo: might want to have a flag on the animations to see whether it loops or should stop on the
    // final frame
    //
    U32 frame_index0 = (U32) ((animation->time / total_time) * animation->num_frames);
    U32 frame_index1 = (frame_index0 + 1) % animation->num_frames;

    // @todo: is this t calculation correct?
    //
    F32 t = (animation->time - (inv_framerate * frame_index0)) * framerate;

    A_Sample *frame0 = A_AnimationSamplesForFrame(animation, skeleton->num_bones, frame_index0);
    A_Sample *frame1 = A_AnimationSamplesForFrame(animation, skeleton->num_bones, frame_index1);

    for (U32 it = 0; it < skeleton->num_bones; ++it) {
        output_samples[it] = A_SampleLerp(&frame0[it], &frame1[it], t);
    }
}

void A_AnimationBoneMatricesGet(Mat4x4F *output_matrices, A_Skeleton *skeleton, A_Sample *samples) {
    for (U32 it = 0; it < skeleton->num_bones; ++it) {
        A_Bone *bone = &skeleton->bones[it];

        if (bone->parent_index == 0xFF) {
            // root bone
            //
            output_matrices[it] = A_SampleToM4x4F(&samples[it]);
        }
        else {
            assert(bone->parent_index < it);

            Mat4x4F transform   = A_SampleToM4x4F(&samples[it]);
            output_matrices[it] = M4x4FMul(output_matrices[bone->parent_index], transform);
        }
    }

    // we must do this in a second pass becuase the original sample matrix may be used by
    // child bones to calculate their final transforms
    //
    for (U32 it = 0; it < skeleton->num_bones; ++it) {
        A_Bone *bone = &skeleton->bones[it];
        output_matrices[it] = M4x4FMul(output_matrices[it], bone->inv_bind_pose);
    }
}

#include "file_formats.c"
