#include "ImageWrap.h"
#include "DescriptorWrap.h"
#include "BufferWrap.h"
#include "Util.h"
#include "Graphics.h"
#include "Window.h"
#include "Camera.h"
#include "DOFPass.h"
#include "LightingPass.h"
#include "BufferDebugDraw.h"
#include "TileMaxPass.h"
#include "NeighbourMax.h"
#include "MBlurPass.h"
#include "PreDOFPass.h"
#include "MedianPass.h"
#include "UpscalePass.h"
#include "RayMaskPass.h"

#include <iostream>
#include "extensions_vk.hpp"

const char* APPLICATION_NAME = "Hybrid_framework";
const char* ENGINE_NAME = "vk_gfx";

void Graphics::CreateInstance(bool api_dump) {
    uint32_t GLFW_extension_count = 0;
    const char** req_GLFW_extensions = glfwGetRequiredInstanceExtensions(&GLFW_extension_count);

    assert(GLFW_extension_count > 0);
    
    std::cout << "GLFW required extensions : " << std::endl;
    //Append the GLFW extensions to the extension list to pass to the createInstance call
    for (unsigned int i = 0; i < GLFW_extension_count; i++) {
        instance_extensions.push_back(req_GLFW_extensions[i]);
        std::cout << "\t" << req_GLFW_extensions[i] << std::endl;
    }

    //TODO - Parse a command line argument to set/unset doApiDump
    if (api_dump)
        instance_layers.push_back("VK_LAYER_LUNARG_api_dump");

    // initialize the vk::ApplicationInfo structure
    vk::ApplicationInfo applicationInfo(APPLICATION_NAME, 1, ENGINE_NAME, 1, VK_API_VERSION_1_3);

    // initialize the vk::InstanceCreateInfo
    vk::InstanceCreateInfo instanceCreateInfo(
        vk::InstanceCreateFlags(), &applicationInfo,
        instance_layers.size(), instance_layers.data(),
        instance_extensions.size(), instance_extensions.data());

    // create an Instance
    m_instance = vk::createInstance(instanceCreateInfo);
    assert(m_instance);
}

void Graphics::CreatePhysicalDevice() {
    std::vector<vk::PhysicalDevice> available_devices = m_instance.enumeratePhysicalDevices();
    std::vector<uint32_t> compatible_devices;

    printf("%d devices enumerated\n", available_devices.size());
    vk::PhysicalDeviceProperties GPU_properties;
    std::vector<vk::ExtensionProperties> extension_properties;
    unsigned int extensionCount;

    // For each GPU:
    for (auto device : available_devices) {

        // Get the GPU's properties
        auto GPU_properties2 =
            device.getProperties2<
            vk::PhysicalDeviceProperties2,
            vk::PhysicalDeviceBlendOperationAdvancedPropertiesEXT>();
        GPU_properties = GPU_properties2.get<vk::PhysicalDeviceProperties2>().properties;
        if (GPU_properties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu) {
            std::cout << "Physical Device : " << GPU_properties.deviceName <<
                " Does not satisfy DISCRETE_GPU property" << std::endl;
            continue;
        }

        std::cout << "Physical Device : " << GPU_properties.deviceName <<
            " Satisfies DISCRETE_GPU property" << std::endl;

        // Get the GPU's extension list;  Another two-step list retrieval procedure:
        extension_properties =
            device.enumerateDeviceExtensionProperties();

        extensionCount = 0;
        for (auto const& extensionProperty : extension_properties) {
            for (auto const& reqExtension : device_extensions) {
                if (strcmp(extensionProperty.extensionName, reqExtension) == 0) {
                    extensionCount++;
                    if (extensionCount == device_extensions.size()) {
                        m_physical_device = device;
                        std::cout << "Physical Device : " << GPU_properties.deviceName <<
                            " satisfies required extensions" << std::endl;
                        return;
                    }
                    break;
                }
            }
        }

        std::cout << "Physical Device : " << GPU_properties.deviceName <<
            " Does not satisfy required extensions" << std::endl;
    }
}

void Graphics::ChooseQueueIndex() {
    vk::QueueFlags required_queue_flags = 
        vk::QueueFlagBits::eGraphics |
        vk::QueueFlagBits::eCompute |
        vk::QueueFlagBits::eTransfer;

    // need to explicitly specify all the template arguments for getQueueFamilyProperties2 to make the compiler happy
    using Chain = vk::StructureChain<vk::QueueFamilyProperties2,
        vk::QueueFamilyCheckpointPropertiesNV>;
    auto queueFamilyProperties2 =
        m_physical_device.getQueueFamilyProperties2<>();


    for (size_t j = 0; j < queueFamilyProperties2.size(); j++)
    {
        std::cout << "\t"
            << "QueueFamily " << j << "\n";
        vk::QueueFamilyProperties const& properties = queueFamilyProperties2[j].queueFamilyProperties;
        std::cout << "\t\t"
            << "QueueFamilyProperties:\n";
        std::cout << "\t\t\t"
            << "queueFlags                  = " << vk::to_string(properties.queueFlags) << "\n";
        std::cout << "\t\t\t"
            << "queueCount                  = " << properties.queueCount << "\n";
        std::cout << "\t\t\t"
            << "timestampValidBits          = " << properties.timestampValidBits << "\n";
        std::cout << "\t\t\t"
            << "minImageTransferGranularity = " << properties.minImageTransferGranularity.width << " x " << properties.minImageTransferGranularity.height
            << " x " << properties.minImageTransferGranularity.depth << "\n";
        std::cout << "\n";

        if (m_graphics_queue_index == VK_QUEUE_FAMILY_IGNORED) {
            if (properties.queueFlags & required_queue_flags)
                m_graphics_queue_index = j;
        }
    }

    if (m_graphics_queue_index == VK_QUEUE_FAMILY_IGNORED) {
        throw std::runtime_error("Unable to find a queue with the required flags");
    }
    else {
        std::cout << "Choosing Queue index: " << m_graphics_queue_index << std::endl;
    }
}

