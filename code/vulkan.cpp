#if OS_WINDOWS
    #define VK_USE_PLATFORM_WIN32_KHR   1
#elif OS_LINUX
    #define VK_USE_PLATFORM_WAYLAND_KHR 1
#endif

#define VK_NO_PROTOTYPES 1
#include <vulkan/vulkan.h>

#include <time.h>

#define VK_CHECK(x) assert((x) == VK_SUCCESS)

#define VK_FRAME_COUNT     2 // number of processing frames in flight

#define VK_IMAGE_COUNT     3 // number of images on the swapchain (triple buffer)
#define VK_MAX_IMAGE_COUNT 8 // maximum number of images a swapchain can have

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
    B32 vsync; // whether vsync is on or off

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

struct VK_Pipeline {
    VkPipeline handle;

    VkShaderModule vs;
    VkShaderModule fs;

    VkDescriptorSetLayout set_layout;
    VkPipelineLayout layout;
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

static B32 VK_LibraryLoad(VK_Context *vk) {
    B32 result;

#if OS_WINDOWS
    // I assume this just works, its how volk does it and if its in path it should be available
    //
    vk->lib = LoadLibraryA("vulkan-1.dll");
    if (vk->lib) {
        vk->GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) GetProcAddress(vk->lib, "vkGetInstanceProcAddr");
    }
#elif OS_LINUX
    vk->lib = dlopen("libvulkan.so", RTLD_NOW);
    if (vk->lib) {
        vk->GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(vk->lib, "vkGetInstanceProcAddr");
    }
#endif

    result = (vk->lib != 0) && (vk->GetInstanceProcAddr != 0);
    return result;
}

static VkBool32 VK_DebugMessageCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
        void*                                            pUserData)
{
    (void) messageSeverity;
    (void) messageTypes;
    (void) pUserData;

    // @todo: make this more verbose
    //
    printf("[VULKAN] :: %s\n", pCallbackData->pMessage);

    return VK_FALSE;
}

