#include <stdio.h>
#include <assert.h>
#include <fstream>
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

VkImageView createImageView(VkDevice device, VkImage swapchainImage, SwapChainDetails details)
{
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = swapchainImage;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = chooseSwapChainSurfaceFormat(details.formats).format;
    createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, 
                              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.layerCount = 1;
    createInfo.subresourceRange.levelCount = 1;

    VkImageView view;

    VK_CHECK(vkCreateImageView(device, &createInfo, 0, &view));

    return view;
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

VkRenderPass createRenderPass(VkDevice device, SwapChainDetails details)
{
    VkRenderPass renderPass;

    VkAttachmentDescription attachment[1]; //Hack : Currently we only have color attachment
    attachment[0].flags = 0;
    attachment[0].format = chooseSwapChainSurfaceFormat(details.formats).format;
    attachment[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //Initialy I gave it as color optimial that raised validation layer error
    attachment[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachment = { 0 , VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDesc[1] = {}; //Currently we have one but will be many
    subpassDesc[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc[0].colorAttachmentCount = 1;
    subpassDesc[0].pColorAttachments = &colorAttachment;
    
    VkRenderPassCreateInfo createInfo = {};

    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = sizeof(attachment) / sizeof(attachment[0]);
    createInfo.pAttachments = attachment;
    createInfo.subpassCount = sizeof(subpassDesc) / sizeof(subpassDesc[0]);
    createInfo.pSubpasses = subpassDesc;
    //createInfo.dependencyCount; //Not filling currently
    //createInfo.pDependencies; //Not filling currently
    
    VK_CHECK(vkCreateRenderPass(device, &createInfo, 0, &renderPass));

    return renderPass;
}

VkFramebuffer createFramebuffer(VkDevice device, VkRenderPass renderPass, VkImageView imageView)
{
    VkFramebuffer framebuffer;

    VkFramebufferCreateInfo createInfo = {  };
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass  = renderPass;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &imageView;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(device, &createInfo, 0, &framebuffer));

    return framebuffer;

}

std::vector<char> readFile(const std::string& fileName) 
{
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open " + fileName + "\n");
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, std::vector<char>& buffer)
{
    VkShaderModule module;
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VK_CHECK(vkCreateShaderModule(device, &createInfo, 0, &module));

    return module;
}

VkPipelineLayout createPipilineLayout(VkDevice device)
{
    VkPipelineLayout pipelineLayout;
    /*We need mechanism to pass uniforms to shaders but we dont want to modify graphics pipeline
     So we create pipeline layout object*/
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    return pipelineLayout;
}

VkPipeline createGraphicsPipeline(VkDevice device, VkShaderModule vs, VkShaderModule fs, VkRenderPass renderPass, VkPipelineLayout pipelineLayout)
{
    VkPipeline graphicsPipeline;

    VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo = {};
    vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageCreateInfo.module = vs;
    vertShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo = {};
    fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageCreateInfo.module = fs;
    fragShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageCreateInfo, fragShaderStageCreateInfo };

    /*Create fixed function pipeline*/
    //Vertex State
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
    vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
    vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
    vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

    //Input assembly 
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    //Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.lineWidth = 1.0f;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    //Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    //Depth and Stenciling ---> Currently we will pass nullptr

    //Color Blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    //Dynamic state
    /* Some part of hard coded state that we created above can be changed
       following structure used for that*/
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;;
    pipelineInfo.pVertexInputState = &vertexInputCreateInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState; // Optional
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

    return graphicsPipeline;
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

    uint32_t swapchainImageCount = 0;
    std::vector<VkImage> images;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &swapchainImageCount, 0));
    assert(swapchainImageCount != 0);
    images.resize(swapchainImageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &swapchainImageCount, images.data()));
    assert(images.size() != 0);

    std::vector<VkImageView> imageViews(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; i++)
    {
        imageViews[i] = createImageView(device, images[i] , details);
        assert(imageViews[i]);
    }

    VkRenderPass renderPass = createRenderPass(device, details);
    assert(renderPass);

    std::vector<char> vsCode = readFile("Shaders/vert.spv");
    std::vector<char> fsCode = readFile("Shaders/frag.spv");
    assert(vsCode.size() != 0);
    assert(fsCode.size() != 0);

    VkShaderModule vs = createShaderModule(device, vsCode);
    assert(vs);
    VkShaderModule fs = createShaderModule(device, fsCode);
    assert(fs);

    VkPipelineLayout pipelineLayout = createPipilineLayout(device);
    assert(pipelineLayout);

    VkPipeline graphicsPipeline = createGraphicsPipeline(device, vs, fs, renderPass, pipelineLayout);
    assert(graphicsPipeline);

    std::vector<VkFramebuffer> frameBuffers(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; i++)
    {
        frameBuffers[i] = createFramebuffer(device, renderPass, imageViews[i]);
        assert(frameBuffers[i]);
    }

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

    VkClearColorValue color = { 0.0f, 0.0f, 0.0f, 1.0f };

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

        VkClearValue clearColor[1] = { color };

        VkRenderPassBeginInfo rBeginInfo = {};
        rBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rBeginInfo.renderPass = renderPass;
        rBeginInfo.framebuffer = frameBuffers[imageIndex];
        rBeginInfo.renderArea.extent.width = width;
        rBeginInfo.renderArea.extent.height = height;
        rBeginInfo.clearValueCount = sizeof(clearColor)/ sizeof(clearColor[0]);
        rBeginInfo.pClearValues = clearColor;

        vkCmdBeginRenderPass(cmdBuffer, &rBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        //Vulkan flips +Y so we flip the viewport
        VkViewport viewport = {0, static_cast<float>(height), static_cast<float>(width), -static_cast<float>(height) , 0, 1};
        VkRect2D scissor = { {0, 0}, {width, height} };

        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuffer, 0, 1 ,&scissor);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmdBuffer);

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