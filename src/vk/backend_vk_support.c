#include "backend_vk_internal.h"
#include "present_conversion_spv.h"

#include "../present/color_convert.h"

#include <string.h>
vk_state_t g_vk = {0};

void vk_set_error_text(const char *text)
{
    snprintf(g_vk.error, sizeof(g_vk.error), "%s", text);
}

void vk_set_error_result(const char *what, VkResult result)
{
    snprintf(g_vk.error, sizeof(g_vk.error), "%s failed (vk=0x%08X).", what, (unsigned)result);
}

void vk_set_hdr_status_text(const char *text)
{
    snprintf(g_vk.hdr_status, sizeof(g_vk.hdr_status), "%s", text != NULL ? text : "");
}

static bool vk_output_reports_hdr(void)
{
    return g_vk.dxgi.factory != NULL && dxgi_output_reports_hdr(g_vk.dxgi.output_color_space);
}

static const char *vk_surface_kind_name(vk_surface_kind_t surface_kind)
{
    switch (surface_kind) {
        case VK_SURFACE_KIND_SDR_BGRA8:
            return "VK_FORMAT_B8G8R8A8_UNORM + VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";

        case VK_SURFACE_KIND_SDR_RGBA8:
            return "VK_FORMAT_R8G8B8A8_UNORM + VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";

        case VK_SURFACE_KIND_HDR_SCRGB:
            return "VK_FORMAT_R16G16B16A16_SFLOAT + VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT";

        case VK_SURFACE_KIND_HDR10:
            return "VK_FORMAT_A2B10G10R10_UNORM_PACK32 + VK_COLOR_SPACE_HDR10_ST2084_EXT";

        case VK_SURFACE_KIND_NONE:
        default:
            return "unknown Vulkan surface";
    }
}

void vk_update_hdr_status(bool has_hdr10_candidate)
{
    char available_surfaces[64];
    const char *output_color_space_text = "DXGI output detection unavailable";
    dxgi_surface_kind_t desired_kind = DXGI_SURFACE_KIND_SDR;
    const char *hdr_metadata_text =
        (g_vk.surface_kind == VK_SURFACE_KIND_HDR10)
            ? (g_vk.hdr_metadata_applied
                   ? " HDR10 metadata was applied."
                   : (g_vk.hdr_metadata_enabled ? " HDR10 metadata extension was enabled but not applied." : ""))
            : "";
    const char *conversion_text =
        (g_vk.conversion_mode == PRESENT_CONVERSION_DEVICE_SHADER)
            ? "device-side conversion shader"
            : "presentation-layer CPU conversion fallback";

    if (g_vk.dxgi.factory != NULL) {
        dxgi_available_surfaces_text(&g_vk.dxgi, available_surfaces, sizeof(available_surfaces));
        output_color_space_text = dxgi_color_space_name(g_vk.dxgi.output_color_space);
        desired_kind = dxgi_choose_surface_kind(&g_vk.dxgi, g_vk.shader_color_space);
    } else {
        snprintf(available_surfaces, sizeof(available_surfaces), "%s", "DXGI detection unavailable");
    }

    if ((g_vk.surface_kind == VK_SURFACE_KIND_HDR_SCRGB || g_vk.surface_kind == VK_SURFACE_KIND_HDR10) && vk_output_reports_hdr()) {
        g_vk.hdr_presenting = true;
        snprintf(
            g_vk.hdr_status,
            sizeof(g_vk.hdr_status),
            "Vulkan is presenting %s shader output through %s using a %s on HDR output %s. DXGI reports available surfaces: %s. HDR is present to the surface.",
            dxgi_shader_color_space_name(g_vk.shader_color_space),
            vk_surface_kind_name(g_vk.surface_kind),
            conversion_text,
            output_color_space_text,
            available_surfaces);
        if (hdr_metadata_text[0] != '\0') {
            snprintf(g_vk.hdr_status + strlen(g_vk.hdr_status), sizeof(g_vk.hdr_status) - strlen(g_vk.hdr_status), "%s", hdr_metadata_text);
        }
        return;
    }

    g_vk.hdr_presenting = false;

    if (g_vk.surface_kind == VK_SURFACE_KIND_HDR_SCRGB || g_vk.surface_kind == VK_SURFACE_KIND_HDR10) {
        snprintf(
            g_vk.hdr_status,
            sizeof(g_vk.hdr_status),
            "Vulkan selected %s for %s shader output using a %s, but DXGI reports output %s and available surfaces %s. HDR is not present to the surface.",
            vk_surface_kind_name(g_vk.surface_kind),
            dxgi_shader_color_space_name(g_vk.shader_color_space),
            conversion_text,
            output_color_space_text,
            available_surfaces);
        if (hdr_metadata_text[0] != '\0') {
            snprintf(g_vk.hdr_status + strlen(g_vk.hdr_status), sizeof(g_vk.hdr_status) - strlen(g_vk.hdr_status), "%s", hdr_metadata_text);
        }
        return;
    }

    if (desired_kind != DXGI_SURFACE_KIND_SDR) {
        snprintf(
            g_vk.hdr_status,
            sizeof(g_vk.hdr_status),
            "DXGI reports HDR-capable output %s for %s shader output. Vulkan is using %s through %s. HDR10 candidate present: %s. Available DXGI surfaces: %s. HDR is not present to the surface.",
            output_color_space_text,
            dxgi_shader_color_space_name(g_vk.shader_color_space),
            vk_surface_kind_name(g_vk.surface_kind),
            conversion_text,
            has_hdr10_candidate ? "yes" : "no",
            available_surfaces);
        if (hdr_metadata_text[0] != '\0') {
            snprintf(g_vk.hdr_status + strlen(g_vk.hdr_status), sizeof(g_vk.hdr_status) - strlen(g_vk.hdr_status), "%s", hdr_metadata_text);
        }
        return;
    }

    snprintf(
        g_vk.hdr_status,
        sizeof(g_vk.hdr_status),
        "Vulkan is presenting %s shader output through %s using a %s on output %s. DXGI reports available surfaces: %s. HDR is not present to the surface.",
        dxgi_shader_color_space_name(g_vk.shader_color_space),
        vk_surface_kind_name(g_vk.surface_kind),
        conversion_text,
        output_color_space_text,
        available_surfaces);
    if (hdr_metadata_text[0] != '\0') {
        snprintf(g_vk.hdr_status + strlen(g_vk.hdr_status), sizeof(g_vk.hdr_status) - strlen(g_vk.hdr_status), "%s", hdr_metadata_text);
    }
}