void Graphics::CreateDevice() {
    // Build a pNext chain of the following six "feature" structures:
    //   features2->features11->features12->features13->accelFeature->rtPipelineFeature->NULL

    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature;

    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelFeature;
    accelFeature.setPNext(&rtPipelineFeature);

    vk::PhysicalDeviceVulkan13Features feature13;
    feature13.setPNext(&accelFeature);

    vk::PhysicalDeviceVulkan12Features feature12;
    feature12.setPNext(&feature13);

    vk::PhysicalDeviceVulkan11Features feature11;
    feature11.setPNext(&feature12);

    vk::PhysicalDeviceFeatures2 feature2;
    feature2.setPNext(&feature11);

    m_physical_device.getFeatures2(&feature2);

    float priority = 0.0f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(),
        static_cast<uint32_t>(m_graphics_queue_index), 1, &priority);

    vk::DeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.setFlags(vk::DeviceCreateFlags());

    deviceCreateInfo.setQueueCreateInfoCount(1);
    deviceCreateInfo.setQueueCreateInfos(deviceQueueCreateInfo);

    deviceCreateInfo.setEnabledLayerCount(instance_layers.size());
    deviceCreateInfo.setPpEnabledLayerNames(instance_layers.data());

    deviceCreateInfo.setEnabledExtensionCount(device_extensions.size());
    deviceCreateInfo.setPpEnabledExtensionNames(device_extensions.data());

    deviceCreateInfo.setPNext(&feature2);

    m_device = m_physical_device.createDevice(deviceCreateInfo);
}

void Graphics::GetCommandQueue() {
    m_queue = m_device.getQueue(m_graphics_queue_index, 0);
}

vk::Format Graphics::GetSupportedDepthFormat() {
    // Since all depth formats may be optional, we need to find a suitable depth format to use
    // Start with the highest precision packed format
    std::vector<vk::Format> default_depth_formats = {
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD32Sfloat,
        vk::Format::eD24UnormS8Uint,
        vk::Format::eD16UnormS8Uint,
        vk::Format::eD16Unorm
    };

    for (auto& format : default_depth_formats)
    {
        vk::FormatProperties format_props = m_physical_device.getFormatProperties(format);
        // Format must support depth stencil attachment for optimal tiling
        if (format_props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            return format;
        }
    }

    return vk::Format::eUndefined;
}

// Calling load_VK_EXTENSIONS from extensions_vk.cpp.  A Python script
// from NVIDIA created extensions_vk.cpp from the current Vulkan spec
// for the purpose of loading the symbols for all registered
// extension.  This be (indistinguishable from) magic.
void Graphics::LoadExtensions() {
    load_VK_EXTENSIONS(m_instance, vkGetInstanceProcAddr, m_device, vkGetDeviceProcAddr);
}

void Graphics::GetSurface() {
    VkSurfaceKHR _surface;
    VkResult res = glfwCreateWindowSurface(m_instance, p_parent_window->GetGLFWPointer(), nullptr, &_surface);
    if (res == VK_SUCCESS)
        m_surface = vk::SurfaceKHR(_surface);
    else {
        throw std::runtime_error("Unable to create a Surface using GLFW");
    }

    if (m_physical_device.getSurfaceSupportKHR(m_graphics_queue_index, m_surface) == VK_TRUE)
        return;

    throw std::runtime_error("Could not create a supported surface");
}

void Graphics::CreateCommandPool() {
    m_cmd_pool = m_device.createCommandPool(
        vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            m_graphics_queue_index));

    // Create a command buffer
    m_cmd_buffer = m_device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo(m_cmd_pool, vk::CommandBufferLevel::ePrimary, 1)).front();
}

