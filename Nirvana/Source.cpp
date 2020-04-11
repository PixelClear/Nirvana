#include <stdio.h>
#include <assert.h>
#include <algorithm>
#include <limits>
#include <optional>
#include <vector>
#include <set>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>

#define VK_CHECK(call) \
do {\
 VkResult result = call; \
assert(result == VK_SUCCESS); \
} while(0);\

const char *debugLayers[] =
{
  "VK_LAYER_LUNARG_standard_validation"
};

const char *deviceExtension[] =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

constexpr uint32_t width = 1024;
constexpr uint32_t height = 768;

typedef struct QueueIndexFamily
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
}QueueIndexFamily;

struct SwapChainDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

VkInstance createInstance()
{
    //ToDo : Should check if 1.1 exists vkEnumerateInstanceVersion
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

#ifdef _DEBUG
    createInfo.ppEnabledLayerNames = debugLayers;
    createInfo.enabledLayerCount = sizeof(debugLayers) / sizeof(debugLayers[0]);
#endif 

    const char* extensionNames[] =
    {
       VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
       VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
    };

    createInfo.ppEnabledExtensionNames = extensionNames;
    createInfo.enabledExtensionCount = sizeof(extensionNames) / sizeof(extensionNames[0]);

    VkInstance instance = {};
    VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

    return instance;
}

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    //need VK_USE_PLATFORM_WIN32_KHR to include vulkan_win32 that has structure VkWin32Surface VkWin32SurfaceCreateInfoKHR
    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(0);
    createInfo.hwnd = glfwGetWin32Window(window); //define GLFW_EXPOSE_NATIVE_WIN32

    VkSurfaceKHR surface = 0;
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &createInfo, 0, &surface)); //Need VK_KHR_WIN32_SURFACE_EXTENSION_NAME

    return surface;
#else
#error Unsupported platform
#endif
}

QueueIndexFamily getQueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueIndexFamily indices;

    uint32_t queueFamilyPropCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyPropCount, 0);

    std::vector<VkQueueFamilyProperties> qProps(queueFamilyPropCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyPropCount, qProps.data());

    uint32_t i = 0;
    for (const auto& q : qProps)
    {
        if (q.queueCount > 0 && q.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (q.queueCount > 0 && presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

bool requiredDeviceExtensionSupported(VkPhysicalDevice device)
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, 0, &extensionCount, 0);

    std::vector<VkExtensionProperties> props(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, 0, &extensionCount, props.data());

    for (const auto& prop :  props)
    {
        if (strcmp(prop.extensionName, deviceExtension[0]) == 0)
            return true;
    }

    return false;
}

SwapChainDetails getSurfaceCompatibility(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainDetails details;

    //Below functions are part of VK_KHR_Surface extension
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities));

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, 0));

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data()));
    }

    uint32_t presentModeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, 0));

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data()));
    }

    return details;
}

VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, QueueIndexFamily& outIndices, SwapChainDetails& outDetails)
{
    uint32_t deviceCount = 0;

    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, 0));

    if (deviceCount != 0)
    {
        std::vector<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount,devices.data()));

        for (const auto device : devices)
        {
            QueueIndexFamily indices = getQueueFamilyIndices(device, surface);
            bool reqExtensionSupported = requiredDeviceExtensionSupported(device);
            SwapChainDetails details = getSurfaceCompatibility(device, surface);
            bool surfaceCompatible = !details.formats.empty() && !details.presentModes.empty();

            if (indices.isComplete() && reqExtensionSupported && surfaceCompatible)
            {
                outIndices = indices;
                outDetails = details;
                return device;
            }
        }
    }

    return VK_NULL_HANDLE;
}

VkDevice createLogicalDevice(VkPhysicalDevice device, VkSurfaceKHR surface,  QueueIndexFamily indices)
{
    //device can create multiple qs instance here it will create two qs one for present and other for graphics
    std::set<uint32_t> uniqueIndices = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    std::vector<VkDeviceQueueCreateInfo> qCreateInfo = {};

    float qPriority = 1.0f;

    for (const auto uniqueIndex : uniqueIndices)
    {
        VkDeviceQueueCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        createInfo.pNext = 0;
        createInfo.flags = 0;
        createInfo.pQueuePriorities = &qPriority;
        createInfo.queueFamilyIndex = uniqueIndex;
        createInfo.queueCount = 1;

        qCreateInfo.push_back(createInfo);
    }

    VkPhysicalDeviceFeatures    pDeviceFeatures = {};
    VkDeviceCreateInfo createInfo = {};

    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = 0;
    createInfo.flags = 0;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(qCreateInfo.size());
    createInfo.pQueueCreateInfos = qCreateInfo.data();
#ifdef _DEBUG
    createInfo.ppEnabledLayerNames = debugLayers;
    createInfo.enabledLayerCount = sizeof(debugLayers) / sizeof(debugLayers[0]);
#endif
    createInfo.ppEnabledExtensionNames = deviceExtension;
    createInfo.enabledExtensionCount = sizeof(deviceExtension) / sizeof(deviceExtension[0]);
    createInfo.pEnabledFeatures = &pDeviceFeatures;

    VkDevice logicalDevice;
    VK_CHECK(vkCreateDevice(device, &createInfo, 0, &logicalDevice));

    return logicalDevice;
}

