#include "backend_vk_internal.h"

bool backend_vk_create(const present_backend_desc_t *desc)
{
    const char *instance_extensions[3];
    uint instance_extension_count = 0;
    const char *device_extensions[2];
    uint device_extension_count = 0;
    VkApplicationInfo app_info = {0};
    VkInstanceCreateInfo instance_info = {0};
    VkWin32SurfaceCreateInfoKHR surface_info = {0};
    VkDeviceQueueCreateInfo queue_info = {0};
    VkDeviceCreateInfo device_info = {0};
    float queue_priority = 1.0f;
    VkResult result;

    if (desc == NULL || desc->hwnd == NULL) {
        vk_set_error_text("Vulkan backend description is invalid.");
        return false;
    }

    backend_vk_destroy();
    ZeroMemory(&g_vk, sizeof(g_vk));
    g_vk.hwnd = desc->hwnd;
    g_vk.vsync_enabled = desc->vsync_enabled;
    g_vk.shader_color_space = desc->shader_color_space;
    g_vk.render_width = desc->render_width;
    g_vk.render_height = desc->render_height;
    g_vk.popup_width = desc->popup_width;
    g_vk.popup_height = desc->popup_height;
    vk_set_hdr_status_text("");

    if (SUCCEEDED(dxgi_support_create_factory(&g_vk.dxgi))) {
        dxgi_support_detect_surface_capabilities(&g_vk.dxgi, g_vk.hwnd);
    }

    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "A";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "A";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    instance_extensions[instance_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
    instance_extensions[instance_extension_count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
    if (vk_has_instance_extension(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME)) {
        instance_extensions[instance_extension_count++] = VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;
    }

    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = instance_extension_count;
    instance_info.ppEnabledExtensionNames = instance_extensions;

    result = vkCreateInstance(&instance_info, NULL, &g_vk.instance);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateInstance", result);
        return false;
    }

    surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_info.hinstance = GetModuleHandleA(NULL);
    surface_info.hwnd = g_vk.hwnd;

    result = vkCreateWin32SurfaceKHR(g_vk.instance, &surface_info, NULL, &g_vk.surface);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateWin32SurfaceKHR", result);
        backend_vk_destroy();
        return false;
    }

    if (!vk_pick_physical_device()) {
        backend_vk_destroy();
        return false;
    }

    device_extensions[device_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if (vk_has_device_extension(g_vk.physical_device, VK_EXT_HDR_METADATA_EXTENSION_NAME)) {
        device_extensions[device_extension_count++] = VK_EXT_HDR_METADATA_EXTENSION_NAME;
        g_vk.hdr_metadata_enabled = true;
    }

    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = g_vk.queue_family_index;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = device_extension_count;
    device_info.ppEnabledExtensionNames = device_extensions;

    result = vkCreateDevice(g_vk.physical_device, &device_info, NULL, &g_vk.device);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateDevice", result);
        backend_vk_destroy();
        return false;
    }

    if (g_vk.hdr_metadata_enabled) {
        g_vk.set_hdr_metadata_ext = (PFN_vkSetHdrMetadataEXT)vkGetDeviceProcAddr(g_vk.device, "vkSetHdrMetadataEXT");
        if (g_vk.set_hdr_metadata_ext == NULL) {
            g_vk.hdr_metadata_enabled = false;
        }
    }

    vkGetDeviceQueue(g_vk.device, g_vk.queue_family_index, 0, &g_vk.queue);

    if (!vk_create_swapchain_resources()) {
        backend_vk_destroy();
        return false;
    }

    g_vk.conversion_mode = PRESENT_CONVERSION_PRESENT_LAYER_FALLBACK;
    if (g_vk.swapchain_supports_color_attachment && vk_create_device_shader_resources()) {
        g_vk.conversion_mode = PRESENT_CONVERSION_DEVICE_SHADER;
    } else {
        vk_destroy_device_shader_resources();
        if (!g_vk.swapchain_supports_transfer_dst) {
            vk_set_error_text("Vulkan swapchain does not support transfer-destination or color-attachment presentation for this backend.");
            backend_vk_destroy();
            return false;
        }
        if ((g_vk.swapchain_width != g_vk.render_width || g_vk.swapchain_height != g_vk.render_height) && !g_vk.can_scale_blit) {
            vk_set_error_text("Vulkan backend requires device-shader conversion or blit-capable scaling for this swapchain.");
            backend_vk_destroy();
            return false;
        }
        if (g_vk.surface_kind == VK_SURFACE_KIND_HDR10) {
            vk_set_error_text("Vulkan HDR10 presentation currently requires the device-side conversion shader path.");
            backend_vk_destroy();
            return false;
        }
    }

    if (!vk_apply_hdr_metadata()) {
        backend_vk_destroy();
        return false;
    }
    vk_update_hdr_status(g_vk.surface_kind == VK_SURFACE_KIND_HDR10);

    {
        VkCommandPoolCreateInfo pool_info = {0};
        VkCommandBufferAllocateInfo alloc_info = {0};
        VkSemaphoreCreateInfo semaphore_info = {0};
        VkFenceCreateInfo fence_info = {0};

        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = g_vk.queue_family_index;
        result = vkCreateCommandPool(g_vk.device, &pool_info, NULL, &g_vk.command_pool);
        if (result != VK_SUCCESS) {
            vk_set_error_result("vkCreateCommandPool", result);
            backend_vk_destroy();
            return false;
        }

        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = g_vk.command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        result = vkAllocateCommandBuffers(g_vk.device, &alloc_info, &g_vk.command_buffer);
        if (result != VK_SUCCESS) {
            vk_set_error_result("vkAllocateCommandBuffers", result);
            backend_vk_destroy();
            return false;
        }

        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        result = vkCreateSemaphore(g_vk.device, &semaphore_info, NULL, &g_vk.acquire_semaphore);
        if (result != VK_SUCCESS) {
            vk_set_error_result("vkCreateSemaphore(acquire)", result);
            backend_vk_destroy();
            return false;
        }

        result = vkCreateSemaphore(g_vk.device, &semaphore_info, NULL, &g_vk.present_semaphore);
        if (result != VK_SUCCESS) {
            vk_set_error_result("vkCreateSemaphore(present)", result);
            backend_vk_destroy();
            return false;
        }

        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        result = vkCreateFence(g_vk.device, &fence_info, NULL, &g_vk.submit_fence);
        if (result != VK_SUCCESS) {
            vk_set_error_result("vkCreateFence", result);
            backend_vk_destroy();
            return false;
        }
    }

    g_vk.ready = true;
    g_vk.error[0] = '\0';
    return true;
}

