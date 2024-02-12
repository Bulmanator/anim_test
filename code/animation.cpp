#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_syswm.h>

#define STBI_ONLY_PNG 1
#define STB_IMAGE_IMPLEMENTATION 1
#include <stb_image.h>

#define CORE_IMPL 1
#include "core.h"

#include "math.cpp"

#include "file_formats.h"
#include "animation.h"

#include "render.h"

static void *zmalloc(size_t size) {
    // @todo: remove this, I think the only thing that still uses this is vulkan device allocation
    //
    void *result = malloc(size);
    memset(result, 0, size);

    return result;
}

#include "vulkan.h"
#include "vulkan.cpp"

Function B32 MeshFileLoad(Arena *arena, A_Mesh *mesh, Str8 path) {
    B32 result = false;

    TempArena temp = TempGet(1, &arena);

    char *zpath = ArenaPush(temp.arena, char, path.count + 1);
    memcpy(zpath, path.data, path.count);

    FILE *handle = fopen(zpath, "rb");
    if (handle) {
        Str8 data;

        fseek(handle, 0, SEEK_END);
        data.count = ftell(handle);
        fseek(handle, 0, SEEK_SET);

        data.data = ArenaPush(temp.arena, U8, data.count, ARENA_FLAG_NO_ZERO);

        fread(data.data, data.count, 1, handle);
        fclose(handle);

        AMTM_Mesh amtm = { 0 };
        AMTM_MeshFromData(temp.arena, &amtm, data);

        // Copy the string table
        //
        mesh->string_table.count = amtm.string_table.count;
        mesh->string_table.data  = ArenaPushCopy(arena, amtm.string_table.data, U8, mesh->string_table.count);

        mesh->num_textures  = amtm.num_textures;
        mesh->num_submeshes = amtm.num_submeshes;
        mesh->num_materials = amtm.num_materials;

        // Gather submesh information
        //
        // @todo: for now this just makes a copy of the vertex data and index data and stores it on the
        // submesh, we should pass in a staging buffer and upload this stuff directly!
        //
        mesh->submeshes = ArenaPush(arena, A_Submesh, mesh->num_submeshes);

        // running totals for base offsets
        //
        U32 total_vertices  = 0;
        U32 total_indices   = 0;

        for (U32 it = 0; it < mesh->num_submeshes; ++it) {
            A_Submesh    *dst = &mesh->submeshes[it];
            AMTM_Submesh *src = &amtm.submeshes[it];

            dst->name.count = src->info->name_count;
            dst->name.data  = &mesh->string_table.data[src->info->name_offset];

            dst->flags = src->info->flags;

            dst->base_vertex = total_vertices;
            dst->base_index  = total_indices;

            dst->num_vertices = src->info->num_vertices;
            dst->num_indices  = src->info->num_indices;

            dst->indices = ArenaPushCopy(arena, src->indices, U16, dst->num_indices);

            // @todo: come up with a more coherent way to copy the vertices, R_SkinnedVertex3 is a the
            // same as R_Vertex3 but with extra data, this method can be error-prone in the event
            // the vertex layout changes
            //
            if (dst->flags & AMTM_MESH_FLAG_IS_SKINNED) {
                R_SkinnedVertex3   *to_vertices   = ArenaPush(arena, R_SkinnedVertex3, dst->num_vertices);
                AMTM_SkinnedVertex *from_vertices = src->skinned_vertices;

                for (U32 v = 0; v < dst->num_vertices; ++v) {
                    R_SkinnedVertex3   *to   = &to_vertices[v];
                    AMTM_SkinnedVertex *from = &from_vertices[v];

                    to->position.x = from->position[0];
                    to->position.y = from->position[1];
                    to->position.z = from->position[2];

                    to->uv[0] = cast(U16) (U16_MAX * from->uv[0]);
                    to->uv[1] = cast(U16) (U16_MAX * from->uv[1]);

                    to->normal[0] = cast(U8) ((from->normal[0] * 127.0f) + 127.5f);
                    to->normal[1] = cast(U8) ((from->normal[1] * 127.0f) + 127.5f);
                    to->normal[2] = cast(U8) ((from->normal[2] * 127.0f) + 127.5f);
                    to->normal[3] = 254; // 1.0, unused in shader only for padding

                    // @todo: when we start loading multiple meshes materials will be compacted into a
                    // single buffer on the gpu, this means the material_index will have to be re-based to
                    // the current offset in that buffer
                    //
                    // :material_base
                    //
                    to->material_index = from->material_index;

                    to->bone_indices[0] = from->bone_indices[0];
                    to->bone_indices[1] = from->bone_indices[1];
                    to->bone_indices[2] = from->bone_indices[2];
                    to->bone_indices[3] = from->bone_indices[3];

                    to->bone_weights[0] = cast(U8) (U8_MAX * from->bone_weights[0]);
                    to->bone_weights[1] = cast(U8) (U8_MAX * from->bone_weights[1]);
                    to->bone_weights[2] = cast(U8) (U8_MAX * from->bone_weights[2]);
                    to->bone_weights[3] = cast(U8) (U8_MAX * from->bone_weights[3]);
                }

                dst->vertices = cast(void *) to_vertices;
            }
            else {
                R_Vertex3   *to_vertices   = ArenaPush(arena, R_Vertex3, dst->num_vertices);
                AMTM_Vertex *from_vertices = src->vertices;

                for (U32 v = 0; v < dst->num_vertices; ++v) {
                    R_Vertex3   *to   = &to_vertices[v];
                    AMTM_Vertex *from = &from_vertices[v];

                    to->position.x = from->position[0];
                    to->position.y = from->position[1];
                    to->position.z = from->position[2];

                    to->uv[0] = cast(U16) (U16_MAX * from->uv[0]);
                    to->uv[1] = cast(U16) (U16_MAX * from->uv[1]);

                    to->normal[0] = cast(U8) ((from->normal[0] * 127.0f) + 127.5f);
                    to->normal[1] = cast(U8) ((from->normal[1] * 127.0f) + 127.5f);
                    to->normal[2] = cast(U8) ((from->normal[2] * 127.0f) + 127.5f);
                    to->normal[3] = 254; // 1.0, unused in shader only for padding

                    // :material_base
                    //
                    to->material_index = from->material_index;
                }

                dst->vertices = cast(void *) to_vertices;
            }

            total_vertices += dst->num_vertices;
            total_indices  += dst->num_indices;
        }

        // Gather material data
        //
        mesh->materials = ArenaPush(arena, A_Material, mesh->num_materials);

        for (U32 it = 0; it < mesh->num_materials; ++it) {
            A_Material    *dst = &mesh->materials[it];
            AMTM_Material *src = &amtm.materials[it];

            dst->colour = src->colour;

            dst->roughness = src->properties[AMTM_MATERIAL_PROPERTY_TYPE_ROUGHNESS];
            dst->metallic  = src->properties[AMTM_MATERIAL_PROPERTY_TYPE_METALLIC];
            dst->ior       = src->properties[AMTM_MATERIAL_PROPERTY_TYPE_IOR];

            dst->anisotropic          = src->properties[AMTM_MATERIAL_PROPERTY_TYPE_ANISOTROPIC];
            dst->anisotropic_rotation = src->properties[AMTM_MATERIAL_PROPERTY_TYPE_ANISOTROPIC_ROTATION];

            dst->clear_coat           = src->properties[AMTM_MATERIAL_PROPERTY_TYPE_COAT_WEIGHT];
            dst->clear_coat_roughness = src->properties[AMTM_MATERIAL_PROPERTY_TYPE_COAT_ROUGHNESS];

            dst->sheen           = src->properties[AMTM_MATERIAL_PROPERTY_TYPE_SHEEN_WEIGHT];
            dst->sheen_roughness = src->properties[AMTM_MATERIAL_PROPERTY_TYPE_SHEEN_ROUGHNESS];

            U32 texture = src->textures[AMTM_MATERIAL_TEXTURE_TYPE_BASE_COLOUR];
            Assert(texture != U32_MAX); // unused if this is the case, we require base colour texture

            U32 channels = texture >> AMTM_TEXTURE_CHANNELS_SHIFT;
            U32 index    = texture &  AMTM_TEXTURE_INDEX_MASK;

            Assert(channels == 0xF); // all RGBA channels used

            dst->albedo_index = index;
        }

        // Gather texture data
        //
        // :note we do actually load all textures even though only base colour is used, testing!
        //
        mesh->textures = ArenaPush(arena, A_Texture, mesh->num_textures);

        for (U32 it = 0; it < mesh->num_textures; ++it) {
            A_Texture    *dst = &mesh->textures[it];
            AMTM_Texture *src = &amtm.textures[it];

            Str8 name;
            name.count = src->name_count;
            name.data  = &mesh->string_table.data[src->name_offset];

            dst->name = name;

            char image_path[1024];
            snprintf(image_path, sizeof(image_path), "textures/%.*s.png", cast(U32) name.count, name.data);

            // load the image data
            //
            int w, h, c;
            U8 *pixels = stbi_load(image_path, &w, &h, &c, 4);

            Assert(pixels != 0);

            dst->width  = w;
            dst->height = h;
            dst->pixels = pixels;
        }

        result = true;
    }

    return result;
}

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