static B32 VK_ContextInitialise(VK_Context *vk) {
    B32 result = false;

    if (!VK_LibraryLoad(vk)) {
        printf("[error] :: failed to load vulkan library\n");
        return 1;
    }

    #define VK_DYN_FUNCTION(x) vk->x = (PFN_vk##x) vk->GetInstanceProcAddr(0, Stringify(vk##x))
        #define VK_GLOBAL_FUNCTIONS
        #include "vulkan_dyn_functions.cpp"
    #undef VK_DYN_FUNCTION

    U32 version;
    vk->EnumerateInstanceVersion(&version);

    if (version < VK_API_VERSION_1_3) {
        printf("[error] :: minimum api version is not met. requires at least vulkan 1.3\n");
        return 1;
    }

    B32 debug = (vk->flags & VK_CONTEXT_FLAG_DEBUG) != 0;

    // Create instance
    //
    // @todo: other windowing systems
    //
    {
        VkApplicationInfo app_info = {};
        app_info.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.apiVersion = VK_API_VERSION_1_3;

        U32 extension_count = 2;
        const char *extensions[8] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
        #if OS_WINDOWS
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
        #elif OS_LINUX
            // @todo: add xlib support!
            //
            VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
        #endif
        };

        if (debug) { extensions[extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME; }

        const char *layers[] = { "VK_LAYER_KHRONOS_validation" };

        VkInstanceCreateInfo create_info = {};
        create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo        = &app_info;
        create_info.enabledExtensionCount   = extension_count;
        create_info.ppEnabledExtensionNames = extensions;
        create_info.enabledLayerCount       = debug ? 1 : 0;
        create_info.ppEnabledLayerNames     = layers;

        VK_CHECK(vk->CreateInstance(&create_info, 0, &vk->instance));

        #define VK_DYN_FUNCTION(x) vk->x = (PFN_vk##x) vk->GetInstanceProcAddr(vk->instance, Stringify(vk##x))
            #define VK_DEBUG_FUNCTIONS
            #define VK_INSTANCE_FUNCTIONS
            #include "vulkan_dyn_functions.cpp"
        #undef VK_DYN_FUNCTION
    }

    if (debug) {
        VkDebugUtilsMessengerCreateInfoEXT create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        create_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        create_info.pfnUserCallback = VK_DebugMessageCallback;
        create_info.pUserData       = (void *) vk;

        VK_CHECK(vk->CreateDebugUtilsMessengerEXT(vk->instance, &create_info, 0, &vk->debug_messenger));
    }

    // Enumerate physical devices
    //
    {
        U32 device_count = 0;
        VkPhysicalDevice devices[16];

        VK_CHECK(vk->EnumeratePhysicalDevices(vk->instance, &device_count, 0));
        VK_CHECK(vk->EnumeratePhysicalDevices(vk->instance, &device_count, devices));

        VK_Device *first = 0;
        VK_Device *last  = 0;

        for (U32 it = 0; it < device_count; ++it) {
            VkPhysicalDevice handle = devices[it];

            VK_Device *device = (VK_Device *) zmalloc(sizeof(VK_Device));
            last = (last ? last->next : first) = device;

            device->physical = handle;
            device->vk       = vk;

            vk->GetPhysicalDeviceProperties(handle, &device->properties);
            vk->GetPhysicalDeviceFeatures(handle, &device->features);
            vk->GetPhysicalDeviceMemoryProperties(handle, &device->memory_properties);

            U32 queue_family_count = 0;
            VkQueueFamilyProperties queue_families[16];

            vk->GetPhysicalDeviceQueueFamilyProperties(handle, &queue_family_count, 0);
            vk->GetPhysicalDeviceQueueFamilyProperties(handle, &queue_family_count, queue_families);

            device->graphics_queue.family = (U32) -1;

            for (U32 q = 0; q < queue_family_count; ++q) {
                VkQueueFamilyProperties *family = &queue_families[q];

                if (family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    device->graphics_queue.family = q;
                    break;
                }
            }

            assert(device->graphics_queue.family != (U32) -1);
        }

        vk->devices = first;

        // Select device
        //
        // @todo: could probably do it above but want to test other things later so break this out
        // into another step
        //

        VK_Device *preferred = 0;
        VK_Device *fallback  = 0;
        for (VK_Device *device = vk->devices; device != 0; device = device->next) {
            if (device->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                // This is very simple at the moment only preferring discrete gpus if available, could
                // make it more interesting later like scoring based on specific features available etc.
                //
                preferred = device;
                break;
            }

            if (!fallback) { fallback = device; }
        }

        vk->device = preferred ? preferred : fallback;
        assert(vk->device != 0);

        printf("[INFO] :: using device '%s'\n", vk->device->properties.deviceName);
    }

    // Create device
    //
    VK_Device *device = vk->device;
    {

        F32 queue_priority = 1.0f;

        VkDeviceQueueCreateInfo queue_create = {};
        queue_create.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create.queueFamilyIndex = device->graphics_queue.family;
        queue_create.queueCount       = 1;
        queue_create.pQueuePriorities = &queue_priority;

        const char *device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkPhysicalDeviceVulkan11Features features11 = {};
        VkPhysicalDeviceVulkan12Features features12 = {};
        VkPhysicalDeviceVulkan13Features features13 = {};

        VkPhysicalDeviceFeatures features = {};
        features.fillModeNonSolid = VK_TRUE;
        features.shaderInt16      = VK_TRUE;
        features.wideLines        = VK_TRUE;

        // features from vulkan 1.1
        features11.sType                              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        features11.storageBuffer16BitAccess           = VK_TRUE;
        features11.uniformAndStorageBuffer16BitAccess = VK_TRUE;

        // features from vulkan 1.2
        //
        // may want
        // descriptorBindingSampledImageUpdateAfterBind for bindless textures
        //
        features12.sType                             = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.pNext                             = &features11;
        features12.shaderFloat16                     = VK_TRUE;
        features12.shaderInt8                        = VK_TRUE;
        features12.storageBuffer8BitAccess           = VK_TRUE;
        features12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
        features12.scalarBlockLayout                 = VK_TRUE;

        // features from vulkan 1.3
        features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.pNext            = &features12;
        features13.synchronization2 = VK_TRUE;
        features13.dynamicRendering = VK_TRUE;

        VkDeviceCreateInfo create_info = {};
        create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.pNext                   = &features13;
        create_info.queueCreateInfoCount    = 1;
        create_info.pQueueCreateInfos       = &queue_create;
        create_info.enabledExtensionCount   = 1; // @todo: may want other extensions enabled
        create_info.ppEnabledExtensionNames = device_extensions;
        create_info.pEnabledFeatures        = &features;

        VK_CHECK(vk->CreateDevice(device->physical, &create_info, 0, &device->handle));

        #define VK_DYN_FUNCTION(x) vk->x = (PFN_vk##x) vk->GetDeviceProcAddr(vk->device->handle, Stringify(vk##x))
            #define VK_DEVICE_FUNCTIONS
            #include "vulkan_dyn_functions.cpp"
        #undef VK_DYN_FUNCTION

        vk->GetDeviceQueue(device->handle, device->graphics_queue.family, 0, &device->graphics_queue.handle);
    }

    // create a command pool for quick one time scratch commands
    //
    {
        VkCommandPoolCreateInfo create_info = {};
        create_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        create_info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        create_info.queueFamilyIndex = device->graphics_queue.family;

        VK_CHECK(vk->CreateCommandPool(device->handle, &create_info, 0, &device->scratch_cmd_pool));
    }

    for (U32 it = 0; it < VK_FRAME_COUNT; ++it) {
        VK_Frame *frame = &device->frames[it];
        VK_Frame *next  = &device->frames[(it + 1) % VK_FRAME_COUNT];

        frame->next = next;

        // Now initialise this frame's data
        //
        {
            VkCommandPoolCreateInfo create_info = {};
            create_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            create_info.queueFamilyIndex = device->graphics_queue.family;

            VK_CHECK(vk->CreateCommandPool(device->handle, &create_info, 0, &frame->command_pool));

            VK_CommandBufferSet *cmds = &frame->cmds_first;

            cmds->next_buffer = 0;
            cmds->next        = 0;

            VkCommandBufferAllocateInfo alloc_info = {};
            alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            alloc_info.commandPool        = frame->command_pool;
            alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            alloc_info.commandBufferCount = VK_COMMAND_BUFFER_SET_COUNT;

            VK_CHECK(vk->AllocateCommandBuffers(device->handle, &alloc_info, cmds->handles));

            frame->cmds = cmds;
        }

        // :note the pool sizes were chosen arbitarily, can change if need be. sum to 16384
        //
        // :note we only support separated samplers, this is simpler and is more in line with how
        // gpus actually work. we also only use storage buffer, however, on modern desktop class gpus
        // the difference between uniform and storage buffers is either non-existent or negligible
        //
        // @todo: UPDATE_AFTER_BIND_BIT for image pool, may need two seperate pools for allocating
        // image descriptors if we want. for bindless texturing
        //
        {
            VkDescriptorPoolSize pool_sizes[3] = {};

            pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_SAMPLER;
            pool_sizes[0].descriptorCount = 64;

            pool_sizes[1].type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            pool_sizes[1].descriptorCount = 12224;

            pool_sizes[2].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            pool_sizes[2].descriptorCount = 4096;

            VkDescriptorPoolCreateInfo create_info = {};
            create_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            create_info.maxSets       = 2048;
            create_info.poolSizeCount = 3;
            create_info.pPoolSizes    = pool_sizes;

            VK_CHECK(vk->CreateDescriptorPool(device->handle, &create_info, 0, &frame->descriptor_pool));
        }

        {
            VkSemaphoreCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VK_CHECK(vk->CreateSemaphore(device->handle, &create_info, 0, &frame->acquire));
            VK_CHECK(vk->CreateSemaphore(device->handle, &create_info, 0, &frame->render));
        }

        {
            VkFenceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            VK_CHECK(vk->CreateFence(device->handle, &create_info, 0, &frame->fence));
        }
    }

    device->frame = &device->frames[0];

    result = true;
    return result;
}