void Graphics::CreateSwapchain() {
    m_device.waitIdle();

    // Get the surface's capabilities
    vk::SurfaceCapabilitiesKHR surfaceCapabilities =
        m_physical_device.getSurfaceCapabilitiesKHR(m_surface);

    vk::Extent2D               swapchainExtent;
    if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max())
    {
        // Does this case ever happen?
        int width, height;
        glfwGetFramebufferSize(p_parent_window->GetGLFWPointer(), &width, &height);
        
        swapchainExtent.width = glm::clamp(swapchainExtent.width,
            surfaceCapabilities.minImageExtent.width,
            surfaceCapabilities.maxImageExtent.width);

        swapchainExtent.height = glm::clamp(swapchainExtent.height,
            surfaceCapabilities.minImageExtent.height,
            surfaceCapabilities.maxImageExtent.height);
    }
    else
    {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfaceCapabilities.currentExtent;
    }

    // Test against valid size, typically hit when windows are minimized.
    // The app must prevent triggering this code in such a case
    assert(swapchainExtent.width && swapchainExtent.height);

    std::vector<vk::PresentModeKHR> present_modes =
        m_physical_device.getSurfacePresentModesKHR(m_surface);

    // Choose VK_PRESENT_MODE_FIFO_KHR as a default (this must be supported)
    vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;

    auto present_mode_iter = std::find_if(present_modes.begin(), present_modes.end(),
        [](vk::PresentModeKHR const& _present_mode) { return _present_mode == vk::PresentModeKHR::eMailbox; });

    if (present_mode_iter != present_modes.end()) {
        swapchainPresentMode = *present_mode_iter;
        std::cout << "Mailbox Mode found as a present mode" << std::endl;
    }
    else
        std::cout << "Mailbox Mode NOT found as a present mode" << std::endl;

    std::vector<vk::SurfaceFormatKHR> formats =
        m_physical_device.getSurfaceFormatsKHR(m_surface);
    assert(!formats.empty());

    auto format_iter = std::find_if(formats.begin(), formats.end(),
        [](vk::SurfaceFormatKHR const& _format) { return _format.format == vk::Format::eB8G8R8A8Unorm; });

    vk::Format format;
    vk::ColorSpaceKHR colorSpace;
    if (format_iter != formats.end()) {
        format = (*format_iter).format;
        colorSpace = (*format_iter).colorSpace;
    }
    else
        throw std::runtime_error("Unable to find required Format");

    // Choose the number of swap chain images, within the bounds supported.
    glm::uint imageCount = surfaceCapabilities.minImageCount + 1; // Recommendation: minImageCount+1
    if (surfaceCapabilities.maxImageCount > 0
        && imageCount > surfaceCapabilities.maxImageCount) {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    assert(imageCount == 3); 

    // Create the swap chain
    vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eTransferDst;
    vk::SwapchainCreateInfoKHR swapChainCreateInfo(
        vk::SwapchainCreateFlagsKHR(),
        m_surface,
        imageCount,
        format,
        colorSpace,
        swapchainExtent,
        1,
        imageUsage,
        vk::SharingMode::eExclusive,
        {},
        surfaceCapabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        swapchainPresentMode,
        true,
        nullptr);

    m_swapchain = m_device.createSwapchainKHR(swapChainCreateInfo);

    m_swapchain_images = m_device.getSwapchainImagesKHR(m_swapchain);
    std::cout << "Image count is : " << imageCount << std::endl;
    std::cout << "Swapchain size is : " << m_swapchain_images.size() << std::endl;
    m_image_count = imageCount;

    m_barriers.reserve(m_image_count);
    m_image_views.reserve(m_image_count);

    vk::ImageViewCreateInfo imageViewCreateInfo({}, {},
        vk::ImageViewType::e2D,
        format, {},
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    for (auto image : m_swapchain_images)
    {
        imageViewCreateInfo.image = image;
        m_image_views.push_back(m_device.createImageView(imageViewCreateInfo));
    }

    // Create three VkImageMemoryBarrier structures (one for each swap
    // chain image) and specify the desired
    // layout (VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) for each.
    vk::ImageSubresourceRange res_range(vk::ImageAspectFlagBits::eColor, 0,
        VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS);

    for (auto image : m_swapchain_images) {
        vk::ImageMemoryBarrier mem_barrier({}, {},
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR,
            {}, {}, image, res_range);
        m_barriers.push_back(mem_barrier);
    }

    // Create a temporary command buffer. submit the layout conversion
    // command, submit and destroy the command buffer.
    vk::CommandBuffer cmd = CreateTempCommandBuffer();
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe,
        vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, m_image_count, m_barriers.data());

    SubmitTempCommandBuffer(cmd);

    // Create the three synchronization objects.  These are not
    // technically part of the swap chain, but they are used
    // exclusively for synchronizing the swap chain, so I include them
    // here.
    m_waitfence = m_device.createFence(
        vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));

    m_read_semaphore = m_device.createSemaphore(vk::SemaphoreCreateInfo());
    m_written_semaphore = m_device.createSemaphore(vk::SemaphoreCreateInfo());

    window_size = swapchainExtent;
    // To destroy:  Complete and call function destroySwapchain
}

void Graphics::DestroySwapchain() {
    vkDeviceWaitIdle(m_device);

    for (auto image_view : m_image_views) {
        m_device.destroyImageView(image_view);
    }
    m_device.destroyFence(m_waitfence);
    m_device.destroySemaphore(m_read_semaphore);
    m_device.destroySemaphore(m_written_semaphore);
    m_device.destroySwapchainKHR(m_swapchain);
    m_swapchain = VK_NULL_HANDLE;
    m_image_views.clear();
    m_barriers.clear();
}

vk::CommandBuffer Graphics::CreateTempCommandBuffer() {
    vk::CommandBuffer cmdBuffer = m_device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo(m_cmd_pool, vk::CommandBufferLevel::ePrimary, 1)).front();
    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmdBuffer.begin(beginInfo);

    return cmdBuffer;
}

void Graphics::SubmitTempCommandBuffer(vk::CommandBuffer cmd_buffer) {
    cmd_buffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.setCommandBufferCount(1);
    submitInfo.setPCommandBuffers(&cmd_buffer);
    m_queue.submit(1, &submitInfo, {});
    m_queue.waitIdle();
    m_device.freeCommandBuffers(m_cmd_pool, 1, &cmd_buffer);
}

void Graphics::CreateDepthResource() {
    uint8_t mipLevels = 1;

    vk::Format depth_format = GetSupportedDepthFormat();
    // Note m_depthImage is type ImageWrap; a tiny wrapper around
    // several related Vulkan objects.
    m_depth_image = ImageWrap(window_size.width, window_size.height,
        depth_format,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | 
        vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eDepth,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mipLevels, this);

    m_depth_image.TransitionImageLayout(vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal);
    m_depth_image.CreateTextureSampler();
}

void Graphics::CreatePostProcessRenderPass() {
    std::array<vk::AttachmentDescription, 2> attachments{};
    // Color attachment
    attachments[0].setFormat(vk::Format::eB8G8R8A8Unorm);
    attachments[0].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[0].setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
    attachments[0].setSamples(vk::SampleCountFlagBits::e1);

    // Depth attachment
    attachments[1].setFormat(m_depth_image.GetFormat());
    attachments[1].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[1].setStencilLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[1].setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
    attachments[1].setSamples(vk::SampleCountFlagBits::e1);

    const vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
    const vk::AttachmentReference depthReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);


    std::array<vk::SubpassDependency, 1> subpassDependencies;
    // Transition from final to initial (VK_SUBPASS_EXTERNAL refers to all commands executed outside of the actual renderpass)
    subpassDependencies[0].setSrcSubpass(VK_SUBPASS_EXTERNAL);
    subpassDependencies[0].setDstSubpass(0);
    subpassDependencies[0].setSrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
    subpassDependencies[0].setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    subpassDependencies[0].setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
    subpassDependencies[0].setDstAccessMask(
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
    subpassDependencies[0].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    vk::SubpassDescription subpassDescription;
    subpassDescription.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpassDescription.setColorAttachmentCount(1);
    subpassDescription.setColorAttachments(colorReference);
    subpassDescription.setPDepthStencilAttachment(&depthReference);

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.setAttachmentCount(attachments.size());
    renderPassInfo.setPAttachments(attachments.data());
    renderPassInfo.setSubpassCount(1);
    renderPassInfo.setPSubpasses(&subpassDescription);
    renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassInfo.setDependencyCount(subpassDependencies.size());
    renderPassInfo.setPDependencies(subpassDependencies.data());

    m_post_proc_render_pass = m_device.createRenderPass(renderPassInfo);
}

