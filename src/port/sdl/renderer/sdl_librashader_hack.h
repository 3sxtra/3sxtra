#ifndef SDL_LIBRASHADER_HACK_H
#define SDL_LIBRASHADER_HACK_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

// --- SDL_sysgpu.h HACK ---

// We need to define these to match SDL's internal memory layout.
// Be extremely careful with alignment and member ordering.

#define HACK_MAX_TEXTURE_SAMPLERS_PER_STAGE 16
#define HACK_MAX_STORAGE_TEXTURES_PER_STAGE 8
#define HACK_MAX_STORAGE_BUFFERS_PER_STAGE 8
#define HACK_MAX_UNIFORM_BUFFERS_PER_STAGE 4
#define HACK_MAX_COMPUTE_WRITE_TEXTURES 8
#define HACK_MAX_COMPUTE_WRITE_BUFFERS 8
#define HACK_MAX_VERTEX_BUFFERS 16
#define HACK_MAX_COLOR_TARGET_BINDINGS 8
#define HACK_MAX_PRESENT_COUNT 16
#define HACK_MAX_FRAMES_IN_FLIGHT 3

// Pass Structures (from SDL_sysgpu.h)
typedef struct Hack_Pass {
    void* command_buffer; // SDL_GPUCommandBuffer*
    bool in_progress;
} Hack_Pass;

typedef struct Hack_ComputePass {
    void* command_buffer;
    bool in_progress;
    void* compute_pipeline;
    bool sampler_bound[HACK_MAX_TEXTURE_SAMPLERS_PER_STAGE];
    bool read_only_storage_texture_bound[HACK_MAX_STORAGE_TEXTURES_PER_STAGE];
    bool read_only_storage_buffer_bound[HACK_MAX_STORAGE_BUFFERS_PER_STAGE];
    bool read_write_storage_texture_bound[HACK_MAX_COMPUTE_WRITE_TEXTURES];
    bool read_write_storage_buffer_bound[HACK_MAX_COMPUTE_WRITE_BUFFERS];
} Hack_ComputePass;

typedef struct Hack_RenderPass {
    void* command_buffer;
    bool in_progress;
    void* color_targets[HACK_MAX_COLOR_TARGET_BINDINGS]; // SDL_GPUTexture*
    Uint32 num_color_targets;
    void* depth_stencil_target; // SDL_GPUTexture*
    void* graphics_pipeline;
    bool vertex_sampler_bound[HACK_MAX_TEXTURE_SAMPLERS_PER_STAGE];
    bool vertex_storage_texture_bound[HACK_MAX_STORAGE_TEXTURES_PER_STAGE];
    bool vertex_storage_buffer_bound[HACK_MAX_STORAGE_BUFFERS_PER_STAGE];
    bool fragment_sampler_bound[HACK_MAX_TEXTURE_SAMPLERS_PER_STAGE];
    bool fragment_storage_texture_bound[HACK_MAX_STORAGE_TEXTURES_PER_STAGE];
    bool fragment_storage_buffer_bound[HACK_MAX_STORAGE_BUFFERS_PER_STAGE];
} Hack_RenderPass;

typedef struct Hack_CommandBufferCommonHeader {
    SDL_GPUDevice* device;
    Hack_RenderPass render_pass;
    Hack_ComputePass compute_pass;
    Hack_Pass copy_pass;
    bool swapchain_texture_acquired;
    bool submitted;
    bool ignore_render_pass_texture_validation;
} Hack_CommandBufferCommonHeader;

// SDL_GPUDevice (from SDL_sysgpu.h)
// We only need to access the members at the end, but we need the correct offset.
// This structure is HUGE because of all the function pointers.
// To avoid fragility, we will NOT define the full struct here.
// Instead, we rely on the fact that `driverData` is stored in the struct.
//
// However, `SDL_GPUDevice` definition in `SDL_sysgpu.h` has `driverData` AT THE END.
//
// struct SDL_GPUDevice {
//     ... lots of function pointers ...
//     SDL_GPURenderer *driverData;
//     const char *backend;
//     SDL_GPUShaderFormat shader_formats;
//     bool debug_mode;
//     ...
// };
//
// This is extremely risky to offset-hack.
//
// ALTERNATIVE STRATEGY for `driverData`:
// SDL allocates `SDL_GPUDevice` using `SDL_calloc`.
// The function pointers are filled in.
// We can scan the struct for a pointer that looks like a heap pointer (to `VulkanRenderer`).
// OR, we can just bite the bullet and copy the function pointers. There are ~78 of them.
//
// Let's copy the function pointers. It is safer than guessing.