static U32 VK_MemoryTypeIndexGet(VK_Device *device, U32 bits, VkMemoryPropertyFlags properties) {
    U32 result = (U32) -1;

    VkPhysicalDeviceMemoryProperties *props = &device->memory_properties;
    for (U32 it = 0; it < props->memoryTypeCount; ++it) {
        VkMemoryType *type = &props->memoryTypes[it];

        if (bits & (1 << it) && ((type->propertyFlags & properties) == properties)) {
            result = it;
            break;
        }
    }

    assert(result != (U32) -1);
    return result;
}

static VkCommandBuffer VK_ScratchCommandsBegin(VK_Device *device) {
    VkCommandBuffer result;

    VK_Context *vk = device->vk;

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool        = device->scratch_cmd_pool;
    alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VK_CHECK(vk->AllocateCommandBuffers(device->handle, &alloc_info, &result));

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vk->BeginCommandBuffer(result, &begin_info));

    return result;
}

static void VK_ScratchCommandsEnd(VK_Device *device, VkCommandBuffer cmds) {
    VK_Context *vk = device->vk;

    VK_CHECK(vk->EndCommandBuffer(cmds));

    VkSubmitInfo submit_info = {};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &cmds;

    VK_CHECK(vk->QueueSubmit(device->graphics_queue.handle, 1, &submit_info, 0));
    VK_CHECK(vk->QueueWaitIdle(device->graphics_queue.handle));

    vk->FreeCommandBuffers(device->handle, device->scratch_cmd_pool, 1, &cmds);
}

