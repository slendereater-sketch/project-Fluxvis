#pragma once

#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <vector>
#include "audio_capture.h"

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    bool Init(ANativeWindow* window);
    void Terminate();
    
    void Render(const AudioFeatures& features);
    void OnResize(int width, int height);

private:
    bool CreateInstance();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain();
    bool CreateRenderPass();
    bool CreateGraphicsPipeline();
    bool CreateFramebuffers();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();

    VkInstance mInstance = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;
    
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;
    
    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCommandBuffers;
    
    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    std::vector<VkFence> mInFlightFences;
    
    uint32_t mCurrentFrame = 0;
    const int MAX_FRAMES_IN_FLIGHT = 2;
    
    ANativeWindow* mWindow = nullptr;
    int mWidth, mHeight;
};
