#include "librashader_manager.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_librashader_hack.h"
#include <SDL3/SDL.h>
#include <stdlib.h>

// Enable Vulkan runtime for librashader
#define LIBRA_RUNTIME_VULKAN

#include "librashader.h"
#include <SDL3/SDL_vulkan.h>

// Forward declarations for Vulkan functions we need to load manually
typedef void(VKAPI_PTR* PFN_vkGetDeviceQueue)(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex,
                                              VkQueue* pQueue);
typedef void(VKAPI_PTR* PFN_vkCmdPipelineBarrier)(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask,
                                                  VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags,
                                                  uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers,
                                                  uint32_t bufferMemoryBarrierCount,
                                                  const VkBufferMemoryBarrier* pBufferMemoryBarriers,
                                                  uint32_t imageMemoryBarrierCount,
                                                  const VkImageMemoryBarrier* pImageMemoryBarriers);
typedef void(VKAPI_PTR* PFN_vkCmdBlitImage)(VkCommandBuffer commandBuffer, VkImage srcImage,
                                            VkImageLayout srcImageLayout, VkImage dstImage,
                                            VkImageLayout dstImageLayout, uint32_t regionCount,
                                            const VkImageBlit* pRegions, VkFilter filter);

typedef struct LibrashaderManagerGPU {
    libra_shader_preset_t preset;
    uint64_t frame_count;

    // Vulkan state
    libra_vk_filter_chain_t vk_filter_chain;
    VkDevice vk_device;
    VkQueue vk_queue;
    VkPhysicalDevice vk_physical_device;
    VkInstance vk_instance;
    uint32_t vk_queue_family_index;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdBlitImage vkCmdBlitImage;
} LibrashaderManagerGPU;

// =============================================================================
// Vulkan Backend
// =============================================================================

static LibrashaderManagerGPU* LibrashaderManager_Init_Vulkan(const char* preset_path, SDL_GPUDevice* gpu_device) {
    // --- HACK: Extract Vulkan Handles ---
    Hack_SDL_GPUDevice* hacked_device = (Hack_SDL_GPUDevice*)gpu_device;
    Hack_VulkanRenderer* renderer = (Hack_VulkanRenderer*)hacked_device->driverData;

    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader: Failed to access VulkanRenderer internals");
        return NULL;
    }

    VkInstance instance = renderer->instance;
    VkPhysicalDevice physicalDevice = renderer->physicalDevice;
    VkDevice device = renderer->logicalDevice;

    if (!instance || !physicalDevice || !device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader: Null Vulkan handles extracted");
        return NULL;
    }

    // Load function pointers
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!vkGetInstanceProcAddr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader: Failed to get vkGetInstanceProcAddr");
        return NULL;
    }

    PFN_vkGetDeviceQueue vkGetDeviceQueue = (PFN_vkGetDeviceQueue)vkGetInstanceProcAddr(instance, "vkGetDeviceQueue");
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties)vkGetInstanceProcAddr(instance,
                                                                            "vkGetPhysicalDeviceQueueFamilyProperties");

    if (!vkGetDeviceQueue || !vkGetPhysicalDeviceQueueFamilyProperties) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader: Failed to load necessary Vulkan functions");
        return NULL;
    }

    // Find Graphics Queue Family
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
    VkQueueFamilyProperties* queueProps =
        (VkQueueFamilyProperties*)SDL_malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueProps);

    uint32_t graphicsQueueIndex = (uint32_t)-1;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsQueueIndex = i;
            break;
        }
    }
    SDL_free(queueProps);

    if (graphicsQueueIndex == (uint32_t)-1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader: Could not find Graphics Queue Family");
        return NULL;
    }

    // Get Queue
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, graphicsQueueIndex, 0, &queue);

    if (!queue) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader: Failed to retrieve VkQueue");
        return NULL;
    }

    // --- Initialize Librashader ---
    LibrashaderManagerGPU* manager = (LibrashaderManagerGPU*)calloc(1, sizeof(LibrashaderManagerGPU));
    manager->vk_device = device;
    manager->vk_physical_device = physicalDevice;
    manager->vk_instance = instance;
    manager->vk_queue = queue;
    manager->vk_queue_family_index = graphicsQueueIndex;
    manager->vkGetDeviceQueue = vkGetDeviceQueue;
    manager->vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)vkGetInstanceProcAddr(instance, "vkCmdPipelineBarrier");
    manager->vkCmdBlitImage = (PFN_vkCmdBlitImage)vkGetInstanceProcAddr(instance, "vkCmdBlitImage");

    libra_error_t err;

    // Create Preset
    err = libra_preset_create_with_options(preset_path, NULL, NULL, &manager->preset);
    if (err != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader: Failed to create preset: %s", preset_path);
        libra_error_print(err);
        free(manager);
        return NULL;
    }

    // Create Filter Chain
    struct filter_chain_vk_opt_t opt;
    opt.version = LIBRASHADER_CURRENT_VERSION;
    opt.frames_in_flight = 2;
    opt.force_no_mipmaps = false;
    opt.disable_cache = false;
    opt.use_dynamic_rendering = false;

    struct libra_device_vk_t libra_device = { .physical_device = physicalDevice,
                                              .instance = instance,
                                              .device = device,
                                              .queue = queue,
                                              .entry = vkGetInstanceProcAddr };

    err = libra_vk_filter_chain_create(&manager->preset, libra_device, &opt, &manager->vk_filter_chain);
    if (err != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader: Failed to create Vulkan filter chain");
        libra_error_print(err);
        libra_preset_free(&manager->preset);
        free(manager);
        return NULL;
    }

    SDL_Log("Librashader (Vulkan) initialized successfully.");
    return manager;
}