static VkCommandBuffer VK_CommandBufferPush(VK_Context *vk, VK_Frame *frame) {
    VkCommandBuffer result;

    VK_CommandBufferSet *cmds = frame->cmds;

    // @incomplete: if this is the case we should allocate a new VK_CommandBufferSet and append it
    // to the list
    //
    assert(cmds->next_buffer < VK_COMMAND_BUFFER_SET_COUNT);

    result = cmds->handles[cmds->next_buffer++];

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vk->BeginCommandBuffer(result, &begin_info));

    return result;
}

static B32 VK_SurfaceCreate(VK_Device *device, VK_Swapchain *swapchain) {
    B32 result = false;

    VK_Context *vk = device->vk;

#if OS_WINDOWS
    VkWin32SurfaceCreateInfoKHR create_info = {};
    create_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = swapchain->surface.win.hinstance;
    create_info.hwnd      = swapchain->surface.win.hwnd;

    VK_CHECK(vk->CreateWin32SurfaceKHR(vk->instance, &create_info, 0, &swapchain->surface.handle));
#elif OS_LINUX
    VkWaylandSurfaceCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    create_info.display = swapchain->surface.wl.display;
    create_info.surface = swapchain->surface.wl.surface;

    VK_CHECK(vk->CreateWaylandSurfaceKHR(vk->instance, &create_info, 0, &swapchain->surface.handle));
#endif

    result = true;
    return result;
}