void Graphics::CreatePostFrameBuffers() {
    std::array<vk::ImageView, 2> fbattachments;

    // Create frame buffers for every swap chain image
    VkFramebufferCreateInfo _ci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    vk::FramebufferCreateInfo fbcreateInfo;
    fbcreateInfo.setRenderPass(m_post_proc_render_pass);
    fbcreateInfo.setWidth(window_size.width);
    fbcreateInfo.setHeight(window_size.height);
    fbcreateInfo.setLayers(1);
    fbcreateInfo.setAttachmentCount(2);
    fbcreateInfo.setPAttachments(fbattachments.data());

    // Each of the three swapchain images gets an associated frame
    // buffer, all sharing one depth buffer.
    m_framebuffers.reserve(m_image_count);
    for (uint32_t i = 0; i < m_image_count; i++) {
        fbattachments[0] = m_image_views[i];         // A color attachment from the swap chain
        fbattachments[1] = m_depth_image.GetImageView();  // A depth attachment
        m_framebuffers.push_back(m_device.createFramebuffer(fbcreateInfo));
    }
}

void Graphics::CreatePostDescriptor(const ImageWrap& scanline_buffer) {
    m_post_proc_desc.setBindings(m_device, {
            {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
            {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
            {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
        });
    m_post_proc_desc.write(m_device, 0, scanline_buffer.Descriptor());
}

void Graphics::CreatePostPipeline() {
    // Creating the pipeline layout
    vk::PipelineLayoutCreateInfo createInfo;
    // What we eventually want:
    //createInfo.setLayoutCount         = 1;
    //createInfo.pSetLayouts            = &m_scDesc.descSetLayout;
    // Push constants in the fragment shader
    //VkPushConstantRange pushConstantRanges = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)};
    //createInfo.pushConstantRangeCount = 1;
    //createInfo.pPushConstantRanges    = &pushConstantRanges;

    createInfo.setSetLayoutCount(1);
    createInfo.setPSetLayouts(&m_post_proc_desc.descSetLayout);

    // What we can do now as a first pass:
    m_post_proc_pipeline_layout = m_device.createPipelineLayout(createInfo);

    ////////////////////////////////////////////
    // Create the shaders
    ////////////////////////////////////////////
    vk::ShaderModule vertShaderModule = CreateShaderModule(LoadFileIntoString("spv/post.vert.spv"));
    vk::ShaderModule fragShaderModule = CreateShaderModule(LoadFileIntoString("spv/post.frag.spv"));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eVertex,
        vertShaderModule, "main");

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eFragment,
        fragShaderModule, "main");

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    //auto bindingDescription = Vertex::getBindingDescription();
    //auto attributeDescriptions = Vertex::getAttributeDescriptions();

    // No geometry in this pipeline's draw.
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList);

    vk::Viewport viewport;
    viewport.setWidth(window_size.width);
    viewport.setHeight(window_size.height);
    viewport.setMaxDepth(1.0f);

    vk::Rect2D scissor({ 0, 0 },
        vk::Extent2D(window_size.width, window_size.height));

    vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.setLineWidth(1.0f);

    vk::PipelineMultisampleStateCreateInfo multiSampling;

    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.setDepthTestEnable(VK_TRUE);
    depthStencil.setDepthWriteEnable(VK_TRUE);
    depthStencil.setDepthCompareOp(vk::CompareOp::eLessOrEqual);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE,
        vk::LogicOp::eCopy, 1, &colorBlendAttachment);

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.setStageCount(2);
    pipelineCreateInfo.setPStages(shaderStages);
    pipelineCreateInfo.setPVertexInputState(&vertexInputInfo);
    pipelineCreateInfo.setPInputAssemblyState(&inputAssembly);
    pipelineCreateInfo.setPViewportState(&viewportState);
    pipelineCreateInfo.setPRasterizationState(&rasterizer);
    pipelineCreateInfo.setPMultisampleState(&multiSampling);
    pipelineCreateInfo.setPDepthStencilState(&depthStencil);
    pipelineCreateInfo.setPColorBlendState(&colorBlending);
    pipelineCreateInfo.setLayout(m_post_proc_pipeline_layout);
    pipelineCreateInfo.setRenderPass(m_post_proc_render_pass);
    pipelineCreateInfo.setSubpass(0);
    pipelineCreateInfo.setBasePipelineHandle(nullptr);

    vk::Result   result;
    vk::Pipeline pipeline;
    std::tie(result, pipeline) = m_device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineCreateInfo);


    switch (result)
    {
    case vk::Result::eSuccess:
        m_post_proc_pipeline = pipeline;
        break;
    case vk::Result::ePipelineCompileRequiredEXT:
        // something meaningfull here
        break;
    default: assert(false);  // should never happen
    }

    m_device.destroyShaderModule(fragShaderModule);
    m_device.destroyShaderModule(vertShaderModule);
    // To destroy:  vkDestroyPipelineLayout(m_device, m_postPipelineLayout, nullptr);
    //  and:        vkDestroyPipeline(m_device, m_postPipeline, nullptr);
}

void Graphics::InitGUI() {
    glm::uint subpassID = 0;

    // UI
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    std::vector<VkDescriptorPoolSize> poolSize{ {VK_DESCRIPTOR_TYPE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} };
    VkDescriptorPoolCreateInfo        poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSize.data();
    vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_imgui_descpool);

    // Setup Platform/Renderer back ends
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_instance;
    init_info.PhysicalDevice = m_physical_device;
    init_info.Device = m_device;
    init_info.QueueFamily = m_graphics_queue_index;
    init_info.Queue = m_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_imgui_descpool;
    init_info.Subpass = subpassID;
    init_info.MinImageCount = 2;
    init_info.ImageCount = m_image_count;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = nullptr;
    init_info.Allocator = nullptr;

    ImGui_ImplVulkan_Init(&init_info, m_post_proc_render_pass);

    // Upload Fonts
    VkCommandBuffer cmdbuf = CreateTempCommandBuffer();
    ImGui_ImplVulkan_CreateFontsTexture(cmdbuf);
    SubmitTempCommandBuffer(cmdbuf);

    ImGui_ImplGlfw_InitForVulkan(p_parent_window->GetGLFWPointer(), true);
}