typedef struct Hack_SDL_GPUDevice {
    // Device
    void (*DestroyDevice)(SDL_GPUDevice* device);
    void* (*DestroyXRSwapchain)(void* device, void* swapchain, void** swapchainImages);
    SDL_PropertiesID (*GetDeviceProperties)(SDL_GPUDevice* device);

    // State Creation
    void* (*CreateComputePipeline)(void* driverData, const void* createinfo);
    void* (*CreateGraphicsPipeline)(void* driverData, const void* createinfo);
    void* (*CreateSampler)(void* driverData, const void* createinfo);
    void* (*CreateShader)(void* driverData, const void* createinfo);
    void* (*CreateTexture)(void* driverData, const void* createinfo);
    void* (*CreateBuffer)(void* driverData, Uint32 usageFlags, Uint32 size, const char* debugName);
    void* (*CreateTransferBuffer)(void* driverData, int usage, Uint32 size,
                                  const char* debugName); // SDL_GPUTransferBufferUsage is enum
    void* (*CreateXRSession)(void* driverData, const void* createinfo, void* session);
    void* (*GetXRSwapchainFormats)(void* driverData, void* session, int* num_formats);
    void* (*CreateXRSwapchain)(void* driverData, void* session, const void* createinfo, int format, void* swapchain,
                               void*** textures);

    // Debug Naming
    void (*SetBufferName)(void* driverData, void* buffer, const char* text);
    void (*SetTextureName)(void* driverData, void* texture, const char* text);
    void (*InsertDebugLabel)(void* commandBuffer, const char* text);
    void (*PushDebugGroup)(void* commandBuffer, const char* name);
    void (*PopDebugGroup)(void* commandBuffer);

    // Disposal
    void (*ReleaseTexture)(void* driverData, void* texture);
    void (*ReleaseSampler)(void* driverData, void* sampler);
    void (*ReleaseBuffer)(void* driverData, void* buffer);
    void (*ReleaseTransferBuffer)(void* driverData, void* transferBuffer);
    void (*ReleaseShader)(void* driverData, void* shader);
    void (*ReleaseComputePipeline)(void* driverData, void* computePipeline);
    void (*ReleaseGraphicsPipeline)(void* driverData, void* graphicsPipeline);

    // Render Pass
    void (*BeginRenderPass)(void* commandBuffer, const void* colorTargetInfos, Uint32 numColorTargets,
                            const void* depthStencilTargetInfo);
    void (*BindGraphicsPipeline)(void* commandBuffer, void* graphicsPipeline);
    void (*SetViewport)(void* commandBuffer, const void* viewport);
    void (*SetScissor)(void* commandBuffer, const void* scissor);
    void (*SetBlendConstants)(void* commandBuffer, SDL_FColor blendConstants);
    void (*SetStencilReference)(void* commandBuffer, Uint8 reference);
    void (*BindVertexBuffers)(void* commandBuffer, Uint32 firstSlot, const void* bindings, Uint32 numBindings);
    void (*BindIndexBuffer)(void* commandBuffer, const void* binding, int indexElementSize);
    void (*BindVertexSamplers)(void* commandBuffer, Uint32 firstSlot, const void* textureSamplerBindings,
                               Uint32 numBindings);
    void (*BindVertexStorageTextures)(void* commandBuffer, Uint32 firstSlot, void* const* storageTextures,
                                      Uint32 numBindings);
    void (*BindVertexStorageBuffers)(void* commandBuffer, Uint32 firstSlot, void* const* storageBuffers,
                                     Uint32 numBindings);
    void (*BindFragmentSamplers)(void* commandBuffer, Uint32 firstSlot, const void* textureSamplerBindings,
                                 Uint32 numBindings);
    void (*BindFragmentStorageTextures)(void* commandBuffer, Uint32 firstSlot, void* const* storageTextures,
                                        Uint32 numBindings);
    void (*BindFragmentStorageBuffers)(void* commandBuffer, Uint32 firstSlot, void* const* storageBuffers,
                                       Uint32 numBindings);
    void (*PushVertexUniformData)(void* commandBuffer, Uint32 slotIndex, const void* data, Uint32 length);
    void (*PushFragmentUniformData)(void* commandBuffer, Uint32 slotIndex, const void* data, Uint32 length);
    void (*DrawIndexedPrimitives)(void* commandBuffer, Uint32 numIndices, Uint32 numInstances, Uint32 firstIndex,
                                  Sint32 vertexOffset, Uint32 firstInstance);
    void (*DrawPrimitives)(void* commandBuffer, Uint32 numVertices, Uint32 numInstances, Uint32 firstVertex,
                           Uint32 firstInstance);
    void (*DrawPrimitivesIndirect)(void* commandBuffer, void* buffer, Uint32 offset, Uint32 drawCount);
    void (*DrawIndexedPrimitivesIndirect)(void* commandBuffer, void* buffer, Uint32 offset, Uint32 drawCount);
    void (*EndRenderPass)(void* commandBuffer);

    // Compute Pass
    void (*BeginComputePass)(void* commandBuffer, const void* storageTextureBindings, Uint32 numStorageTextureBindings,
                             const void* storageBufferBindings, Uint32 numStorageBufferBindings);
    void (*BindComputePipeline)(void* commandBuffer, void* computePipeline);
    void (*BindComputeSamplers)(void* commandBuffer, Uint32 firstSlot, const void* textureSamplerBindings,
                                Uint32 numBindings);
    void (*BindComputeStorageTextures)(void* commandBuffer, Uint32 firstSlot, void* const* storageTextures,
                                       Uint32 numBindings);
    void (*BindComputeStorageBuffers)(void* commandBuffer, Uint32 firstSlot, void* const* storageBuffers,
                                      Uint32 numBindings);
    void (*PushComputeUniformData)(void* commandBuffer, Uint32 slotIndex, const void* data, Uint32 length);
    void (*DispatchCompute)(void* commandBuffer, Uint32 groupcountX, Uint32 groupcountY, Uint32 groupcountZ);
    void (*DispatchComputeIndirect)(void* commandBuffer, void* buffer, Uint32 offset);
    void (*EndComputePass)(void* commandBuffer);

    // TransferBuffer Data
    void* (*MapTransferBuffer)(void* device, void* transferBuffer, bool cycle);
    void (*UnmapTransferBuffer)(void* device, void* transferBuffer);

    // Copy Pass
    void (*BeginCopyPass)(void* commandBuffer);
    void (*UploadToTexture)(void* commandBuffer, const void* source, const void* destination, bool cycle);
    void (*UploadToBuffer)(void* commandBuffer, const void* source, const void* destination, bool cycle);
    void (*CopyTextureToTexture)(void* commandBuffer, const void* source, const void* destination, Uint32 w, Uint32 h,
                                 Uint32 d, bool cycle);
    void (*CopyBufferToBuffer)(void* commandBuffer, const void* source, const void* destination, Uint32 size,
                               bool cycle);
    void (*GenerateMipmaps)(void* commandBuffer, void* texture);
    void (*DownloadFromTexture)(void* commandBuffer, const void* source, const void* destination);
    void (*DownloadFromBuffer)(void* commandBuffer, const void* source, const void* destination);
    void (*EndCopyPass)(void* commandBuffer);
    void (*Blit)(void* commandBuffer, const void* info);

    // Submission/Presentation
    bool (*SupportsSwapchainComposition)(void* driverData, SDL_Window* window, int swapchainComposition);
    bool (*SupportsPresentMode)(void* driverData, SDL_Window* window, int presentMode);
    bool (*ClaimWindow)(void* driverData, SDL_Window* window);
    void (*ReleaseWindow)(void* driverData, SDL_Window* window);
    bool (*SetSwapchainParameters)(void* driverData, SDL_Window* window, int swapchainComposition, int presentMode);
    bool (*SetAllowedFramesInFlight)(void* driverData, Uint32 allowedFramesInFlight);
    int (*GetSwapchainTextureFormat)(void* driverData, SDL_Window* window); // SDL_GPUTextureFormat
    void* (*AcquireCommandBuffer)(void* driverData);
    bool (*AcquireSwapchainTexture)(void* commandBuffer, SDL_Window* window, void** swapchainTexture,
                                    Uint32* swapchainTextureWidth, Uint32* swapchainTextureHeight);
    bool (*WaitForSwapchain)(void* driverData, SDL_Window* window);
    bool (*WaitAndAcquireSwapchainTexture)(void* commandBuffer, SDL_Window* window, void** swapchainTexture,
                                           Uint32* swapchainTextureWidth, Uint32* swapchainTextureHeight);
    bool (*Submit)(void* commandBuffer);
    void* (*SubmitAndAcquireFence)(void* commandBuffer);
    bool (*Cancel)(void* commandBuffer);
    bool (*Wait)(void* driverData);
    bool (*WaitForFences)(void* driverData, bool waitAll, void* const* fences, Uint32 numFences);
    bool (*QueryFence)(void* driverData, void* fence);
    void (*ReleaseFence)(void* driverData, void* fence);

    // Feature Queries
    bool (*SupportsTextureFormat)(void* driverData, int format, int type, Uint32 usage);
    bool (*SupportsSampleCount)(void* driverData, int format, int desiredSampleCount);

    // Opaque pointer for the Driver
    void* driverData; // This is what we want!

    // ... fields after this don't matter as long as we don't access them
} Hack_SDL_GPUDevice;