static B32 VK_SwapchainCreate(VK_Device *device, VK_Swapchain *swapchain) {
    B32 result = false;

    VK_Context *vk = device->vk;

    vk->DeviceWaitIdle(device->handle);

    if (swapchain->handle == VK_NULL_HANDLE) {
        // swapchain has not been created yet, select appropriate properties for it
        //
        assert(swapchain->surface.handle == VK_NULL_HANDLE);

        if (!VK_SurfaceCreate(device, swapchain)) { return result; }

        // get possible present mode for disabling vsync
        //

        U32 present_mode_count = 0;
        VkPresentModeKHR present_modes[32];

        VK_CHECK(vk->GetPhysicalDeviceSurfacePresentModesKHR(device->physical, swapchain->surface.handle, &present_mode_count, 0));
        VK_CHECK(vk->GetPhysicalDeviceSurfacePresentModesKHR(device->physical, swapchain->surface.handle, &present_mode_count, present_modes));

        swapchain->vsync_disable = VK_PRESENT_MODE_FIFO_KHR;

        for (U32 it = 0; it < present_mode_count; ++it) {
            if (present_modes[it] == VK_PRESENT_MODE_MAILBOX_KHR) {
                swapchain->vsync_disable = present_modes[it];
                break;
            }
            else if (present_modes[it] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                swapchain->vsync_disable = present_modes[it];
            }
        }

        // get a suitable surface format
        //
        U32 format_count = 0;
        VkSurfaceFormatKHR formats[32];

        VK_CHECK(vk->GetPhysicalDeviceSurfaceFormatsKHR(device->physical, swapchain->surface.handle, &format_count, 0));
        VK_CHECK(vk->GetPhysicalDeviceSurfaceFormatsKHR(device->physical, swapchain->surface.handle, &format_count, formats));

        for (U32 it = 0; it < format_count; ++it) {
            VkSurfaceFormatKHR *format = &formats[it];

            if (format->format == VK_FORMAT_R8G8B8A8_SRGB || format->format == VK_FORMAT_B8G8R8A8_SRGB) {
                if (format->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    // prefer srgb variants for the surface format
                    //
                    swapchain->surface.format = *format;
                    break;
                }
            }
            else if (format->format == VK_FORMAT_R8G8B8A8_UNORM || format->format == VK_FORMAT_B8G8R8A8_UNORM) {
                if (format->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    swapchain->surface.format = *format;
                }
            }
        }
    }

    VkSurfaceCapabilitiesKHR surface_caps = {};
    VK_CHECK(vk->GetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical, swapchain->surface.handle, &surface_caps));

    VkCompositeAlphaFlagBitsKHR surface_composite =
        (surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)            ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        : (surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)  ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
        : (surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
        : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    U32 width, height;

    if (surface_caps.currentExtent.width != (U32) -1) {
        width  = surface_caps.currentExtent.width;
        height = surface_caps.currentExtent.height;
    }
    else {
        width  = swapchain->surface.width;
        height = swapchain->surface.height;
    }

    assert(surface_caps.minImageCount <= VK_MAX_IMAGE_COUNT);

    swapchain->images.count = Max(VK_IMAGE_COUNT, surface_caps.minImageCount);

    if (surface_caps.maxImageCount != 0) {
        // there is an actual limit on the maximum number of images that can be on a swapchain
        //
        swapchain->images.count = Min(swapchain->images.count, surface_caps.maxImageCount);
    }

    VkSwapchainKHR old = swapchain->handle;

    {
        VkSwapchainCreateInfoKHR create_info = {};
        create_info.sType              = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface            = swapchain->surface.handle;
        create_info.minImageCount      = swapchain->images.count;
        create_info.imageFormat        = swapchain->surface.format.format;
        create_info.imageColorSpace    = swapchain->surface.format.colorSpace;
        create_info.imageExtent.width  = width;
        create_info.imageExtent.height = height;
        create_info.imageArrayLayers   = 1;
        create_info.imageUsage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        create_info.preTransform       = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        create_info.compositeAlpha     = surface_composite;
        create_info.presentMode        = swapchain->vsync ? VK_PRESENT_MODE_FIFO_KHR : swapchain->vsync_disable;
        create_info.clipped            = VK_FALSE;
        create_info.oldSwapchain       = old;

        VK_CHECK(vk->CreateSwapchainKHR(device->handle, &create_info, 0, &swapchain->handle));
    }

    if (old != VK_NULL_HANDLE) {
        // teardown depth resources
        //
        vk->FreeMemory(device->handle, swapchain->depth.memory, 0);
        vk->DestroyImageView(device->handle, swapchain->depth.view, 0);
        vk->DestroyImage(device->handle, swapchain->depth.image, 0);

        for (U32 it = 0; it < swapchain->images.count; ++it) {
            vk->DestroyImageView(device->handle, swapchain->images.views[it], 0);
        }

        vk->DestroySwapchainKHR(device->handle, old, 0);
    }

    swapchain->surface.width  = width;
    swapchain->surface.height = height;

    U32 image_count = 0;
    VK_CHECK(vk->GetSwapchainImagesKHR(device->handle, swapchain->handle, &image_count, 0));
    VK_CHECK(vk->GetSwapchainImagesKHR(device->handle, swapchain->handle, &image_count, swapchain->images.handles));

    assert(image_count <= VK_MAX_IMAGE_COUNT);

    swapchain->images.count = image_count;

    for (U32 it = 0; it < image_count; ++it) {
        VkImageViewCreateInfo create_info = {};
        create_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image    = swapchain->images.handles[it];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format   = swapchain->surface.format.format;

        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.layerCount = 1;

        VK_CHECK(vk->CreateImageView(device->handle, &create_info, 0, &swapchain->images.views[it]));
    }

    // @todo: memory allocator
    // @todo: VK_Image that can be used generically
    //
    // setup depth resoruces
    //
    {
        VkImageCreateInfo create_info = {};
        create_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        create_info.imageType     = VK_IMAGE_TYPE_2D;
        create_info.format        = VK_FORMAT_D32_SFLOAT; // pretty much supported everywhere
        create_info.extent.width  = swapchain->surface.width;
        create_info.extent.height = swapchain->surface.height;
        create_info.extent.depth  = 1;
        create_info.mipLevels     = 1;
        create_info.arrayLayers   = 1;
        create_info.samples       = VK_SAMPLE_COUNT_1_BIT;
        create_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        create_info.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VK_CHECK(vk->CreateImage(device->handle, &create_info, 0, &swapchain->depth.image));

        VkMemoryRequirements requirements;
        vk->GetImageMemoryRequirements(device->handle, swapchain->depth.image, &requirements);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize  = requirements.size;
        alloc_info.memoryTypeIndex = VK_MemoryTypeIndexGet(device, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vk->AllocateMemory(device->handle, &alloc_info, 0, &swapchain->depth.memory));
        VK_CHECK(vk->BindImageMemory(device->handle, swapchain->depth.image, swapchain->depth.memory, 0));
    }

    {
        VkImageViewCreateInfo create_info = {};
        create_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image    = swapchain->depth.image;
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format   = VK_FORMAT_D32_SFLOAT;

        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.layerCount = 1;

        VK_CHECK(vk->CreateImageView(device->handle, &create_info, 0, &swapchain->depth.view));
    }

    {
        VkCommandBuffer cmds = VK_ScratchCommandsBegin(device);

        VkImageMemoryBarrier2 depth_transition = {};
        depth_transition.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        depth_transition.srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        depth_transition.dstStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        depth_transition.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depth_transition.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_transition.newLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_transition.image         = swapchain->depth.image;

        depth_transition.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_transition.subresourceRange.levelCount = 1;
        depth_transition.subresourceRange.layerCount = 1;

        VkDependencyInfo dependency_info = {};
        dependency_info.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers    = &depth_transition;

        vk->CmdPipelineBarrier2(cmds, &dependency_info);

        VK_ScratchCommandsEnd(device, cmds);
    }

    result = true;
    return result;
}

