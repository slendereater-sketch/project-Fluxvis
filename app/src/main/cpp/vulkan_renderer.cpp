#include "vulkan_renderer.h"
#include <android/log.h>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>

#define TAG "VulkanRenderer"

VulkanRenderer::VulkanRenderer() {}

VulkanRenderer::~VulkanRenderer() {
    Terminate();
}

bool VulkanRenderer::Init(ANativeWindow* window) {
    mWindow = window;
    try {
        if (!CreateInstance()) return false;
        if (!SelectPhysicalDevice()) return false;
        if (!CreateLogicalDevice()) return false;
        if (!CreateSwapchain()) return false;
        if (!CreateRenderPass()) return false;
        if (!CreateGraphicsPipeline()) return false;
        if (!CreateFramebuffers()) return false;
        if (!CreateCommandBuffers()) return false;
        if (!CreateSyncObjects()) return false;
    } catch (const std::exception& e) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Vulkan Init Failed: %s", e.what());
        return false;
    }
    return true;
}

void VulkanRenderer::Terminate() {
    if (mDevice != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(mDevice);
        
        for (auto fence : mInFlightFences) vkDestroyFence(mDevice, fence, nullptr);
        for (auto semaphore : mRenderFinishedSemaphores) vkDestroySemaphore(mDevice, semaphore, nullptr);
        for (auto semaphore : mImageAvailableSemaphores) vkDestroySemaphore(mDevice, semaphore, nullptr);
        
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        for (auto framebuffer : mSwapchainFramebuffers) vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
        vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        for (auto imageView : mSwapchainImageViews) vkDestroyImageView(mDevice, imageView, nullptr);
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        vkDestroyDevice(mDevice, nullptr);
    }
    
    if (mInstance != VK_NULL_HANDLE) {
        if (mSurface != VK_NULL_HANDLE) vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        vkDestroyInstance(mInstance, nullptr);
    }
}

bool VulkanRenderer::CreateInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Visualizer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensions;

    if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS) {
        return false;
    }
    return true;
}

bool VulkanRenderer::SelectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    if (deviceCount == 0) return false;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
    
    mPhysicalDevice = devices[0]; // Just pick the first one for now
    return true;
}

bool VulkanRenderer::CreateLogicalDevice() {
    // Basic queue setup
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = 0; // Assume index 0 is graphics
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(mDevice, 0, 0, &mGraphicsQueue);
    return true;
}

// Skeletons for the rest
bool VulkanRenderer::CreateSwapchain() { return true; }
bool VulkanRenderer::CreateRenderPass() { return true; }
bool VulkanRenderer::CreateGraphicsPipeline() { return true; }
bool VulkanRenderer::CreateFramebuffers() { return true; }
bool VulkanRenderer::CreateCommandBuffers() { return true; }
bool VulkanRenderer::CreateSyncObjects() { return true; }

void VulkanRenderer::Render(const AudioFeatures& features) {
    (void)features;
    // Vulkan render loop would go here
}

void VulkanRenderer::OnResize(int width, int height) {
    mWidth = width;
    mHeight = height;
}