static Str8 FileReadAll(Arena *arena, const char *path) {
    Str8 result = {};

    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        result.count = ftell(f);
        fseek(f, 0, SEEK_SET);

        result.data = ArenaPush(arena, U8, result.count, ARENA_FLAG_NO_ZERO);

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

    const char *mesh_path = "../test/Mako/Characters_Mako.amtm";
    const char *skel_path = "../test/Mako/Characters_Mako.amts";
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
        VK_PipelineState *state = &pipeline.state;

        state->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        state->cull_mode    = VK_CULL_MODE_BACK_BIT;
        state->polygon_mode = VK_POLYGON_MODE_FILL;
        state->front_face   = VK_FRONT_FACE_CLOCKWISE;

        state->depth_test       = VK_TRUE;
        state->depth_write      = VK_TRUE;
        state->depth_compare_op = VK_COMPARE_OP_LESS;

        // render target info
        //
        pipeline.num_targets       = 1;
        pipeline.target_formats[0] = swapchain->surface.format.format;
        pipeline.depth_format      = VK_FORMAT_D32_SFLOAT;

        TempArena temp = TempGet(0, 0);

        Str8 vert_code = FileReadAll(temp.arena, "shaders/basic.vert.spv");
        Str8 frag_code = FileReadAll(temp.arena, "shaders/basic.frag.spv");

        pipeline.num_shaders = 2;

        VK_ShaderCreate(device, &pipeline.shaders[0], vert_code);
        VK_ShaderCreate(device, &pipeline.shaders[1], frag_code);

        VK_PipelineCreate(device, &pipeline);

        TempRelease(&temp);
    }

    // for vertex data
    //
    // @todo: staging buffer!
    //

    VK_Buffer vb = {};
    vb.size        = MB(64);
    vb.host_mapped = true;
    vb.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VK_Buffer ib = {};
    ib.size        = MB(64);
    ib.host_mapped = true;
    ib.usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    VK_BufferCreate(device, &vb);
    VK_BufferCreate(device, &ib);

    {
        // Only copy first submesh for now
        //
        A_Submesh *submesh = &mesh.submeshes[0];
        Assert(submesh->flags & AMTM_MESH_FLAG_IS_SKINNED);

        memcpy(vb.data, submesh->vertices, submesh->num_vertices * sizeof(R_SkinnedVertex3));
        memcpy(ib.data, submesh->indices,  submesh->num_indices  * sizeof(U16));
    }

    VK_Buffer bb = {};
    bb.size        = skeleton.num_bones * sizeof(Mat4x4F);
    bb.host_mapped = true;
    bb.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VK_BufferCreate(device, &bb);

    VK_Buffer mb = {};
    mb.size        = mesh.num_materials * sizeof(R_Material);
    mb.host_mapped = true;
    mb.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VK_BufferCreate(device, &mb);

    {
        R_Material *materials = cast(R_Material *) mb.data;

        for (U32 it = 0; it < mesh.num_materials; ++it) {
            R_Material *dst = &materials[it];
            A_Material *src = &mesh.materials[it];

            dst->colour    = src->colour;
            dst->metallic  = src->metallic;
            dst->roughness = src->roughness;
            dst->ior       = src->ior;
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
            alloc_info.pSetLayouts        = &pipeline.layout.set;

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

        vk->CmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline.layout.pipeline, 0, 1, &set, 0, 0);

        vk->CmdPushConstants(cmds, pipeline.layout.pipeline,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(R_Setup), &setup);

        {
            A_Submesh *submesh = &mesh.submeshes[0];

            vk->CmdDrawIndexed(cmds, submesh->num_indices, 1, 0, 0, 0);
        }

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
            Assert(bone->parent_index < it);

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