void Graphics::DrawGUI() {
    for (auto& pass : render_passes) {
        pass->DrawGUI();
    }
}


void Graphics::PostProcess() {
    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color = vk::ClearColorValue(std::array<float, 4>({ { 1.0f, 1.0f, 1.0f, 1.0f } }));
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderPassBeginInfo renderPassBeginInfo(
        m_post_proc_render_pass, m_framebuffers[m_swapchain_index],
        vk::Rect2D(vk::Offset2D(0, 0), VkExtent2D(window_size)), clearValues);

    m_cmd_buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    {   // extra indent for renderpass commands
        auto aspectRatio = static_cast<float>(window_size.width)
            / static_cast<float>(window_size.height);

        m_cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_post_proc_pipeline);

        m_cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, 
            m_post_proc_pipeline_layout, 0, 1, &m_post_proc_desc.descSet, 0, nullptr);
        // Weird! This draws 3 vertices but with no vertices/triangles buffers bound in.
        // Hint: The vertex shader fabricates vertices from gl_VertexIndex
        m_cmd_buffer.draw(3, 1, 0, 0);

#ifdef GUI
        ImGui::Render();  // Rendering UI
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_cmd_buffer);
#endif
    }
    m_cmd_buffer.endRenderPass();
}

void Graphics::Teardown() {
    m_device.waitIdle();

    m_device.destroyDescriptorPool(m_imgui_descpool, nullptr);
    ImGui_ImplVulkan_Shutdown();

    DestroyUniformData();

    m_device.destroyPipelineLayout(m_post_proc_pipeline_layout);
    m_device.destroyPipeline(m_post_proc_pipeline);

    for (auto framebuffer : m_framebuffers) {
        m_device.destroyFramebuffer(framebuffer);
    }
    m_device.destroyRenderPass(m_post_proc_render_pass);

    for (auto& render_pass : render_passes) {
        render_pass.reset();
    }

    m_depth_image.destroy(m_device);
    m_post_proc_desc.destroy(m_device);
    DestroySwapchain();
    m_device.destroyCommandPool(m_cmd_pool);
    m_instance.destroySurfaceKHR(m_surface);
    m_device.destroy();
    m_instance.destroy();
}

void Graphics::DestroyUniformData() {
    for (auto ob : m_objData) ob.destroy(m_device);
    for (auto t : m_objText) t.destroy(m_device);
    
    m_objDescriptionBW.destroy(m_device);
    m_matrixBW.destroy(m_device);
    m_lightBW.destroy(m_device);
}

Graphics::Graphics(Window* _p_parent_window, bool api_dump) :
    p_parent_window(_p_parent_window), do_post_process(true) {
	CreateInstance(api_dump);
    CreatePhysicalDevice();
    ChooseQueueIndex();
    CreateDevice();
    GetCommandQueue();

    LoadExtensions();

    GetSurface();
    CreateCommandPool();

    CreateSwapchain();
    CreateDepthResource();
    CreatePostProcessRenderPass();
    CreatePostFrameBuffers();

    InitGUI();
    
    /*
    * https://benedikt-bitterli.me/resources/
    */
    LoadModel("models/fireplace_room/fireplace_room.obj", glm::mat4());
    CreateMatrixBuffer();
    CreateObjDescriptionBuffer();
    
    std::unique_ptr<LightingPass> p_lighting_pass = std::make_unique<LightingPass>(this);
    CreatePostDescriptor(p_lighting_pass->GetBufferRef());
    CreatePostPipeline();
    
    //Add the TileMaxPass to the list of passes.
    std::unique_ptr<TileMaxPass> p_tile_max_pass =
        std::make_unique<TileMaxPass>(this, p_lighting_pass.get());
    //Add the NeighbourMax to the list of passes.
    std::unique_ptr<NeighbourMax> p_neighbour_max_pass =
        std::make_unique<NeighbourMax>(this, p_tile_max_pass.get());

    //Add the pre DOF pass
    std::unique_ptr<PreDOFPass> p_pre_dof_pass = 
        std::make_unique<PreDOFPass>(this, p_lighting_pass.get());
    p_pre_dof_pass->SetNeighbourMaxBufferDesc(p_neighbour_max_pass->GetBuffer());

    //Add the depth of field pass to the list of passes.
    std::unique_ptr<DOFPass> p_dof_pass = std::make_unique<DOFPass>(this, p_pre_dof_pass.get());
    p_dof_pass->SetNeighbourMaxBufferDesc(p_neighbour_max_pass->GetBuffer());
    //Make sure TileMaxPass can access DOFPass for the DOF parameters
    p_tile_max_pass->SetDOFPass(p_dof_pass.get());
    //Make sure PreDOFPass can access DOFPass for the DOF parameters
    p_pre_dof_pass->SetDOFPass(p_dof_pass.get());

    //Add the median pass to the list of passes.
    std::unique_ptr<MedianPass> p_median_pass =
        std::make_unique<MedianPass>(this, p_dof_pass.get());

    //Add the upscale pass to the list of passes
    std::unique_ptr<UpscalePass> p_upscale_pass = 
        std::make_unique<UpscalePass>(this, p_median_pass.get());
    p_upscale_pass->SetFullResBufferDesc(p_lighting_pass->GetBufferRef());
    p_upscale_pass->SetFullResDepthBufferDesc(p_lighting_pass->GetVeloDepthBufferRef());
    p_upscale_pass->SetNeighbourBufferDesc(p_neighbour_max_pass->GetBuffer());
    p_upscale_pass->SetDOFPass(p_dof_pass.get());

    //Add a raymask pass to the list of passes
    std::unique_ptr<RayMaskPass> p_raymask_pass =
        std::make_unique<RayMaskPass>(this, p_pre_dof_pass.get());

    //Add the MBlur pass to the list of passes.
    std::unique_ptr<MBlurPass> p_mblur_pass = std::make_unique<MBlurPass>(this, p_lighting_pass.get());
    p_mblur_pass->SetNeighbourMaxDesc(p_neighbour_max_pass->GetBuffer());

    //Add the debug buffer draw pass to the list of passes.
    std::unique_ptr<BufferDebugDraw> p_debug_buffer_pass = 
        std::make_unique<BufferDebugDraw>(this, p_dof_pass.get());
    p_debug_buffer_pass->SetDOFPass(p_dof_pass.get());
    p_debug_buffer_pass->SetVeloDepthBuffer(p_lighting_pass->GetVeloDepthBufferRef());
    p_debug_buffer_pass->SetTileMaxBuffer(p_tile_max_pass->GetBuffer());
    p_debug_buffer_pass->SetNeighbourMaxBuffer(p_neighbour_max_pass->GetBuffer());
    p_debug_buffer_pass->SetPreDOFBuffer(p_pre_dof_pass->GetBuffer());
    p_debug_buffer_pass->SetPreDOFParamsBuffer(p_pre_dof_pass->GetParamsBuffer());
    p_debug_buffer_pass->SetDOFBGBuffer(p_dof_pass->GetBGBuffer());
    p_debug_buffer_pass->SetDOFFGBuffer(p_dof_pass->GetFGBuffer());
    p_debug_buffer_pass->SetDOFBuffer(p_dof_pass->GetBuffer());
    p_debug_buffer_pass->SetMedianBuffer(p_median_pass->GetBuffer());
    p_debug_buffer_pass->SetUpscaledBuffer(p_upscale_pass->GetBuffer());
    p_debug_buffer_pass->SetRaymaskBuffer(p_raymask_pass->GetBuffer());


    render_passes.push_back(std::move(p_lighting_pass));
    render_passes.push_back(std::move(p_tile_max_pass));
    render_passes.push_back(std::move(p_neighbour_max_pass));
    render_passes.push_back(std::move(p_pre_dof_pass));
    render_passes.push_back(std::move(p_dof_pass));
    render_passes.push_back(std::move(p_median_pass));
    render_passes.push_back(std::move(p_upscale_pass));
    render_passes.push_back(std::move(p_mblur_pass));
    render_passes.push_back(std::move(p_raymask_pass));
    render_passes.push_back(std::move(p_debug_buffer_pass));

    //Setup all the render passes
    for (auto& render_pass : render_passes) {
        render_pass->Setup();
    }
}