static VK_Frame *VK_NextFrameAcquire(VK_Device *device, VK_Swapchain *swapchain) {
    VK_Frame *result = device->frame->next;
    device->frame = result;

    VK_Context *vk = device->vk;

    VK_CHECK(vk->WaitForFences(device->handle, 1, &result->fence, VK_FALSE, UINT64_MAX));
    VK_CHECK(vk->ResetFences(device->handle, 1, &result->fence));

    VK_CHECK(vk->ResetCommandPool(device->handle, result->command_pool, 0));

    VK_CommandBufferSet *cmds = result->cmds;
    cmds->next_buffer = 0;

    VK_CHECK(vk->ResetDescriptorPool(device->handle, result->descriptor_pool, 0));

    if (swapchain) {
        VkResult success;
        do {
            success = vk->AcquireNextImageKHR(device->handle, swapchain->handle, UINT64_MAX, result->acquire, VK_NULL_HANDLE, &result->image_index);

            if (success == VK_ERROR_OUT_OF_DATE_KHR) {
                VK_SwapchainCreate(device, swapchain);
            }
            else if (success == VK_ERROR_SURFACE_LOST_KHR) {
                // if we get an error telling us the surface has been lost, we destroy the current surface
                // and create a new one as we still have the window data.
                //
                // as the entire surface has to be re-created, theoretically the prameters to the swapchain
                // could've change, thus we completely teardown everything and start from scratch when
                // calling VK_SwapchainCreate
                //
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
                // not optimal but the system will still allow us to present, this normally occurs
                // when resizing as the swapchain is re-created on resize and when presenting to a
                // suboptimal swapchain this will natually handle itself by the end of the frame
                //
                // :suboptimal_present
                //
                break;
            }
            else {
                assert(success == VK_SUCCESS);
            }
        } while (success != VK_SUCCESS);
    }

    return result;
}

static void VK_BufferCreate(VK_Device *device, VK_Buffer *buffer) {
    VK_Context *vk = device->vk;

    VkBufferCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size  = buffer->size;
    create_info.usage = buffer->usage;

    VK_CHECK(vk->CreateBuffer(device->handle, &create_info, 0, &buffer->handle));

    VkMemoryPropertyFlags memory_properties;
    if (buffer->host_mapped) {
        memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    else {
        memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    VkMemoryRequirements requirements;
    vk->GetBufferMemoryRequirements(device->handle, buffer->handle, &requirements);

    buffer->size      = requirements.size;
    buffer->alignment = requirements.alignment;

    // @todo: memory allocator
    //

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize  = requirements.size;
    alloc_info.memoryTypeIndex = VK_MemoryTypeIndexGet(device, requirements.memoryTypeBits, memory_properties);

    VK_CHECK(vk->AllocateMemory(device->handle, &alloc_info, 0, &buffer->memory));
    VK_CHECK(vk->BindBufferMemory(device->handle, buffer->handle, buffer->memory, 0));

    if (buffer->host_mapped) { vk->MapMemory(device->handle, buffer->memory, 0, buffer->size, 0, &buffer->data); }
}