static void LibrashaderManager_Render_Vulkan(LibrashaderManagerGPU* manager, void* command_buffer, void* input_texture,
                                             void* intermediate_texture, void* swapchain_texture, int input_w,
                                             int input_h, int viewport_w, int viewport_h, int swapchain_w,
                                             int swapchain_h, int display_x, int display_y) {
    // --- HACK: Extract Handles ---
    Hack_VulkanCommandBuffer* vk_cmd_buf_wrapper = (Hack_VulkanCommandBuffer*)command_buffer;
    VkCommandBuffer vk_cmd = vk_cmd_buf_wrapper->commandBuffer;

    Hack_VulkanTextureContainer* input_container = (Hack_VulkanTextureContainer*)input_texture;
    VkImage input_image = input_container->activeTexture->image;

    Hack_VulkanTextureContainer* intermediate_container = (Hack_VulkanTextureContainer*)intermediate_texture;
    VkImage intermediate_image = intermediate_container->activeTexture->image;

    Hack_VulkanTextureContainer* swapchain_container = (Hack_VulkanTextureContainer*)swapchain_texture;
    VkImage swapchain_image = swapchain_container->activeTexture->image;

    // =========================================================================
    // Stage 1: Render librashader to intermediate texture at {0,0}
    // This ensures gl_FragCoord starts at (0.5, 0.5) — matching the GL backend.
    // Curvature shaders compute around the correct center point.
    // =========================================================================

    struct libra_image_vk_t input_img = {
        .handle = input_image, .format = VK_FORMAT_R8G8B8A8_UNORM, .width = input_w, .height = input_h
    };

    // Output = viewport-sized intermediate (NOT the full swapchain)
    struct libra_image_vk_t output_img = {
        .handle = intermediate_image, .format = VK_FORMAT_B8G8R8A8_UNORM, .width = viewport_w, .height = viewport_h
    };

    // Map SDL swapchain format to VkFormat
    SDL_GPUTextureFormat sdl_fmt = SDL_GetGPUSwapchainTextureFormat(SDLApp_GetGPUDevice(), SDLApp_GetWindow());
    if (sdl_fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM)
        output_img.format = VK_FORMAT_B8G8R8A8_UNORM;
    else if (sdl_fmt == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)
        output_img.format = VK_FORMAT_R8G8B8A8_UNORM;
    else if (sdl_fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
        output_img.format = VK_FORMAT_B8G8R8A8_SRGB;
    else if (sdl_fmt == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB)
        output_img.format = VK_FORMAT_R8G8B8A8_SRGB;

    // Viewport at {0,0} — fills the entire intermediate texture
    struct libra_viewport_t viewport = {
        .x = 0.0f, .y = 0.0f, .width = (uint32_t)viewport_w, .height = (uint32_t)viewport_h
    };

    static bool logged_once = false;
    if (!logged_once) {
        SDL_Log("Librashader Render: input=%dx%d intermediate=%dx%d display_offset={%d,%d} swapchain=%dx%d",
                input_w,
                input_h,
                viewport_w,
                viewport_h,
                display_x,
                display_y,
                swapchain_w,
                swapchain_h);
        logged_once = true;
    }

    struct frame_vk_opt_t opt;
    opt.version = LIBRASHADER_CURRENT_VERSION;
    opt.clear_history = false;
    opt.frame_direction = 1;
    opt.rotation = 0;
    opt.total_subframes = 1;
    opt.current_subframe = 1;
    opt.aspect_ratio = 0.0f;
    opt.frames_per_second = 60.0f;
    opt.frametime_delta = 16;

    libra_error_t err = libra_vk_filter_chain_frame(
        &manager->vk_filter_chain, vk_cmd, manager->frame_count++, input_img, output_img, &viewport, NULL, &opt);

    if (err != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader: Frame render failed");
        libra_error_print(err);
        return;
    }

    // =========================================================================
    // Stage 2: Blit intermediate texture to swapchain at the letterbox position
    // Uses raw Vulkan commands to bypass SDL_GPU's internal layout tracking.
    // =========================================================================

    if (!manager->vkCmdPipelineBarrier || !manager->vkCmdBlitImage)
        return;

    // Barrier: intermediate image → TRANSFER_SRC_OPTIMAL
    {
        VkImageMemoryBarrier barrier;
        memset(&barrier, 0, sizeof(barrier));
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = intermediate_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        manager->vkCmdPipelineBarrier(vk_cmd,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      0,
                                      0,
                                      NULL,
                                      0,
                                      NULL,
                                      1,
                                      &barrier);
    }

    // Barrier: swapchain image → TRANSFER_DST_OPTIMAL
    // (After the clear render pass, SDL_GPU left it in COLOR_ATTACHMENT_OPTIMAL)
    {
        VkImageMemoryBarrier barrier;
        memset(&barrier, 0, sizeof(barrier));
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapchain_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        manager->vkCmdPipelineBarrier(vk_cmd,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      0,
                                      0,
                                      NULL,
                                      0,
                                      NULL,
                                      1,
                                      &barrier);
    }

    // Blit: intermediate[0,0 → viewport_w,viewport_h] → swapchain[display_x,display_y → ...]
    {
        VkImageBlit region;
        memset(&region, 0, sizeof(region));
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[0] = (VkOffset3D) { 0, 0, 0 };
        region.srcOffsets[1] = (VkOffset3D) { viewport_w, viewport_h, 1 };
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[0] = (VkOffset3D) { display_x, display_y, 0 };
        region.dstOffsets[1] = (VkOffset3D) { display_x + viewport_w, display_y + viewport_h, 1 };

        manager->vkCmdBlitImage(vk_cmd,
                                intermediate_image,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                swapchain_image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                1,
                                &region,
                                VK_FILTER_LINEAR);
    }

    // Barrier: swapchain image → COLOR_ATTACHMENT_OPTIMAL (for bezels to render on top)
    {
        VkImageMemoryBarrier barrier;
        memset(&barrier, 0, sizeof(barrier));
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapchain_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        manager->vkCmdPipelineBarrier(vk_cmd,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      0,
                                      0,
                                      NULL,
                                      0,
                                      NULL,
                                      1,
                                      &barrier);
    }
}

// =============================================================================
// Public API
// =============================================================================

LibrashaderManagerGPU* LibrashaderManager_Init_GPU(const char* preset_path) {
    SDL_GPUDevice* gpu_device = SDLApp_GetGPUDevice();
    if (!gpu_device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader: SDL_GPUDevice is NULL");
        return NULL;
    }

    const char* backend = SDL_GetGPUDeviceDriver(gpu_device);
    SDL_Log("Librashader: SDL_GPU backend is '%s'", backend);

    if (SDL_strcmp(backend, "vulkan") == 0) {
        return LibrashaderManager_Init_Vulkan(preset_path, gpu_device);
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Librashader: Unsupported GPU backend '%s'. Only Vulkan is supported.",
                     backend);
        return NULL;
    }
}

void LibrashaderManager_Render_GPU(LibrashaderManagerGPU* manager, void* command_buffer, void* input_texture,
                                   void* intermediate_texture, void* swapchain_texture, int input_w, int input_h,
                                   int viewport_w, int viewport_h, int swapchain_w, int swapchain_h, int display_x,
                                   int display_y) {
    if (!manager || !manager->vk_filter_chain)
        return;

    LibrashaderManager_Render_Vulkan(manager,
                                     command_buffer,
                                     input_texture,
                                     intermediate_texture,
                                     swapchain_texture,
                                     input_w,
                                     input_h,
                                     viewport_w,
                                     viewport_h,
                                     swapchain_w,
                                     swapchain_h,
                                     display_x,
                                     display_y);
}

void LibrashaderManager_Free_GPU(LibrashaderManagerGPU* manager) {
    if (!manager)
        return;

    if (manager->vk_filter_chain) {
        libra_vk_filter_chain_free(&manager->vk_filter_chain);
    }

    // Preset is invalidated by filter chain creation, nothing to free.
    free(manager);
}