VkSurfaceFormatKHR chooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    //If user has not specified anything
    if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    }

    //If not provided with any option
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    //Else just return the first format
    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
{
    VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            bestMode = availablePresentMode;
        }
        else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            bestMode = availablePresentMode;
        }
    }

    return bestMode;
}

//ToDo: Cleanup
VkSwapchainKHR createSwapchain(VkDevice device, VkPhysicalDevice pDevice, VkSurfaceKHR surface, QueueIndexFamily indices, SwapChainDetails details)
{
    VkSurfaceFormatKHR imageFormat = chooseSwapChainSurfaceFormat(details.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(details.presentModes);
    VkExtent2D extents = { width, height };

    uint32_t imageCount = details.capabilities.minImageCount + 1;

    if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
    {
        imageCount = details.capabilities.maxImageCount;
    }

    VkSwapchainKHR swapChain;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.pNext = 0;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = imageFormat.format;
    createInfo.imageColorSpace = imageFormat.colorSpace;
    createInfo.imageExtent = extents;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    if (indices.graphicsFamily.value() != indices.presentFamily.value())
    {
        uint32_t familyIndices[] = { indices.graphicsFamily.value() ,indices.presentFamily.value() };
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; 
        createInfo.queueFamilyIndexCount = sizeof(familyIndices) / sizeof(familyIndices[0]);
        createInfo.pQueueFamilyIndices = familyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; 
        createInfo.queueFamilyIndexCount = 1;
        createInfo.pQueueFamilyIndices = &indices.graphicsFamily.value(); //Same q family so any index can be used
    }

    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode; 
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, 0, &swapChain));

    return swapChain;
}

VkSemaphore createSemaphore(VkDevice device)
{
    VkSemaphore semaphore = 0;
    VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    VK_CHECK(vkCreateSemaphore(device, &createInfo, 0, &semaphore));

    return semaphore;
}

VkCommandPool createCommandPool(VkDevice device, VkPhysicalDevice pDevice, VkSurfaceKHR surface, QueueIndexFamily indices)
{
    VkCommandPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.queueFamilyIndex = indices.graphicsFamily.value();

    VkCommandPool commandPool;
    VK_CHECK(vkCreateCommandPool(device, &createInfo, 0, &commandPool));

    return commandPool;
}

VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool pool)
{
    VkCommandBuffer cmdBuffer;
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandBufferCount = 1;
    allocateInfo.commandPool = pool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, &cmdBuffer));

    return cmdBuffer;
}

int main()
{
    int rc = glfwInit();
    assert(rc);

    VkInstance instance = createInstance();
    assert(instance);

    GLFWwindow* window = glfwCreateWindow(width, height, "Nirvana", 0, 0);
    assert(window);

    //Create surface before logical device as it will be used to select q family that can present to this surface
    VkSurfaceKHR surface = createSurface(instance, window); 
    assert(surface);

    //Pick physical device have graphics q + presentation q + swapchain support(has formats and so on) + present to this surface
    //indices.isComplete() && extensionSupported && surfaceCompatible;
    QueueIndexFamily indices;
    SwapChainDetails details;
    VkPhysicalDevice physicalDevice = pickPhysicalDevice(instance, surface, indices, details);
    assert(physicalDevice);

    VkDevice device = createLogicalDevice(physicalDevice, surface, indices);
    assert(device);
  
    VkSwapchainKHR swapChain = createSwapchain(device, physicalDevice, surface, indices, details);
    assert(swapChain);

    uint32_t imageCount = 0;
    std::vector<VkImage> images;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, 0));
    assert(imageCount != 0);
    images.resize(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data()));
    assert(images.size() != 0);

    VkQueue queue;
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &queue); //Hack needs to get separate present and graphics q

    VkSemaphore imageAquired = createSemaphore(device);
    assert(imageAquired);
    VkSemaphore cmdSubmited = createSemaphore(device);
    assert(cmdSubmited);

    VkCommandPool pool = createCommandPool(device, physicalDevice, surface, indices);
    assert(pool);
    
    VkCommandBuffer cmdBuffer = createCommandBuffer(device, pool);
    assert(cmdBuffer);

    VkClearColorValue color = { 1.0f, 0.0f, 1.0f, 1.0f };

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        uint32_t imageIndex = 0;
        VK_CHECK(vkAcquireNextImageKHR(device, swapChain, ~0ull, imageAquired, 0, &imageIndex));

        VK_CHECK(vkResetCommandPool(device, pool, 0)); //Make command buffer reusable 

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.layerCount = 1;
        range.levelCount = 1;

        vkCmdClearColorImage(cmdBuffer, images[imageIndex], VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);

        VK_CHECK(vkEndCommandBuffer(cmdBuffer));

        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAquired;
        submitInfo.pWaitDstStageMask = &stageMask;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &cmdSubmited;

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, 0));

        VkPresentInfoKHR presentInfo = { };
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &cmdSubmited;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.pImageIndices = &imageIndex;
        
        VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

        VK_CHECK(vkDeviceWaitIdle(device));
    }

    glfwDestroyWindow(window);
}