bool vk_has_instance_extension(const char *name)
{
    uint count = 0;
    VkResult result;
    VkExtensionProperties *properties = NULL;
    bool found = false;

    result = vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
    if (result != VK_SUCCESS || count == 0) {
        return false;
    }

    properties = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * count);
    if (properties == NULL) {
        return false;
    }

    result = vkEnumerateInstanceExtensionProperties(NULL, &count, properties);
    if (result == VK_SUCCESS) {
        for (uint index = 0; index < count; index++) {
            if (strcmp(properties[index].extensionName, name) == 0) {
                found = true;
                break;
            }
        }
    }

    free(properties);
    return found;
}

bool vk_has_device_extension(VkPhysicalDevice device, const char *name)
{
    uint count = 0;
    VkResult result;
    VkExtensionProperties *properties = NULL;
    bool found = false;

    result = vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL);
    if (result != VK_SUCCESS || count == 0) {
        return false;
    }

    properties = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * count);
    if (properties == NULL) {
        return false;
    }

    result = vkEnumerateDeviceExtensionProperties(device, NULL, &count, properties);
    if (result == VK_SUCCESS) {
        for (uint index = 0; index < count; index++) {
            if (strcmp(properties[index].extensionName, name) == 0) {
                found = true;
                break;
            }
        }
    }

    free(properties);
    return found;
}

static uint vk_find_memory_type(uint memory_type_bits, VkMemoryPropertyFlags required_flags)
{
    VkPhysicalDeviceMemoryProperties memory_properties;

    vkGetPhysicalDeviceMemoryProperties(g_vk.physical_device, &memory_properties);
    for (uint index = 0; index < memory_properties.memoryTypeCount; index++) {
        const VkMemoryType *type = &memory_properties.memoryTypes[index];

        if (((memory_type_bits >> index) & 1u) == 0u) {
            continue;
        }
        if ((type->propertyFlags & required_flags) == required_flags) {
            return index;
        }
    }

    return UINT_MAX;
}

static bool vk_create_buffer(
    VkDeviceSize size_bytes,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_flags,
    VkBuffer *buffer_out,
    VkDeviceMemory *memory_out,
    void **mapped_out)
{
    VkBufferCreateInfo buffer_info = {0};
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo alloc_info = {0};
    uint memory_type_index;
    VkResult result;

    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size_bytes;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(g_vk.device, &buffer_info, NULL, buffer_out);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateBuffer", result);
        return false;
    }

    vkGetBufferMemoryRequirements(g_vk.device, *buffer_out, &memory_requirements);
    memory_type_index = vk_find_memory_type(memory_requirements.memoryTypeBits, memory_flags);
    if (memory_type_index == UINT_MAX) {
        vk_set_error_text("Vulkan backend could not find a matching buffer memory type.");
        return false;
    }

    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = memory_type_index;

    result = vkAllocateMemory(g_vk.device, &alloc_info, NULL, memory_out);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkAllocateMemory(buffer)", result);
        return false;
    }

    result = vkBindBufferMemory(g_vk.device, *buffer_out, *memory_out, 0);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkBindBufferMemory", result);
        return false;
    }

    if (mapped_out != NULL) {
        result = vkMapMemory(g_vk.device, *memory_out, 0, size_bytes, 0, mapped_out);
        if (result != VK_SUCCESS) {
            vk_set_error_result("vkMapMemory(buffer)", result);
            return false;
        }
    }

    return true;
}

static bool vk_create_image(
    int width,
    int height,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImage *image_out,
    VkDeviceMemory *memory_out)
{
    VkImageCreateInfo image_info = {0};
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo alloc_info = {0};
    uint memory_type_index;
    VkResult result;

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.extent.width = (uint)width;
    image_info.extent.height = (uint)height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = vkCreateImage(g_vk.device, &image_info, NULL, image_out);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateImage", result);
        return false;
    }

    vkGetImageMemoryRequirements(g_vk.device, *image_out, &memory_requirements);
    memory_type_index = vk_find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_type_index == UINT_MAX) {
        vk_set_error_text("Vulkan backend could not find device-local image memory.");
        return false;
    }

    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = memory_type_index;

    result = vkAllocateMemory(g_vk.device, &alloc_info, NULL, memory_out);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkAllocateMemory(image)", result);
        return false;
    }

    result = vkBindImageMemory(g_vk.device, *image_out, *memory_out, 0);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkBindImageMemory", result);
        return false;
    }

    return true;
}

static bool vk_create_image_view(VkImage image, VkFormat format, VkImageView *view_out)
{
    VkImageViewCreateInfo view_info = {0};
    VkResult result;

    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;

    result = vkCreateImageView(g_vk.device, &view_info, NULL, view_out);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateImageView", result);
        return false;
    }

    return true;
}

static bool vk_create_shader_module(const uint *words, size_t word_count, VkShaderModule *module_out)
{
    VkShaderModuleCreateInfo create_info = {0};
    VkResult result;

    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = word_count * sizeof(uint);
    create_info.pCode = words;

    result = vkCreateShaderModule(g_vk.device, &create_info, NULL, module_out);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateShaderModule", result);
        return false;
    }

    return true;
}

