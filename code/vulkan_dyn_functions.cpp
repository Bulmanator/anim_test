#if defined(VK_GLOBAL_FUNCTIONS)
    VK_DYN_FUNCTION(EnumerateInstanceVersion);
    VK_DYN_FUNCTION(CreateInstance);

    #undef VK_GLOBAL_FUNCTIONS
#endif

#if defined(VK_DEBUG_FUNCTIONS)
    VK_DYN_FUNCTION(CreateDebugUtilsMessengerEXT);
    VK_DYN_FUNCTION(DestroyDebugUtilsMessengerEXT);

    #undef VK_DEBUG_FUNCTIONS
#endif

#if defined(VK_INSTANCE_FUNCTIONS)
    VK_DYN_FUNCTION(EnumeratePhysicalDevices);
    VK_DYN_FUNCTION(GetPhysicalDeviceProperties);
    VK_DYN_FUNCTION(GetPhysicalDeviceFeatures);
    VK_DYN_FUNCTION(GetPhysicalDeviceMemoryProperties);
    VK_DYN_FUNCTION(GetPhysicalDeviceQueueFamilyProperties);
    VK_DYN_FUNCTION(CreateDevice);
    VK_DYN_FUNCTION(GetDeviceProcAddr);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VK_DYN_FUNCTION(CreateWin32SurfaceKHR);
#endif

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VK_DYN_FUNCTION(CreateWaylandSurfaceKHR);
#endif


    VK_DYN_FUNCTION(GetPhysicalDeviceSurfaceCapabilitiesKHR);
    VK_DYN_FUNCTION(GetPhysicalDeviceSurfacePresentModesKHR);
    VK_DYN_FUNCTION(GetPhysicalDeviceSurfaceFormatsKHR);
    VK_DYN_FUNCTION(DestroySurfaceKHR);

    #undef VK_INSTANCE_FUNCTIONS
#endif

#if defined(VK_DEVICE_FUNCTIONS)
    VK_DYN_FUNCTION(GetDeviceQueue);
    VK_DYN_FUNCTION(QueueWaitIdle);
    VK_DYN_FUNCTION(DeviceWaitIdle);
    VK_DYN_FUNCTION(CreateSwapchainKHR);
    VK_DYN_FUNCTION(DestroySwapchainKHR);
    VK_DYN_FUNCTION(GetSwapchainImagesKHR);
    VK_DYN_FUNCTION(CreateCommandPool);
    VK_DYN_FUNCTION(CreateDescriptorPool);
    VK_DYN_FUNCTION(CreateSemaphore);
    VK_DYN_FUNCTION(CreateFence);
    VK_DYN_FUNCTION(WaitForFences);
    VK_DYN_FUNCTION(ResetFences);
    VK_DYN_FUNCTION(ResetCommandPool);
    VK_DYN_FUNCTION(ResetDescriptorPool);
    VK_DYN_FUNCTION(AcquireNextImageKHR);
    VK_DYN_FUNCTION(AllocateCommandBuffers);
    VK_DYN_FUNCTION(BeginCommandBuffer);
    VK_DYN_FUNCTION(EndCommandBuffer);
    VK_DYN_FUNCTION(CreateImageView);
    VK_DYN_FUNCTION(DestroyImageView);
    VK_DYN_FUNCTION(QueueSubmit);
    VK_DYN_FUNCTION(QueuePresentKHR);
    VK_DYN_FUNCTION(FreeMemory);
    VK_DYN_FUNCTION(CreateImage);
    VK_DYN_FUNCTION(DestroyImage);
    VK_DYN_FUNCTION(GetImageMemoryRequirements);
    VK_DYN_FUNCTION(AllocateMemory);
    VK_DYN_FUNCTION(BindImageMemory);
    VK_DYN_FUNCTION(FreeCommandBuffers);
    VK_DYN_FUNCTION(CreateDescriptorSetLayout);
    VK_DYN_FUNCTION(CreatePipelineLayout);
    VK_DYN_FUNCTION(CreateShaderModule);
    VK_DYN_FUNCTION(CreateGraphicsPipelines);
    VK_DYN_FUNCTION(CreateBuffer);
    VK_DYN_FUNCTION(GetBufferMemoryRequirements);
    VK_DYN_FUNCTION(BindBufferMemory);
    VK_DYN_FUNCTION(MapMemory);
    VK_DYN_FUNCTION(AllocateDescriptorSets);
    VK_DYN_FUNCTION(UpdateDescriptorSets);
    VK_DYN_FUNCTION(CreateSampler);

    // commands
    //
    VK_DYN_FUNCTION(CmdPipelineBarrier2);
    VK_DYN_FUNCTION(CmdBeginRendering);
    VK_DYN_FUNCTION(CmdEndRendering);
    VK_DYN_FUNCTION(CmdBindPipeline);
    VK_DYN_FUNCTION(CmdDraw);
    VK_DYN_FUNCTION(CmdDrawIndexed);
    VK_DYN_FUNCTION(CmdSetViewport);
    VK_DYN_FUNCTION(CmdSetScissor);
    VK_DYN_FUNCTION(CmdBindIndexBuffer);
    VK_DYN_FUNCTION(CmdBindDescriptorSets);
    VK_DYN_FUNCTION(CmdPushConstants);
    VK_DYN_FUNCTION(CmdCopyBufferToImage);

    #undef VK_DEVICE_FUNCTIONS
#endif