void Graphics::SetActiveCamPtr(Camera* p_cam) {
    p_active_cam = p_cam;
}

void Graphics::PrepareFrame() {
    m_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_read_semaphore,
        (VkFence)VK_NULL_HANDLE, &m_swapchain_index);

    // Check if window has been resized -- or other(??) swapchain specific event
    //if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    //    recreateSizedResources(VkExtent2D(windowSize)); }

    // Use a fence to wait until the command buffer has finished execution before using it again
    while (vk::Result::eTimeout == m_device.waitForFences(1, &m_waitfence, VK_TRUE, 1'000'000))
    {
    }
}

void Graphics::SubmitFrame() {
    m_device.resetFences(1, &m_waitfence);

    // Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
    const vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    // The submit info structure specifies a command buffer queue submission batch
    vk::SubmitInfo submitInfo(1, &m_read_semaphore,
        &waitStageMask,
        1, &m_cmd_buffer,
        1, &m_written_semaphore);
    m_queue.submit(1, &submitInfo, m_waitfence);

    vk::PresentInfoKHR presentInfo(1, &m_written_semaphore,
        1, &m_swapchain, &m_swapchain_index);
    m_queue.presentKHR(presentInfo);
}

void Graphics::DrawFrame() {
    PrepareFrame();

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    m_cmd_buffer.begin(beginInfo);
    {   // Extra indent for recording commands into m_commandBuffer
        UpdateCameraBuffer();

        for (auto& render_pass : render_passes) {
            render_pass->Render();
        }

        if (do_post_process)
            PostProcess(); //  tone mapper and output to swapchain image.

        m_cmd_buffer.end();
    }   // Done recording;  Execute!

    SubmitFrame();
}

Camera* Graphics::GetCamera() {
    return p_active_cam;
}

void Graphics::EnablePostProcess() {
    do_post_process = true;
}

void Graphics::DisablePostProcess() {
    do_post_process = false;
}

// Gets a list of memory types supported by the GPU, and search
// through that list for one that matches the requested properties
// flag.  The (only?) two types requested here are:
//
// (1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: For the bulk of the memory
// used by the GPU to store things internally.
//
// (2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
// for memory visible to the CPU  for CPU to GPU copy operations.
uint8_t Graphics::FindMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties) const {
    vk::PhysicalDeviceMemoryProperties memoryProperties = m_physical_device.getMemoryProperties();
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if ((typeBits & (1 << i)) &&
            ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
        {
            return i;
        }
    }
}

void Graphics::UpdateCameraBuffer() {
    // Prepare new UBO contents on host.
    const float    aspectRatio = window_size.width / static_cast<float>(window_size.height);
    MatrixUniforms hostUBO = {};

    glm::mat4    view = p_active_cam->view();
    glm::mat4    proj = p_active_cam->perspective(aspectRatio);

    hostUBO.priorViewProj = m_prior_viewproj;
    hostUBO.viewProj = proj * view;
    m_prior_viewproj = hostUBO.viewProj;
    hostUBO.viewInverse = glm::inverse(view);
    hostUBO.projInverse = glm::inverse(proj);

    // UBO on the device, and what stages access it.
    vk::Buffer deviceUBO = m_matrixBW.buffer;
    auto     uboUsageStages = vk::PipelineStageFlagBits::eVertexShader |
        vk::PipelineStageFlagBits::eRayTracingShaderKHR;


    // Ensure that the modified UBO is not visible to previous frames.
    vk::BufferMemoryBarrier beforeBarrier;
    beforeBarrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
    beforeBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
    beforeBarrier.setBuffer(deviceUBO);
    beforeBarrier.setOffset(0);
    beforeBarrier.setSize(sizeof(hostUBO));

    m_cmd_buffer.pipelineBarrier(uboUsageStages, vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlagBits::eDeviceGroup, 0, nullptr,
        1, &beforeBarrier, 0, nullptr);

    // Schedule the host-to-device upload. (hostUBO is copied into the cmd
    // buffer so it is okay to deallocate when the function returns).
    m_cmd_buffer.updateBuffer(m_matrixBW.buffer, vk::DeviceSize(0), sizeof(MatrixUniforms), &hostUBO);

    // Making sure the updated UBO will be visible.
    vk::BufferMemoryBarrier afterBarrier;
    afterBarrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
    afterBarrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    afterBarrier.setBuffer(deviceUBO);
    afterBarrier.setOffset(0);
    afterBarrier.setSize(sizeof(hostUBO));
    m_cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, uboUsageStages,
        vk::DependencyFlagBits::eDeviceGroup,
        0, nullptr,
        1, &afterBarrier, 0, nullptr);
}