static uint16_t vk_float_to_half(float value)
{
    union {
        float f;
        uint  u;
    } bits;
    uint sign;
    int exponent;
    uint mantissa;

    bits.f = value;
    sign = (bits.u >> 16) & 0x8000u;
    exponent = (int)((bits.u >> 23) & 0xFFu) - 127 + 15;
    mantissa = bits.u & 0x007FFFFFu;

    if (((bits.u >> 23) & 0xFFu) == 0xFFu) {
        if (mantissa != 0u) {
            return (uint16_t)(sign | 0x7E00u);
        }
        return (uint16_t)(sign | 0x7C00u);
    }

    if (exponent <= 0) {
        if (exponent < -10) {
            return (uint16_t)sign;
        }

        mantissa |= 0x00800000u;
        mantissa >>= (uint)(1 - exponent);
        return (uint16_t)(sign | ((mantissa + 0x00001000u) >> 13));
    }

    if (exponent >= 31) {
        return (uint16_t)(sign | 0x7C00u);
    }

    return (uint16_t)(sign | ((uint)exponent << 10) | ((mantissa + 0x00001000u) >> 13));
}

void vk_destroy_device_shader_resources(void)
{
    if (g_vk.device != VK_NULL_HANDLE && g_vk.framebuffers != NULL) {
        for (uint index = 0; index < g_vk.swapchain_image_count; index++) {
            if (g_vk.framebuffers[index] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(g_vk.device, g_vk.framebuffers[index], NULL);
            }
        }
    }
    free(g_vk.framebuffers);
    g_vk.framebuffers = NULL;

    if (g_vk.device != VK_NULL_HANDLE && g_vk.swapchain_image_views != NULL) {
        for (uint index = 0; index < g_vk.swapchain_image_count; index++) {
            if (g_vk.swapchain_image_views[index] != VK_NULL_HANDLE) {
                vkDestroyImageView(g_vk.device, g_vk.swapchain_image_views[index], NULL);
            }
        }
    }
    free(g_vk.swapchain_image_views);
    g_vk.swapchain_image_views = NULL;

    if (g_vk.device != VK_NULL_HANDLE && g_vk.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_vk.device, g_vk.pipeline, NULL);
        g_vk.pipeline = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_vk.device, g_vk.render_pass, NULL);
        g_vk.render_pass = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_vk.device, g_vk.pipeline_layout, NULL);
        g_vk.pipeline_layout = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_vk.device, g_vk.descriptor_pool, NULL);
        g_vk.descriptor_pool = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_vk.device, g_vk.descriptor_set_layout, NULL);
        g_vk.descriptor_set_layout = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(g_vk.device, g_vk.sampler, NULL);
        g_vk.sampler = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.source_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(g_vk.device, g_vk.source_image_view, NULL);
        g_vk.source_image_view = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.source_image != VK_NULL_HANDLE) {
        vkDestroyImage(g_vk.device, g_vk.source_image, NULL);
        g_vk.source_image = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.source_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk.device, g_vk.source_memory, NULL);
        g_vk.source_memory = VK_NULL_HANDLE;
    }

    g_vk.descriptor_set = VK_NULL_HANDLE;
    g_vk.source_initialized = false;
}

