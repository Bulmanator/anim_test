#if !defined(ANIMATION_VULKAN_H_)
#define ANIMATION_VULKAN_H_

#if OS_WINDOWS
    #define VK_USE_PLATFORM_WIN32_KHR   1
#elif OS_LINUX
    #define VK_USE_PLATFORM_WAYLAND_KHR 1
#endif

#define VK_NO_PROTOTYPES 1
#include <vulkan/vulkan.h>

#include <spirv-headers/spirv.h>

#define VK_CHECK(x) assert((x) == VK_SUCCESS)

#define VK_FRAME_COUNT     2 // number of processing frames in flight

#define VK_IMAGE_COUNT     3 // number of images on the swapchain (triple buffer)
#define VK_MAX_IMAGE_COUNT 8 // maximum number of images a swapchain can have

#define VK_STAGING_BUFFER_SIZE MB(64)

struct VK_Context;
struct VK_Device;

typedef U32 VK_ContextFlags;
enum {
    VK_CONTEXT_FLAG_DEBUG = (1 << 0),
};

struct VK_Queue {
    U32 family;
    VkQueue handle;
};

#define VK_COMMAND_BUFFER_SET_COUNT 8

struct VK_CommandBufferSet {
    U32 next_buffer;
    VkCommandBuffer handles[VK_COMMAND_BUFFER_SET_COUNT];

    VK_CommandBufferSet *next;
};

struct VK_Frame {
    VkCommandPool command_pool;

    VK_CommandBufferSet cmds_first; // head reset to on pool reset
    VK_CommandBufferSet *cmds;      // place to allocate next command buffer from

    VkDescriptorPool descriptor_pool;

    VkSemaphore acquire; // signalled when swapchain image has been acquired, wait on before rendering
    VkSemaphore render;  // signalled when rendering has complete, wait on before presenting

    VkFence fence;

    U32 image_index; // index into the swapchain images array

    VK_Frame *next;
};

struct VK_Buffer {
    VkBuffer handle;
    VkDeviceMemory memory;

    B32 host_mapped;
    VkBufferUsageFlags usage;

    U64   offset;
    U64   size;
    U64   alignment;
    void *data;
};

struct VK_Device {
    VkDevice handle;
    VkPhysicalDevice physical;

    VkPhysicalDeviceProperties       properties;
    VkPhysicalDeviceFeatures         features;
    VkPhysicalDeviceMemoryProperties memory_properties;

    // assumes present is also available on the same queue, this is the case
    // across the board for all gpu vendors
    //
    VK_Queue graphics_queue;

    VkCommandPool scratch_cmd_pool; // for quick commands that only need to be submitted once
    VK_Buffer staging_buffer;

    VK_Frame frames[VK_FRAME_COUNT];
    VK_Frame *frame;

    VK_Context *vk;
    VK_Device  *next;
};

struct VK_Context {
#if OS_WINDOWS
    HMODULE lib;
#elif OS_LINUX
    void *lib;
#endif

    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;

    VK_ContextFlags flags;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;

    VK_Device *devices; // list of all available devices
    VK_Device *device;  // selected device, is initialised for use

    #define VK_DYN_FUNCTION(x) PFN_vk##x x
        #define VK_GLOBAL_FUNCTIONS
        #define VK_DEBUG_FUNCTIONS
        #define VK_INSTANCE_FUNCTIONS
        #define VK_DEVICE_FUNCTIONS
        #include "vulkan_dyn_functions.cpp"
    #undef VK_DYN_FUNCTION
};

struct VK_Swapchain {
    struct {
        VkSurfaceKHR handle;
        VkSurfaceFormatKHR format;

        union {
#if OS_WINDOWS
            struct {
                HINSTANCE hinstance;
                HWND hwnd;
            } win;
#elif OS_LINUX
            struct {
                struct wl_display *display;
                struct wl_surface *surface;
            } wl;
#endif
        };

        U32 width;
        U32 height;
    } surface;

    VkSwapchainKHR handle;

    // :note this is a supported present mode allowing vsync to be disabled, FIFO is vsync enabled and
    // is mandated by spec to be supported. if there are no other present modes available then this will
    // just be set to FIFO and will have no effect
    //
    VkPresentModeKHR vsync_disable;
    B32 vsync; // whether vsync is on or off, @incomplete: we have to rebuild the swapchain if this changes!

    struct {
        U32 count;
        VkImage handles[VK_MAX_IMAGE_COUNT];
        VkImageView views[VK_MAX_IMAGE_COUNT];
    } images;

    struct {
        VkDeviceMemory memory;

        VkImage     image;
        VkImageView view;
    } depth;
};

struct VK_Image {
    VkImage     handle;
    VkImageView view;

    VkDeviceMemory memory;

    U32 width;
    U32 height;

    VkFormat format;

    VkImageLayout     layout;
    VkImageUsageFlags usage;

    VkImageAspectFlags aspect_mask;
};

struct VK_PipelineState {
    VkPrimitiveTopology topology;

    VkCullModeFlags cull_mode;
    VkPolygonMode   polygon_mode;
    VkFrontFace     front_face;

    VkBool32    depth_test;
    VkBool32    depth_write;
    VkCompareOp depth_compare_op;
};

struct VK_DescriptorInfo {
    VkDescriptorType   type;
    VkShaderStageFlags stages;
};

struct VK_Shader {
    VkShaderModule handle;

    VkShaderStageFlags stage;

    // @todo: we currently assume that the entrypoint of every shader is just "main", the
    // spir-v actually tells us this and we parse it anyway when getting the execution model so maybe
    // copy it and use it here
    //
    // :shader_entry
    //

    B16 has_push_constants;
    U16 resource_mask;
    VkDescriptorType resources[16];
};

struct VK_Pipeline {
    VkPipeline handle;

    VK_PipelineState state;

    // Max three shader modules:
    //   -      vs + fs
    //   -      ms + fs
    //   - ts + ms + fs
    //
    // no support for tesselation or geometry shaders. @todo: mesh shading!
    //
    U32 num_shaders;
    VK_Shader shaders[3];

    U32 num_targets;
    VkFormat target_formats[8];
    VkFormat depth_format;

    struct {
        VkDescriptorSetLayout set;
        VkPipelineLayout      pipeline;
    } layout;
};

Function void VK_BufferCreate(VK_Device *device, VK_Buffer *buffer);

#endif  // ANIMATION_VULKAN_H_