// --- SDL_gpu_vulkan.c HACK ---

typedef struct Hack_VulkanRenderer {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties2KHR physicalDeviceProperties;
    VkPhysicalDeviceDriverPropertiesKHR physicalDeviceDriverProperties;
    VkDevice logicalDevice;
    Uint8 integratedMemoryNotification;
    Uint8 outOfDeviceLocalMemoryWarning;
    Uint8 outofBARMemoryWarning;
    Uint8 fillModeOnlyWarning;

    // OpenXR
    Uint32 minimumVkVersion;
    // #ifdef HAVE_GPU_OPENXR ... we need to be careful here.
    // SDL code wraps this in #ifdef HAVE_GPU_OPENXR.
    // If our build has HAVE_GPU_OPENXR defined differently than SDL's build, offset will be WRONG.
    // SDL3 usually enables OpenXR if headers are found?
    // We should check SDL_config.h or cmake cache?
    // Assuming NO OpenXR for now or checking the unifiedQueue alignment.
    // Actually, checking `SDL_gpu_vulkan.c`:
    // #ifdef HAVE_GPU_OPENXR
    //    XrInstance xrInstance;
    //    XrSystemId xrSystemId;
    //    XrInstancePfns *xr;
    // #endif
    //
    // If we assume standard desktop build without OpenXR for now...
    // Wait, the CMakeLists.txt doesn't explicitly enable OpenXR.
    // BUT `SDL_vulkan.h` was present.
    // `queueFamilyIndex` is after OpenXR fields.
    // `unifiedQueue` is after `queueFamilyIndex`.

    // We can search for `logicalDevice` (which we have) and then scan for `unifiedQueue` (which is a pointer/handle).
    // Or we can just use `vkGetDeviceQueue` since we have `logicalDevice` and `physicalDevice`.
    // We can query queue families from `physicalDevice` to find the graphics queue index ourselves!
    // This is safer than relying on `VulkanRenderer` layout beyond `logicalDevice`.

} Hack_VulkanRenderer;

typedef struct Hack_VulkanTexture {
    void* container; // VulkanTextureContainer*
    Uint32 containerIndex;

    void* usedRegion; // VulkanMemoryUsedRegion*

    VkImage image;
    VkImageView fullView;
    // ...
} Hack_VulkanTexture;

typedef struct Hack_VulkanCommandBuffer {
    Hack_CommandBufferCommonHeader common;
    Hack_VulkanRenderer* renderer;

    VkCommandBuffer commandBuffer;
    void* commandPool;

    // ...
} Hack_VulkanCommandBuffer;

// SDL_GPUTexture* is actually a VulkanTextureContainer*, not a VulkanTexture*.
// We need to go through the container to reach the active VulkanTexture and its VkImage.
//
// Layout (from SDL_gpu_vulkan.c):
//   struct VulkanTextureContainer {
//       TextureCommonHeader header;  // = { SDL_GPUTextureCreateInfo info; }
//       VulkanTexture *activeTexture;
//       ...
//   };
typedef struct Hack_VulkanTextureContainer {
    SDL_GPUTextureCreateInfo header; // TextureCommonHeader
    Hack_VulkanTexture* activeTexture;
} Hack_VulkanTextureContainer;

#endif // SDL_LIBRASHADER_HACK_H