void backend_vk_destroy(void)
{
    if (g_vk.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_vk.device);
    }

    if (g_vk.device != VK_NULL_HANDLE && g_vk.submit_fence != VK_NULL_HANDLE) {
        vkDestroyFence(g_vk.device, g_vk.submit_fence, NULL);
        g_vk.submit_fence = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.present_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_vk.device, g_vk.present_semaphore, NULL);
        g_vk.present_semaphore = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.acquire_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_vk.device, g_vk.acquire_semaphore, NULL);
        g_vk.acquire_semaphore = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_vk.device, g_vk.command_pool, NULL);
        g_vk.command_pool = VK_NULL_HANDLE;
    }

    vk_destroy_swapchain_resources();

    if (g_vk.device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_vk.device, NULL);
        g_vk.device = VK_NULL_HANDLE;
    }
    if (g_vk.instance != VK_NULL_HANDLE && g_vk.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
        g_vk.surface = VK_NULL_HANDLE;
    }
    if (g_vk.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_vk.instance, NULL);
        g_vk.instance = VK_NULL_HANDLE;
    }

    dxgi_support_destroy(&g_vk.dxgi);
    g_vk.ready = false;
}

bool backend_vk_is_ready(void)
{
    return g_vk.ready;
}

bool backend_vk_present(const f32x4_surface_t *surface)
{
    VkResult result;
    uint image_index = 0;
    VkSubmitInfo submit_info = {0};
    VkPresentInfoKHR present_info = {0};
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (!g_vk.ready) {
        vk_set_error_text("Vulkan backend is not ready.");
        return false;
    }
    if (surface == NULL || surface->pixels == NULL) {
        vk_set_error_text("Vulkan present surface is invalid.");
        return false;
    }
    if (surface->width != g_vk.render_width || surface->height != g_vk.render_height) {
        vk_set_error_text("Vulkan present surface dimensions do not match the backend.");
        return false;
    }

    vk_pack_surface(surface);

    result = vkWaitForFences(g_vk.device, 1, &g_vk.submit_fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkWaitForFences", result);
        return false;
    }

    result = vkResetFences(g_vk.device, 1, &g_vk.submit_fence);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkResetFences", result);
        return false;
    }

    result = vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX, g_vk.acquire_semaphore, VK_NULL_HANDLE, &image_index);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkAcquireNextImageKHR", result);
        return false;
    }

    result = vkResetCommandBuffer(g_vk.command_buffer, 0);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkResetCommandBuffer", result);
        return false;
    }

    if (!vk_record_present_commands(image_index)) {
        return false;
    }

    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &g_vk.acquire_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &g_vk.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &g_vk.present_semaphore;

    result = vkQueueSubmit(g_vk.queue, 1, &submit_info, g_vk.submit_fence);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkQueueSubmit", result);
        return false;
    }

    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &g_vk.present_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g_vk.swapchain;
    present_info.pImageIndices = &image_index;

    result = vkQueuePresentKHR(g_vk.queue, &present_info);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkQueuePresentKHR", result);
        return false;
    }

    g_vk.swapchain_image_initialized[image_index] = true;
    if (g_vk.conversion_mode == PRESENT_CONVERSION_DEVICE_SHADER) {
        g_vk.source_initialized = true;
    } else {
        g_vk.intermediate_initialized = true;
    }
    return true;
}

void backend_vk_set_vsync(bool enabled)
{
    g_vk.vsync_enabled = enabled;
}

bool backend_vk_get_vsync(void)
{
    return g_vk.vsync_enabled;
}

bool backend_vk_is_hdr_presenting(void)
{
    return g_vk.hdr_presenting;
}

present_conversion_mode_t backend_vk_conversion_mode(void)
{
    return g_vk.conversion_mode;
}

const char *backend_vk_hdr_status(void)
{
    return g_vk.hdr_status[0] != '\0' ? g_vk.hdr_status : "Vulkan HDR status is not available.";
}

const char *backend_vk_error(void)
{
    return g_vk.error;
}
