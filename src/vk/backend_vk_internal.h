#pragma once

#define VK_USE_PLATFORM_WIN32_KHR

#include "backend_vk.h"

#include "../dxgi/dxgi_support.h"

#include <vulkan/vulkan.h>

typedef enum {
    VK_SURFACE_KIND_NONE = 0,
    VK_SURFACE_KIND_SDR_BGRA8,
    VK_SURFACE_KIND_SDR_RGBA8,
    VK_SURFACE_KIND_HDR_SCRGB,
    VK_SURFACE_KIND_HDR10
} vk_surface_kind_t;

typedef struct {
    int image_width;
    int image_height;
    int shader_color_space;
    int surface_kind;
} vk_present_push_constants_t;

typedef struct {
    HWND               hwnd;
    bool               ready;
    bool               vsync_enabled;
    bool               hdr_presenting;
    bool               hdr_metadata_enabled;
    bool               hdr_metadata_applied;
    present_conversion_mode_t conversion_mode;
    shader_color_space_t shader_color_space;
    int                render_width;
    int                render_height;
    int                popup_width;
    int                popup_height;
    int                swapchain_width;
    int                swapchain_height;
    char               hdr_status[512];
    char               error[512];
    VkInstance         instance;
    VkSurfaceKHR       surface;
    VkPhysicalDevice   physical_device;
    VkDevice           device;
    PFN_vkSetHdrMetadataEXT set_hdr_metadata_ext;
    uint               queue_family_index;
    VkQueue            queue;
    VkSwapchainKHR     swapchain;
    VkFormat           swapchain_format;
    VkColorSpaceKHR    swapchain_color_space;
    VkPresentModeKHR   present_mode;
    uint               swapchain_image_count;
    VkImage           *swapchain_images;
    bool              *swapchain_image_initialized;
    VkBuffer           staging_buffer;
    VkDeviceMemory     staging_memory;
    void              *staging_mapped;
    size_t             staging_size;
    VkImage            intermediate_image;
    VkDeviceMemory     intermediate_memory;
    bool               intermediate_initialized;
    VkImage            source_image;
    VkDeviceMemory     source_memory;
    VkImageView        source_image_view;
    bool               source_initialized;
    VkCommandPool      command_pool;
    VkCommandBuffer    command_buffer;
    VkSemaphore        acquire_semaphore;
    VkSemaphore        present_semaphore;
    VkFence            submit_fence;
    bool               can_scale_blit;
    bool               swapchain_supports_transfer_dst;
    bool               swapchain_supports_color_attachment;
    vk_surface_kind_t  surface_kind;
    VkSampler          sampler;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool   descriptor_pool;
    VkDescriptorSet    descriptor_set;
    VkPipelineLayout   pipeline_layout;
    VkRenderPass       render_pass;
    VkPipeline         pipeline;
    VkImageView       *swapchain_image_views;
    VkFramebuffer     *framebuffers;
    dxgi_support_t     dxgi;
} vk_state_t;

extern vk_state_t g_vk;

void vk_set_error_text(const char *text);
void vk_set_error_result(const char *what, VkResult result);
void vk_set_hdr_status_text(const char *text);
bool vk_has_instance_extension(const char *name);
bool vk_has_device_extension(VkPhysicalDevice device, const char *name);
bool vk_pick_physical_device(void);
bool vk_create_swapchain_resources(void);
void vk_destroy_swapchain_resources(void);
bool vk_create_device_shader_resources(void);
void vk_destroy_device_shader_resources(void);
void vk_update_hdr_status(bool has_hdr10_candidate);
void vk_pack_surface(const f32x4_surface_t *surface);
bool vk_record_present_commands(uint image_index);
bool vk_apply_hdr_metadata(void);