bool vk_create_device_shader_resources(void)
{
    VkSamplerCreateInfo sampler_info = {0};
    VkDescriptorSetLayoutBinding binding = {0};
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {0};
    VkDescriptorPoolSize pool_size = {0};
    VkDescriptorPoolCreateInfo descriptor_pool_info = {0};
    VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {0};
    VkDescriptorImageInfo descriptor_image_info = {0};
    VkWriteDescriptorSet descriptor_write = {0};
    VkPushConstantRange push_range = {0};
    VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
    VkAttachmentDescription attachment = {0};
    VkAttachmentReference attachment_ref = {0};
    VkSubpassDescription subpass = {0};
    VkRenderPassCreateInfo render_pass_info = {0};
    VkShaderModule vert_module = VK_NULL_HANDLE;
    VkShaderModule frag_module = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo shader_stages[2] = {0};
    VkPipelineVertexInputStateCreateInfo vertex_input = {0};
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
    VkPipelineViewportStateCreateInfo viewport_state = {0};
    VkPipelineRasterizationStateCreateInfo raster = {0};
    VkPipelineMultisampleStateCreateInfo multisample = {0};
    VkPipelineColorBlendAttachmentState blend_attachment = {0};
    VkPipelineColorBlendStateCreateInfo blend = {0};
    VkPipelineDynamicStateCreateInfo dynamic_state = {0};
    VkDynamicState dynamic_states[2];
    VkGraphicsPipelineCreateInfo pipeline_info = {0};
    VkFramebufferCreateInfo framebuffer_info = {0};
    VkResult result;

    if (!vk_create_image(
            g_vk.render_width,
            g_vk.render_height,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            &g_vk.source_image,
            &g_vk.source_memory))
    {
        return false;
    }

    if (!vk_create_image_view(g_vk.source_image, VK_FORMAT_R32G32B32A32_SFLOAT, &g_vk.source_image_view)) {
        return false;
    }

    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.maxAnisotropy = 1.0f;
    result = vkCreateSampler(g_vk.device, &sampler_info, NULL, &g_vk.sampler);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateSampler", result);
        return false;
    }

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_info.bindingCount = 1;
    descriptor_set_layout_info.pBindings = &binding;
    result = vkCreateDescriptorSetLayout(g_vk.device, &descriptor_set_layout_info, NULL, &g_vk.descriptor_set_layout);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateDescriptorSetLayout", result);
        return false;
    }

    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;
    descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.maxSets = 1;
    descriptor_pool_info.poolSizeCount = 1;
    descriptor_pool_info.pPoolSizes = &pool_size;
    result = vkCreateDescriptorPool(g_vk.device, &descriptor_pool_info, NULL, &g_vk.descriptor_pool);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateDescriptorPool", result);
        return false;
    }

    descriptor_set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_alloc_info.descriptorPool = g_vk.descriptor_pool;
    descriptor_set_alloc_info.descriptorSetCount = 1;
    descriptor_set_alloc_info.pSetLayouts = &g_vk.descriptor_set_layout;
    result = vkAllocateDescriptorSets(g_vk.device, &descriptor_set_alloc_info, &g_vk.descriptor_set);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkAllocateDescriptorSets", result);
        return false;
    }

    descriptor_image_info.sampler = g_vk.sampler;
    descriptor_image_info.imageView = g_vk.source_image_view;
    descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = g_vk.descriptor_set;
    descriptor_write.dstBinding = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.pImageInfo = &descriptor_image_info;
    vkUpdateDescriptorSets(g_vk.device, 1, &descriptor_write, 0, NULL);

    push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.size = sizeof(vk_present_push_constants_t);
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &g_vk.descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_range;
    result = vkCreatePipelineLayout(g_vk.device, &pipeline_layout_info, NULL, &g_vk.pipeline_layout);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreatePipelineLayout", result);
        return false;
    }

    attachment.format = g_vk.swapchain_format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment_ref.attachment = 0;
    attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachment_ref;
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    result = vkCreateRenderPass(g_vk.device, &render_pass_info, NULL, &g_vk.render_pass);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateRenderPass", result);
        return false;
    }

    if (!vk_create_shader_module(g_vk_present_vert_spv, g_vk_present_vert_spv_count, &vert_module) ||
        !vk_create_shader_module(g_vk_present_frag_spv, g_vk_present_frag_spv_count, &frag_module))
    {
        if (vert_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(g_vk.device, vert_module, NULL);
        }
        if (frag_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(g_vk.device, frag_module, NULL);
        }
        return false;
    }

    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = vert_module;
    shader_stages[0].pName = "main";
    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = frag_module;
    shader_stages[1].pName = "main";

    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_attachment;
    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &raster;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pColorBlendState = &blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = g_vk.pipeline_layout;
    pipeline_info.renderPass = g_vk.render_pass;
    result = vkCreateGraphicsPipelines(g_vk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &g_vk.pipeline);
    vkDestroyShaderModule(g_vk.device, vert_module, NULL);
    vkDestroyShaderModule(g_vk.device, frag_module, NULL);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateGraphicsPipelines", result);
        return false;
    }

    g_vk.swapchain_image_views = (VkImageView *)calloc(g_vk.swapchain_image_count, sizeof(VkImageView));
    g_vk.framebuffers = (VkFramebuffer *)calloc(g_vk.swapchain_image_count, sizeof(VkFramebuffer));
    if (g_vk.swapchain_image_views == NULL || g_vk.framebuffers == NULL) {
        vk_set_error_text("Failed to allocate Vulkan shader-present image views.");
        return false;
    }

    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = g_vk.render_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.width = (uint)g_vk.swapchain_width;
    framebuffer_info.height = (uint)g_vk.swapchain_height;
    framebuffer_info.layers = 1;
    for (uint index = 0; index < g_vk.swapchain_image_count; index++) {
        if (!vk_create_image_view(g_vk.swapchain_images[index], g_vk.swapchain_format, &g_vk.swapchain_image_views[index])) {
            return false;
        }

        framebuffer_info.pAttachments = &g_vk.swapchain_image_views[index];
        result = vkCreateFramebuffer(g_vk.device, &framebuffer_info, NULL, &g_vk.framebuffers[index]);
        if (result != VK_SUCCESS) {
            vk_set_error_result("vkCreateFramebuffer", result);
            return false;
        }
    }

    return true;
}

void vk_pack_surface(const f32x4_surface_t *surface)
{
    if (g_vk.conversion_mode == PRESENT_CONVERSION_DEVICE_SHADER) {
        size_t row_bytes = (size_t)surface->width * sizeof(vec4_t);

        if (surface->stride_bytes == (int)row_bytes) {
            memcpy(g_vk.staging_mapped, surface->pixels, row_bytes * (size_t)surface->height);
            return;
        }

        for (int y = 0; y < surface->height; y++) {
            const char *src_row = (const char *)surface->pixels + (size_t)y * (size_t)surface->stride_bytes;
            char *dst_row = (char *)g_vk.staging_mapped + (size_t)y * row_bytes;
            memcpy(dst_row, src_row, row_bytes);
        }
        return;
    }

    if (g_vk.surface_kind == VK_SURFACE_KIND_HDR_SCRGB) {
        uint16_t *dst = (uint16_t *)g_vk.staging_mapped;

        for (int y = 0; y < surface->height; y++) {
            /* The CPU shader surface uses the project's bottom-origin convention. */
            const vec4_t *src = (const vec4_t *)((const char *)surface->pixels + (size_t)(surface->height - 1 - y) * (size_t)surface->stride_bytes);
            for (int x = 0; x < surface->width; x++) {
                vec3_t scrgb = present_encode_scrgb_linear(
                    g_vk.shader_color_space,
                    vec3(src[x].x, src[x].y, src[x].z));
                *dst++ = vk_float_to_half(scrgb.x);
                *dst++ = vk_float_to_half(scrgb.y);
                *dst++ = vk_float_to_half(scrgb.z);
                *dst++ = vk_float_to_half(present_saturate(src[x].w));
            }
        }
        return;
    }

    for (int y = 0; y < surface->height; y++) {
        const vec4_t *src = (const vec4_t *)((const char *)surface->pixels + (size_t)(surface->height - 1 - y) * (size_t)surface->stride_bytes);
        BYTE *dst = (BYTE *)g_vk.staging_mapped + (size_t)y * (size_t)surface->width * 4u;

        for (int x = 0; x < surface->width; x++) {
            vec3_t sdr = present_encode_sdr_display(
                g_vk.shader_color_space,
                vec3(src[x].x, src[x].y, src[x].z));
            if (g_vk.surface_kind == VK_SURFACE_KIND_SDR_BGRA8) {
                dst[x * 4 + 0] = present_float_to_byte(sdr.z);
                dst[x * 4 + 1] = present_float_to_byte(sdr.y);
                dst[x * 4 + 2] = present_float_to_byte(sdr.x);
                dst[x * 4 + 3] = present_float_to_byte(src[x].w);
            } else if (g_vk.surface_kind == VK_SURFACE_KIND_HDR10) {
                dst[x * 4 + 0] = present_float_to_byte(sdr.x);
                dst[x * 4 + 1] = present_float_to_byte(sdr.y);
                dst[x * 4 + 2] = present_float_to_byte(sdr.z);
                dst[x * 4 + 3] = present_float_to_byte(src[x].w);
            } else {
                dst[x * 4 + 0] = present_float_to_byte(sdr.x);
                dst[x * 4 + 1] = present_float_to_byte(sdr.y);
                dst[x * 4 + 2] = present_float_to_byte(sdr.z);
                dst[x * 4 + 3] = present_float_to_byte(src[x].w);
            }
        }
    }
}