vk::Sampler Graphics::CreateTextureSampler() {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physical_device, &properties);

    VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler textureSampler;
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
    return textureSampler;
}

void Graphics::ImageLayoutBarrier(vk::CommandBuffer cmdbuffer,
    vk::Image image,
    vk::ImageLayout oldImageLayout,
    vk::ImageLayout newImageLayout,
    vk::ImageAspectFlags aspectMask) const
{
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.setAspectMask(aspectMask);
    subresourceRange.setLevelCount(VK_REMAINING_MIP_LEVELS);
    subresourceRange.setLayerCount(VK_REMAINING_ARRAY_LAYERS);
    subresourceRange.setBaseMipLevel(0);
    subresourceRange.setBaseArrayLayer(0);

    vk::ImageMemoryBarrier imageMemoryBarrier;

    imageMemoryBarrier.setOldLayout(oldImageLayout);
    imageMemoryBarrier.setNewLayout(newImageLayout);
    imageMemoryBarrier.setImage(image);
    imageMemoryBarrier.setSubresourceRange(subresourceRange);
    imageMemoryBarrier.setSrcAccessMask(AccessFlagsForImageLayout(oldImageLayout));
    imageMemoryBarrier.setDstAccessMask(AccessFlagsForImageLayout(newImageLayout));
    imageMemoryBarrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    imageMemoryBarrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    vk::PipelineStageFlags srcStageMask = PipelineStageForLayout(oldImageLayout);
    vk::PipelineStageFlags destStageMask = PipelineStageForLayout(newImageLayout);

    cmdbuffer.pipelineBarrier(srcStageMask, destStageMask, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

void Graphics::TransitionImageLayout(vk::Image image, vk::Format format, 
    vk::ImageLayout oldLayout, vk::ImageLayout newLayout, 
    uint32_t mipLevels) {
    vk::CommandBuffer commandBuffer = CreateTempCommandBuffer();

    vk::ImageMemoryBarrier barrier;
    barrier.setOldLayout(oldLayout);
    barrier.setNewLayout(newLayout);
    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setImage(image);

    vk::ImageSubresourceRange subResRange;
    subResRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    subResRange.setBaseMipLevel(0);
    subResRange.setLevelCount(mipLevels);
    subResRange.setBaseArrayLayer(0);
    subResRange.setLayerCount(1);
    barrier.setSubresourceRange(subResRange);
    barrier.setSrcAccessMask(vk::AccessFlagBits::eNone);
    barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

    vk::PipelineStageFlags sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    vk::PipelineStageFlags destinationStage = vk::PipelineStageFlagBits::eTransfer;

    commandBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &barrier);

    SubmitTempCommandBuffer(commandBuffer);
}

vk::ShaderModule Graphics::CreateShaderModule(std::string code) {
    vk::ShaderModuleCreateInfo sm_createInfo(vk::ShaderModuleCreateFlags(),
        code.size(), (uint32_t*)code.data(), nullptr);

    return m_device.createShaderModule(sm_createInfo);
}

BufferWrap Graphics::CreateStagedBufferWrap(const vk::CommandBuffer& cmdBuf, const vk::DeviceSize& size, 
    const void* data, vk::BufferUsageFlags usage) {
    BufferWrap staging = CreateBufferWrap(size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible
        | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* dest;
    m_device.mapMemory(staging.memory, 0, size, vk::MemoryMapFlags(), &dest);
    memcpy(dest, data, size);
    m_device.unmapMemory(staging.memory);


    BufferWrap bw = CreateBufferWrap(size, vk::BufferUsageFlagBits::eTransferDst | usage,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    CopyBuffer(staging.buffer, bw.buffer, size);

    staging.destroy(m_device);

    return bw;
}

vec2 Graphics::GetWindowSize() const {
    return vec2(window_size.width, window_size.height);
}

const vk::Extent2D& Graphics::GetWindowExtent() const {
    return window_size;
}

uint32_t Graphics::GetCurrentSwapchainIndex() const {
    return m_swapchain_index;
}

const std::vector<vk::ImageView>& Graphics::GetSwapChainImageViews() const {
    return m_image_views;
}

const ImageWrap& Graphics::GetDepthBuffer() const {
    return m_depth_image;
}

void Graphics::GenerateMipmaps(vk::Image image, vk::Format imageFormat,
    int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    // Check if image format supports linear blitting
    vk::FormatProperties formatProperties = m_physical_device.getFormatProperties(imageFormat);
    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    vk::CommandBuffer commandBuffer = CreateTempCommandBuffer();


    vk::ImageMemoryBarrier barrier;
    barrier.setImage(image);
    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    subresourceRange.setLevelCount(1);
    subresourceRange.setLayerCount(1);
    subresourceRange.setBaseArrayLayer(0);

    barrier.setSubresourceRange(subresourceRange);

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        subresourceRange.setBaseMipLevel(i - 1);
        barrier.setSubresourceRange(subresourceRange);
        barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
        barrier.setNewLayout(vk::ImageLayout::eTransferSrcOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
        barrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
            0, nullptr, 0, nullptr, 1, &barrier);

        vk::ImageBlit blit;

        std::array<vk::Offset3D, 2> srcOffsets;
        srcOffsets[0] = vk::Offset3D{ 0, 0, 0 };
        srcOffsets[1] = vk::Offset3D{ mipWidth, mipHeight, 1 };
        blit.setSrcOffsets(srcOffsets);
        vk::ImageSubresourceLayers srcsubresourceLayers;
        srcsubresourceLayers.setAspectMask(vk::ImageAspectFlagBits::eColor);
        srcsubresourceLayers.setMipLevel(i - 1);
        srcsubresourceLayers.setBaseArrayLayer(0);
        srcsubresourceLayers.setLayerCount(1);
        blit.setSrcSubresource(srcsubresourceLayers);

        std::array<vk::Offset3D, 2> dstOffsets;
        dstOffsets[0] = vk::Offset3D{ 0, 0, 0 };
        dstOffsets[1] = vk::Offset3D{ mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.setDstOffsets(dstOffsets);
        vk::ImageSubresourceLayers dstsubresourceLayers;
        dstsubresourceLayers.setAspectMask(vk::ImageAspectFlagBits::eColor);
        dstsubresourceLayers.setMipLevel(i);
        dstsubresourceLayers.setBaseArrayLayer(0);
        dstsubresourceLayers.setLayerCount(1);
        blit.setDstSubresource(dstsubresourceLayers);

        commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
            image, vk::ImageLayout::eTransferDstOptimal,
            1, &blit, vk::Filter::eLinear);

        barrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal);
        barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferRead);
        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(),
            0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    subresourceRange.setBaseMipLevel(mipLevels - 1);
    barrier.setSubresourceRange(subresourceRange);
    barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
    barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
    barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(),
        0, nullptr, 0, nullptr, 1, &barrier);

    SubmitTempCommandBuffer(commandBuffer);
}