static void vk_cmd_image_barrier(
    VkCommandBuffer command_buffer,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout)
{
    VkImageMemoryBarrier barrier = {0};
    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.dstAccessMask = 0;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(
        command_buffer,
        src_stage,
        dst_stage,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &barrier);
}

static bool vk_pick_surface_format(VkSurfaceFormatKHR *format_out)
{
    uint format_count = 0;
    VkSurfaceFormatKHR *formats = NULL;
    VkSurfaceFormatKHR scrgb_format = {0};
    VkSurfaceFormatKHR hdr10_format = {0};
    VkSurfaceFormatKHR sdr_bgra8_format = {0};
    VkSurfaceFormatKHR sdr_rgba8_format = {0};
    VkSurfaceFormatKHR fallback_format = {0};
    VkResult result;
    bool found = false;
    bool has_scrgb_candidate = false;
    bool has_sdr_bgra8_candidate = false;
    bool has_sdr_rgba8_candidate = false;
    bool has_hdr10_candidate = false;
    dxgi_surface_kind_t desired_kind = dxgi_choose_surface_kind(&g_vk.dxgi, g_vk.shader_color_space);

    result = vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk.physical_device, g_vk.surface, &format_count, NULL);
    if (result != VK_SUCCESS || format_count == 0) {
        vk_set_error_result("vkGetPhysicalDeviceSurfaceFormatsKHR(count)", result);
        return false;
    }

    formats = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * format_count);
    if (formats == NULL) {
        vk_set_error_text("Failed to allocate Vulkan surface format list.");
        return false;
    }

    result = vkGetPhysicalDeviceSurfaceFormatsKHR(g_vk.physical_device, g_vk.surface, &format_count, formats);
    if (result != VK_SUCCESS) {
        free(formats);
        vk_set_error_result("vkGetPhysicalDeviceSurfaceFormatsKHR(list)", result);
        return false;
    }

    fallback_format = formats[0];

    for (uint index = 0; index < format_count; index++) {
        if (formats[index].format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
            formats[index].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
        {
            has_hdr10_candidate = true;
            hdr10_format = formats[index];
        }

        if (!has_scrgb_candidate &&
            formats[index].format == VK_FORMAT_R16G16B16A16_SFLOAT &&
            formats[index].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
        {
            scrgb_format = formats[index];
            has_scrgb_candidate = true;
        }

        if (!has_sdr_bgra8_candidate &&
            formats[index].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            sdr_bgra8_format = formats[index];
            has_sdr_bgra8_candidate = true;
        }

        if (!has_sdr_rgba8_candidate &&
            formats[index].format == VK_FORMAT_R8G8B8A8_UNORM &&
            formats[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            sdr_rgba8_format = formats[index];
            has_sdr_rgba8_candidate = true;
        }
    }

    if (desired_kind == DXGI_SURFACE_KIND_HDR10 && has_hdr10_candidate) {
        *format_out = hdr10_format;
        g_vk.surface_kind = VK_SURFACE_KIND_HDR10;
        found = true;
    }

    if (!found && desired_kind != DXGI_SURFACE_KIND_SDR && has_scrgb_candidate) {
        *format_out = scrgb_format;
        g_vk.surface_kind = VK_SURFACE_KIND_HDR_SCRGB;
        found = true;
    }

    if (!found && has_sdr_bgra8_candidate) {
        *format_out = sdr_bgra8_format;
        g_vk.surface_kind = VK_SURFACE_KIND_SDR_BGRA8;
        found = true;
    }

    if (!found && has_sdr_rgba8_candidate) {
        *format_out = sdr_rgba8_format;
        g_vk.surface_kind = VK_SURFACE_KIND_SDR_RGBA8;
        found = true;
    }

    if (!found && has_scrgb_candidate) {
        *format_out = scrgb_format;
        g_vk.surface_kind = VK_SURFACE_KIND_HDR_SCRGB;
        found = true;
    }

    if (!found) {
        *format_out = fallback_format;
        if (fallback_format.format == VK_FORMAT_B8G8R8A8_UNORM || fallback_format.format == VK_FORMAT_B8G8R8A8_SRGB) {
            g_vk.surface_kind = VK_SURFACE_KIND_SDR_BGRA8;
            found = true;
        } else if (fallback_format.format == VK_FORMAT_R8G8B8A8_UNORM || fallback_format.format == VK_FORMAT_R8G8B8A8_SRGB) {
            g_vk.surface_kind = VK_SURFACE_KIND_SDR_RGBA8;
            found = true;
        } else if (fallback_format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32) {
            g_vk.surface_kind = VK_SURFACE_KIND_HDR10;
            found = true;
        } else if (fallback_format.format == VK_FORMAT_R16G16B16A16_SFLOAT) {
            g_vk.surface_kind = VK_SURFACE_KIND_HDR_SCRGB;
            found = true;
        }
    }

    free(formats);

    if (!found) {
        vk_set_error_text("Vulkan surface does not expose a supported present format for this backend.");
        return false;
    }

    vk_update_hdr_status(has_hdr10_candidate);

    return true;
}

static VkPresentModeKHR vk_pick_present_mode(void)
{
    uint count = 0;
    VkPresentModeKHR *modes = NULL;
    VkPresentModeKHR picked = VK_PRESENT_MODE_FIFO_KHR;
    VkResult result;

    result = vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.physical_device, g_vk.surface, &count, NULL);
    if (result != VK_SUCCESS || count == 0) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    modes = (VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR) * count);
    if (modes == NULL) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    result = vkGetPhysicalDeviceSurfacePresentModesKHR(g_vk.physical_device, g_vk.surface, &count, modes);
    if (result == VK_SUCCESS) {
        for (uint index = 0; index < count; index++) {
            if (g_vk.vsync_enabled) {
                if (modes[index] == VK_PRESENT_MODE_FIFO_KHR) {
                    picked = VK_PRESENT_MODE_FIFO_KHR;
                    break;
                }
            } else {
                if (modes[index] == VK_PRESENT_MODE_MAILBOX_KHR) {
                    picked = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
                if (modes[index] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    picked = VK_PRESENT_MODE_IMMEDIATE_KHR;
                }
            }
        }
    }

    free(modes);
    return picked;
}

static size_t vk_surface_pixel_size(void)
{
    return (g_vk.surface_kind == VK_SURFACE_KIND_HDR_SCRGB) ? 8u : 4u;
}

bool vk_pick_physical_device(void)
{
    uint device_count = 0;
    VkPhysicalDevice *devices = NULL;
    VkResult result;

    result = vkEnumeratePhysicalDevices(g_vk.instance, &device_count, NULL);
    if (result != VK_SUCCESS || device_count == 0u) {
        vk_set_error_text("No Vulkan physical devices were found.");
        return false;
    }

    devices = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * device_count);
    if (devices == NULL) {
        vk_set_error_text("Failed to allocate Vulkan physical device list.");
        return false;
    }

    result = vkEnumeratePhysicalDevices(g_vk.instance, &device_count, devices);
    if (result != VK_SUCCESS) {
        free(devices);
        vk_set_error_result("vkEnumeratePhysicalDevices(list)", result);
        return false;
    }

    for (uint device_index = 0; device_index < device_count; device_index++) {
        uint queue_count = 0;
        VkQueueFamilyProperties *queue_properties = NULL;

        vkGetPhysicalDeviceQueueFamilyProperties(devices[device_index], &queue_count, NULL);
        if (queue_count == 0u) {
            continue;
        }

        queue_properties = (VkQueueFamilyProperties *)malloc(sizeof(VkQueueFamilyProperties) * queue_count);
        if (queue_properties == NULL) {
            continue;
        }

        vkGetPhysicalDeviceQueueFamilyProperties(devices[device_index], &queue_count, queue_properties);
        for (uint queue_index = 0; queue_index < queue_count; queue_index++) {
            VkBool32 present_supported = VK_FALSE;

            if ((queue_properties[queue_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u) {
                continue;
            }

            result = vkGetPhysicalDeviceSurfaceSupportKHR(devices[device_index], queue_index, g_vk.surface, &present_supported);
            if (result == VK_SUCCESS && present_supported == VK_TRUE) {
                g_vk.physical_device = devices[device_index];
                g_vk.queue_family_index = queue_index;
                free(queue_properties);
                free(devices);
                return true;
            }
        }

        free(queue_properties);
    }

    free(devices);
    vk_set_error_text("No Vulkan queue family supports Win32 presentation for this surface.");
    return false;
}

void vk_destroy_swapchain_resources(void)
{
    vk_destroy_device_shader_resources();

    if (g_vk.device != VK_NULL_HANDLE && g_vk.staging_mapped != NULL) {
        vkUnmapMemory(g_vk.device, g_vk.staging_memory);
        g_vk.staging_mapped = NULL;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.staging_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_vk.device, g_vk.staging_buffer, NULL);
        g_vk.staging_buffer = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.staging_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk.device, g_vk.staging_memory, NULL);
        g_vk.staging_memory = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.intermediate_image != VK_NULL_HANDLE) {
        vkDestroyImage(g_vk.device, g_vk.intermediate_image, NULL);
        g_vk.intermediate_image = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.intermediate_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_vk.device, g_vk.intermediate_memory, NULL);
        g_vk.intermediate_memory = VK_NULL_HANDLE;
    }
    if (g_vk.device != VK_NULL_HANDLE && g_vk.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
        g_vk.swapchain = VK_NULL_HANDLE;
    }

    free(g_vk.swapchain_images);
    g_vk.swapchain_images = NULL;
    free(g_vk.swapchain_image_initialized);
    g_vk.swapchain_image_initialized = NULL;
    g_vk.swapchain_image_count = 0;
    g_vk.staging_size = 0;
    g_vk.intermediate_initialized = false;
    g_vk.can_scale_blit = false;
    g_vk.swapchain_supports_transfer_dst = false;
    g_vk.swapchain_supports_color_attachment = false;
    g_vk.swapchain_width = 0;
    g_vk.swapchain_height = 0;
}

bool vk_create_swapchain_resources(void)
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    VkSurfaceFormatKHR surface_format = {0};
    VkSwapchainCreateInfoKHR swapchain_info = {0};
    VkMemoryRequirements staging_requirements;
    VkFormatProperties format_properties;
    VkResult result;
    uint min_image_count;

    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.physical_device, g_vk.surface, &capabilities);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result);
        return false;
    }

    g_vk.swapchain_supports_transfer_dst = (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0u;
    g_vk.swapchain_supports_color_attachment = (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0u;

    if (!vk_pick_surface_format(&surface_format)) {
        return false;
    }

    g_vk.swapchain_format = surface_format.format;
    g_vk.swapchain_color_space = surface_format.colorSpace;
    g_vk.present_mode = vk_pick_present_mode();

    if (capabilities.currentExtent.width != UINT32_MAX) {
        g_vk.swapchain_width = (int)capabilities.currentExtent.width;
        g_vk.swapchain_height = (int)capabilities.currentExtent.height;
    } else {
        g_vk.swapchain_width = max(1, g_vk.popup_width);
        g_vk.swapchain_height = max(1, g_vk.popup_height);
        if ((uint)g_vk.swapchain_width < capabilities.minImageExtent.width) {
            g_vk.swapchain_width = (int)capabilities.minImageExtent.width;
        }
        if ((uint)g_vk.swapchain_width > capabilities.maxImageExtent.width) {
            g_vk.swapchain_width = (int)capabilities.maxImageExtent.width;
        }
        if ((uint)g_vk.swapchain_height < capabilities.minImageExtent.height) {
            g_vk.swapchain_height = (int)capabilities.minImageExtent.height;
        }
        if ((uint)g_vk.swapchain_height > capabilities.maxImageExtent.height) {
            g_vk.swapchain_height = (int)capabilities.maxImageExtent.height;
        }
    }

    min_image_count = capabilities.minImageCount + 1u;
    if (capabilities.maxImageCount != 0u && min_image_count > capabilities.maxImageCount) {
        min_image_count = capabilities.maxImageCount;
    }

    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = g_vk.surface;
    swapchain_info.minImageCount = min_image_count;
    swapchain_info.imageFormat = g_vk.swapchain_format;
    swapchain_info.imageColorSpace = g_vk.swapchain_color_space;
    swapchain_info.imageExtent.width = (uint)g_vk.swapchain_width;
    swapchain_info.imageExtent.height = (uint)g_vk.swapchain_height;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage =
        (g_vk.swapchain_supports_transfer_dst ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0u) |
        (g_vk.swapchain_supports_color_attachment ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0u);
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform = capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = g_vk.present_mode;
    swapchain_info.clipped = VK_TRUE;

    result = vkCreateSwapchainKHR(g_vk.device, &swapchain_info, NULL, &g_vk.swapchain);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkCreateSwapchainKHR", result);
        return false;
    }

    result = vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, &g_vk.swapchain_image_count, NULL);
    if (result != VK_SUCCESS || g_vk.swapchain_image_count == 0u) {
        vk_set_error_result("vkGetSwapchainImagesKHR(count)", result);
        return false;
    }

    g_vk.swapchain_images = (VkImage *)malloc(sizeof(VkImage) * g_vk.swapchain_image_count);
    g_vk.swapchain_image_initialized = (bool *)calloc(g_vk.swapchain_image_count, sizeof(bool));
    if (g_vk.swapchain_images == NULL || g_vk.swapchain_image_initialized == NULL) {
        vk_set_error_text("Failed to allocate Vulkan swapchain image state.");
        return false;
    }

    result = vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, &g_vk.swapchain_image_count, g_vk.swapchain_images);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkGetSwapchainImagesKHR(list)", result);
        return false;
    }

    if (!vk_create_buffer(
            (VkDeviceSize)((size_t)g_vk.render_width * (size_t)g_vk.render_height * max((int)sizeof(vec4_t), (int)vk_surface_pixel_size())),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &g_vk.staging_buffer,
            &g_vk.staging_memory,
            &g_vk.staging_mapped))
    {
        return false;
    }

    vkGetBufferMemoryRequirements(g_vk.device, g_vk.staging_buffer, &staging_requirements);
    g_vk.staging_size = (size_t)staging_requirements.size;

    if (g_vk.swapchain_supports_transfer_dst) {
        if (!vk_create_image(
                g_vk.render_width,
                g_vk.render_height,
                g_vk.swapchain_format,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                &g_vk.intermediate_image,
                &g_vk.intermediate_memory))
        {
            return false;
        }

        vkGetPhysicalDeviceFormatProperties(g_vk.physical_device, g_vk.swapchain_format, &format_properties);
        g_vk.can_scale_blit =
            (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0u &&
            (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0u;
    }

    return true;
}

bool vk_apply_hdr_metadata(void)
{
    VkHdrMetadataEXT metadata = {0};

    if (!g_vk.hdr_metadata_enabled || g_vk.set_hdr_metadata_ext == NULL) {
        g_vk.hdr_metadata_applied = false;
        return true;
    }

    if (g_vk.swapchain == VK_NULL_HANDLE || g_vk.surface_kind != VK_SURFACE_KIND_HDR10) {
        g_vk.hdr_metadata_applied = false;
        return true;
    }

    metadata.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
    metadata.displayPrimaryRed.x = 0.680f;
    metadata.displayPrimaryRed.y = 0.320f;
    metadata.displayPrimaryGreen.x = 0.265f;
    metadata.displayPrimaryGreen.y = 0.690f;
    metadata.displayPrimaryBlue.x = 0.150f;
    metadata.displayPrimaryBlue.y = 0.060f;
    metadata.whitePoint.x = 0.3127f;
    metadata.whitePoint.y = 0.3290f;
    metadata.maxLuminance = 1000.0f;
    metadata.minLuminance = 0.001f;
    metadata.maxContentLightLevel = 1000.0f;
    metadata.maxFrameAverageLightLevel = 400.0f;

    g_vk.set_hdr_metadata_ext(g_vk.device, 1, &g_vk.swapchain, &metadata);
    g_vk.hdr_metadata_applied = true;
    return true;
}

bool vk_record_present_commands(uint image_index)
{
    VkCommandBufferBeginInfo begin_info = {0};
    VkBufferImageCopy buffer_copy = {0};
    VkImageBlit image_blit = {0};
    VkImageCopy image_copy = {0};
    VkViewport viewport = {0};
    VkRect2D scissor = {0};
    VkClearValue clear_value = {0};
    VkRenderPassBeginInfo render_pass_info = {0};
    vk_present_push_constants_t push_constants = {0};
    VkResult result;
    VkImage swapchain_image;

    if (image_index >= g_vk.swapchain_image_count) {
        vk_set_error_text("Vulkan swapchain image index is invalid.");
        return false;
    }

    swapchain_image = g_vk.swapchain_images[image_index];

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = vkBeginCommandBuffer(g_vk.command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkBeginCommandBuffer", result);
        return false;
    }

    if (g_vk.conversion_mode == PRESENT_CONVERSION_DEVICE_SHADER) {
        vk_cmd_image_barrier(
            g_vk.command_buffer,
            g_vk.source_image,
            g_vk.source_initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        buffer_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        buffer_copy.imageSubresource.layerCount = 1;
        buffer_copy.imageExtent.width = (uint)g_vk.render_width;
        buffer_copy.imageExtent.height = (uint)g_vk.render_height;
        buffer_copy.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(
            g_vk.command_buffer,
            g_vk.staging_buffer,
            g_vk.source_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &buffer_copy);

        vk_cmd_image_barrier(
            g_vk.command_buffer,
            g_vk.source_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vk_cmd_image_barrier(
            g_vk.command_buffer,
            swapchain_image,
            g_vk.swapchain_image_initialized[image_index] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        clear_value.color.float32[0] = 0.02f;
        clear_value.color.float32[1] = 0.02f;
        clear_value.color.float32[2] = 0.02f;
        clear_value.color.float32[3] = 1.0f;
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = g_vk.render_pass;
        render_pass_info.framebuffer = g_vk.framebuffers[image_index];
        render_pass_info.renderArea.extent.width = (uint)g_vk.swapchain_width;
        render_pass_info.renderArea.extent.height = (uint)g_vk.swapchain_height;
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear_value;

        viewport.x = 0.0f;
        viewport.y = (float)g_vk.swapchain_height;
        viewport.width = (float)g_vk.swapchain_width;
        viewport.height = -(float)g_vk.swapchain_height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        scissor.extent.width = (uint)g_vk.swapchain_width;
        scissor.extent.height = (uint)g_vk.swapchain_height;

        push_constants.image_width = g_vk.render_width;
        push_constants.image_height = g_vk.render_height;
        push_constants.shader_color_space = (int)g_vk.shader_color_space;
        push_constants.surface_kind =
            (g_vk.surface_kind == VK_SURFACE_KIND_HDR_SCRGB)
                ? 1
                : ((g_vk.surface_kind == VK_SURFACE_KIND_HDR10) ? 2 : 0);

        vkCmdBeginRenderPass(g_vk.command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(g_vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.pipeline);
        vkCmdSetViewport(g_vk.command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(g_vk.command_buffer, 0, 1, &scissor);
        vkCmdBindDescriptorSets(
            g_vk.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            g_vk.pipeline_layout,
            0,
            1,
            &g_vk.descriptor_set,
            0,
            NULL);
        vkCmdPushConstants(
            g_vk.command_buffer,
            g_vk.pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(push_constants),
            &push_constants);
        vkCmdDraw(g_vk.command_buffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(g_vk.command_buffer);

        vk_cmd_image_barrier(
            g_vk.command_buffer,
            swapchain_image,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        result = vkEndCommandBuffer(g_vk.command_buffer);
        if (result != VK_SUCCESS) {
            vk_set_error_result("vkEndCommandBuffer", result);
            return false;
        }

        return true;
    }

    vk_cmd_image_barrier(
        g_vk.command_buffer,
        g_vk.intermediate_image,
        g_vk.intermediate_initialized ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    buffer_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    buffer_copy.imageSubresource.layerCount = 1;
    buffer_copy.imageExtent.width = (uint)g_vk.render_width;
    buffer_copy.imageExtent.height = (uint)g_vk.render_height;
    buffer_copy.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(
        g_vk.command_buffer,
        g_vk.staging_buffer,
        g_vk.intermediate_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &buffer_copy);

    vk_cmd_image_barrier(
        g_vk.command_buffer,
        g_vk.intermediate_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vk_cmd_image_barrier(
        g_vk.command_buffer,
        swapchain_image,
        g_vk.swapchain_image_initialized[image_index] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    if (g_vk.swapchain_width == g_vk.render_width && g_vk.swapchain_height == g_vk.render_height) {
        image_copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy.srcSubresource.layerCount = 1;
        image_copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy.dstSubresource.layerCount = 1;
        image_copy.extent.width = (uint)g_vk.render_width;
        image_copy.extent.height = (uint)g_vk.render_height;
        image_copy.extent.depth = 1;

        vkCmdCopyImage(
            g_vk.command_buffer,
            g_vk.intermediate_image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &image_copy);
    } else {
        image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_blit.srcSubresource.layerCount = 1;
        image_blit.srcOffsets[1].x = g_vk.render_width;
        image_blit.srcOffsets[1].y = g_vk.render_height;
        image_blit.srcOffsets[1].z = 1;
        image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_blit.dstSubresource.layerCount = 1;
        image_blit.dstOffsets[1].x = g_vk.swapchain_width;
        image_blit.dstOffsets[1].y = g_vk.swapchain_height;
        image_blit.dstOffsets[1].z = 1;

        vkCmdBlitImage(
            g_vk.command_buffer,
            g_vk.intermediate_image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &image_blit,
            VK_FILTER_LINEAR);
    }

    vk_cmd_image_barrier(
        g_vk.command_buffer,
        swapchain_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    result = vkEndCommandBuffer(g_vk.command_buffer);
    if (result != VK_SUCCESS) {
        vk_set_error_result("vkEndCommandBuffer", result);
        return false;
    }

    return true;
}