BufferWrap Graphics::CreateBufferWrap(vk::DeviceSize size, vk::BufferUsageFlags usage, 
    vk::MemoryPropertyFlags properties) {
    BufferWrap result;

    vk::BufferCreateInfo bufferInfo;
    bufferInfo.setSize(size);
    bufferInfo.setUsage(usage);
    bufferInfo.setSharingMode(vk::SharingMode::eExclusive);

    m_device.createBuffer(&bufferInfo, nullptr, &result.buffer);

    vk::MemoryRequirements memRequirements;
    m_device.getBufferMemoryRequirements(result.buffer, &memRequirements);

    vk::MemoryAllocateFlagsInfo memFlags;
    memFlags.setFlags(vk::MemoryAllocateFlagBits::eDeviceAddress);
    memFlags.setDeviceMask(0);

    vk::MemoryAllocateInfo allocInfo;

    allocInfo.setPNext(&memFlags);
    allocInfo.setAllocationSize(memRequirements.size);
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (m_device.allocateMemory(&allocInfo, nullptr, &result.memory) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    m_device.bindBufferMemory(result.buffer, result.memory, 0);

    return result;
}

void Graphics::CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {
    vk::CommandBuffer commandBuffer = CreateTempCommandBuffer();

    vk::BufferCopy copyRegion;
    copyRegion.setSize(size);

    commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

    SubmitTempCommandBuffer(commandBuffer);
}

const vk::CommandBuffer& Graphics::GetCommandBuffer() const {
    return m_cmd_buffer;
}

void Graphics::CommandCopyImage(const ImageWrap& src, const ImageWrap& dst) const {
    vk::ImageCopy img_copy_region;
    img_copy_region.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    img_copy_region.srcSubresource.layerCount = 1;
    img_copy_region.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    img_copy_region.dstSubresource.layerCount = 1;
    img_copy_region.extent.width = window_size.width;
    img_copy_region.extent.height = window_size.height;
    img_copy_region.extent.depth = 1;

    ImageLayoutBarrier(m_cmd_buffer, src.GetImage(),
        vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
    ImageLayoutBarrier(m_cmd_buffer, dst.GetImage(),
        vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal);

    m_cmd_buffer.copyImage(src.GetImage(), vk::ImageLayout::eTransferSrcOptimal,
        dst.GetImage(), vk::ImageLayout::eTransferDstOptimal,
        1, &img_copy_region);

    ImageLayoutBarrier(m_cmd_buffer, dst.GetImage(),
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral);
    ImageLayoutBarrier(m_cmd_buffer, src.GetImage(),
        vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);
}


vk::AccessFlags AccessFlagsForImageLayout(vk::ImageLayout layout) {
    switch (layout)
    {
    case vk::ImageLayout::ePreinitialized:
        return vk::AccessFlagBits::eHostWrite;
    case vk::ImageLayout::eTransferDstOptimal:
        return vk::AccessFlagBits::eTransferWrite;
    case vk::ImageLayout::eTransferSrcOptimal:
        return vk::AccessFlagBits::eTransferRead;
    case vk::ImageLayout::eColorAttachmentOptimal:
        return vk::AccessFlagBits::eColorAttachmentWrite;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        return vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
        return vk::AccessFlagBits::eShaderRead;
    default:
        return vk::AccessFlags();
    }
}

vk::PipelineStageFlags PipelineStageForLayout(vk::ImageLayout layout)
{
    switch (layout)
    {
    case vk::ImageLayout::eTransferDstOptimal:
    case vk::ImageLayout::eTransferSrcOptimal:
        return vk::PipelineStageFlagBits::eTransfer;
    case vk::ImageLayout::eColorAttachmentOptimal:
        return vk::PipelineStageFlagBits::eColorAttachmentOutput;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        return vk::PipelineStageFlagBits::eAllCommands;  // Allow queue other than graphic
        // return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
        return vk::PipelineStageFlagBits::eAllCommands;  // Allow queue other than graphic
        // return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case vk::ImageLayout::ePreinitialized:
        return vk::PipelineStageFlagBits::eHost;
    case vk::ImageLayout::eUndefined:
        return vk::PipelineStageFlagBits::eTopOfPipe;
    default:
        return vk::PipelineStageFlagBits::eBottomOfPipe;
    }
}