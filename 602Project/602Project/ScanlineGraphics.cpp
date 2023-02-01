#include "Graphics.h"

void Graphics::CreateScanlineBufferImage(const vk::Extent2D& size) {
    //uint mipLevels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
    uint8_t mipLevels = 1;

    m_sc_img_buffer = ImageWrap(size.width, size.height, vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mipLevels, this);

    m_sc_img_buffer.sampler = CreateTextureSampler();
    m_sc_img_buffer.image_layout = vk::ImageLayout::eGeneral;
}

void Graphics::CreateScanlineRenderPass() {
    vk::AttachmentDescription colorAttachment;
    colorAttachment.setFormat(vk::Format::eR32G32B32A32Sfloat);
    colorAttachment.setSamples(vk::SampleCountFlagBits::e1);
    colorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colorAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colorAttachment.setInitialLayout(vk::ImageLayout::eGeneral);
    colorAttachment.setFinalLayout(vk::ImageLayout::eGeneral);

    vk::AttachmentDescription depthAttachment;
    depthAttachment.setFormat(vk::Format::eX8D24UnormPack32);
    depthAttachment.setSamples(vk::SampleCountFlagBits::e1);
    depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    depthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    depthAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentReference colorAttachmentRef;
    colorAttachmentRef.setAttachment(0);
    colorAttachmentRef.setLayout(vk::ImageLayout::eColorAttachmentOptimal);


    vk::AttachmentReference depthAttachmentRef;
    depthAttachmentRef.setAttachment(1);
    depthAttachmentRef.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::SubpassDescription subpass;
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpass.setColorAttachmentCount(1);
    subpass.setPColorAttachments(&colorAttachmentRef);
    subpass.setPDepthStencilAttachment(&depthAttachmentRef);

    vk::SubpassDependency dependency;
    dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
    dependency.setDstSubpass(0);
    dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests);
    dependency.setSrcAccessMask(vk::AccessFlagBits::eNoneKHR);
    dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests);
    dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
        vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    std::array<vk::AttachmentDescription, 2> attachmentsDsc = { colorAttachment, depthAttachment };

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.setAttachmentCount(static_cast<uint32_t>(attachmentsDsc.size()));
    renderPassInfo.setPAttachments(attachmentsDsc.data());
    renderPassInfo.setSubpassCount(1);
    renderPassInfo.setPSubpasses(&subpass);
    renderPassInfo.setDependencyCount(1);
    renderPassInfo.setPDependencies(&dependency);

    if (m_device.createRenderPass(&renderPassInfo, nullptr, &m_scanline_renderpass) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create scanline render pass!");
    }

    std::vector<vk::ImageView> attachments = { m_sc_img_buffer.image_view, m_depth_image.image_view };

    vk::FramebufferCreateInfo info;
    info.setRenderPass(m_scanline_renderpass);
    info.setAttachmentCount(attachments.size());
    info.setPAttachments(attachments.data());
    info.setWidth(window_size.width);
    info.setHeight(window_size.height);
    info.setLayers(1);

    if (m_device.createFramebuffer(&info, nullptr, &m_scanline_framebuffer) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create scanline render pass!");
    }
}

void Graphics::CreateScDescriptorSet() {
    auto nbTxt = static_cast<uint32_t>(m_objText.size());

    m_scanline_desc.setBindings(m_device, {
            {ScBindings::eMatrices, vk::DescriptorType::eUniformBuffer, 1,
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR},
            {ScBindings::eObjDescs, vk::DescriptorType::eStorageBuffer, 1,
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment |
                vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR},
            {ScBindings::eTextures, vk::DescriptorType::eCombinedImageSampler, nbTxt,
                vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR}
        });

    m_scanline_desc.write(m_device, ScBindings::eMatrices, m_matrixBW.buffer);
    m_scanline_desc.write(m_device, ScBindings::eObjDescs, m_objDescriptionBW.buffer);
    m_scanline_desc.write(m_device, ScBindings::eTextures, m_objText);
}

void Graphics::CreateObjDescriptionBuffer() {
    vk::CommandBuffer cmdBuf = CreateTempCommandBuffer();

    m_objDescriptionBW = CreateStagedBufferWrap(cmdBuf, m_objDesc, vk::BufferUsageFlagBits::eStorageBuffer);

    SubmitTempCommandBuffer(cmdBuf);
}

void Graphics::CreateScanlineFrameBuffers() {
    CreateScanlineBufferImage(window_size);

    vk::CommandBuffer    cmdBuf = CreateTempCommandBuffer();
    ImageLayoutBarrier(cmdBuf, m_sc_img_buffer.image,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

    SubmitTempCommandBuffer(cmdBuf);
}

void Graphics::CreateMatrixBuffer() {
    m_matrixBW = CreateBufferWrap(sizeof(MatrixUniforms),
        vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
}

void Graphics::CreateScPipeline() {
    vk::PushConstantRange pushConstantRanges = {
       vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstantRaster) };

    // Creating the Pipeline Layout
    vk::PipelineLayoutCreateInfo createInfo;
    createInfo.setSetLayoutCount(1);
    createInfo.setPSetLayouts(&m_scanline_desc.descSetLayout);
    createInfo.setPushConstantRangeCount(1);
    createInfo.setPPushConstantRanges(&pushConstantRanges);
    m_device.createPipelineLayout(&createInfo, nullptr, &m_scanline_pipeline_layout);


    vk::ShaderModule vertShaderModule = CreateShaderModule(LoadFileIntoString("spv/scanline.vert.spv"));
    vk::ShaderModule fragShaderModule = CreateShaderModule(LoadFileIntoString("spv/scanline.frag.spv"));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
    vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
    vertShaderStageInfo.setModule(vertShaderModule);
    vertShaderStageInfo.setPName("main");

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
    fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
    fragShaderStageInfo.setModule(fragShaderModule);
    fragShaderStageInfo.setPName("main");

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    vk::VertexInputBindingDescription bindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);


    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions{
        {0, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, pos))},
        {1, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, nrm))},
        {2, 0, vk::Format::eR32G32Sfloat, static_cast<uint32_t>(offsetof(Vertex, texCoord))} };


    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.setVertexBindingDescriptionCount(1);
    vertexInputInfo.setPVertexBindingDescriptions(&bindingDescription);

    vertexInputInfo.setVertexAttributeDescriptionCount(attributeDescriptions.size());
    vertexInputInfo.setPVertexAttributeDescriptions(attributeDescriptions.data());

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);
    inputAssembly.setPrimitiveRestartEnable(VK_FALSE);

    vk::Viewport viewport;
    viewport.setX(0.0f);
    viewport.setY(0.0f);
    viewport.setWidth((float)window_size.width);
    viewport.setHeight((float)window_size.height);
    viewport.setMinDepth(0.0f);
    viewport.setMaxDepth(1.0f);

    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 });
    scissor.setExtent(vk::Extent2D{ window_size.width, window_size.height });

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.setViewportCount(1);
    viewportState.setPViewports(&viewport);
    viewportState.setScissorCount(1);
    viewportState.setPScissors(&scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.setDepthClampEnable(VK_FALSE);
    rasterizer.setRasterizerDiscardEnable(VK_FALSE);
    rasterizer.setPolygonMode(vk::PolygonMode::eFill);
    rasterizer.setLineWidth(1.0f);
    rasterizer.setCullMode(vk::CullModeFlagBits::eNone); //??
    rasterizer.setFrontFace(vk::FrontFace::eCounterClockwise);
    rasterizer.setDepthBiasEnable(VK_FALSE);

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.setSampleShadingEnable(VK_FALSE);
    multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1);

    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.setDepthTestEnable(VK_TRUE);
    depthStencil.setDepthWriteEnable(VK_TRUE);
    depthStencil.setDepthCompareOp(vk::CompareOp::eLessOrEqual);// BEWARE!!  NECESSARY!!
    depthStencil.setDepthBoundsTestEnable(VK_FALSE);
    depthStencil.setStencilTestEnable(VK_FALSE);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    colorBlendAttachment.setBlendEnable(VK_FALSE);

    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.setLogicOpEnable(VK_FALSE);
    colorBlending.setLogicOp(vk::LogicOp::eCopy);
    colorBlending.setAttachmentCount(1);
    colorBlending.setPAttachments(&colorBlendAttachment);
    colorBlending.setBlendConstants({ 0.0f , 0.0f , 0.0f , 0.0f });

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.setStageCount(2);
    pipelineInfo.setPStages(shaderStages);
    pipelineInfo.setPVertexInputState(&vertexInputInfo);
    pipelineInfo.setPInputAssemblyState(&inputAssembly);
    pipelineInfo.setPViewportState(&viewportState);
    pipelineInfo.setPRasterizationState(&rasterizer);
    pipelineInfo.setPMultisampleState(&multisampling);
    pipelineInfo.setPDepthStencilState(&depthStencil);
    pipelineInfo.setPColorBlendState(&colorBlending);
    pipelineInfo.setLayout(m_scanline_pipeline_layout);
    pipelineInfo.setRenderPass(m_scanline_renderpass);
    pipelineInfo.setSubpass(0);
    pipelineInfo.setBasePipelineHandle(VK_NULL_HANDLE);

    if (m_device.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_scanline_pipeline) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create scanline pipeline!");
    }

    // Done with the temporary spv shader modules.
    m_device.destroyShaderModule(fragShaderModule);
    m_device.destroyShaderModule(vertShaderModule);
}

void Graphics::Rasterize() {
    vk::DeviceSize offset{ 0 };

    std::array<vk::ClearValue, 2> clearValues;
    vk::ClearColorValue colorVal;
    colorVal.setFloat32({ 0.0f,0,0,1 });
    clearValues[0].setColor(colorVal);
    clearValues[1].setDepthStencil(vk::ClearDepthStencilValue({ 1.0f, 0 }));

    vk::RenderPassBeginInfo _i;
    _i.setClearValueCount(2);
    _i.setPClearValues(clearValues.data());
    _i.setRenderPass(m_scanline_renderpass);
    _i.setFramebuffer(m_scanline_framebuffer);
    _i.renderArea = { {0, 0}, window_size };
    m_cmd_buffer.beginRenderPass(&_i, vk::SubpassContents::eInline);

    m_cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_scanline_pipeline);
    //vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
    //                        m_scanlinePipelineLayout, 0, 1, &m_scDesc.descSet, 0, nullptr);
    m_cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_scanline_pipeline_layout, 0, 1,
        &m_scanline_desc.descSet, 0, nullptr);

    for (const ObjInst& inst : m_objInst) {
        auto& object = m_objData[inst.objIndex];

        // Information pushed at each draw call
        PushConstantRaster pcRaster{
            inst.transform,      // Object's instance transform.
            {0.5f, 2.5f, 3.0f},  // light position;  Should not be hard-coded here!
            inst.objIndex,       // instance Id
            2.5f,                 // light intensity;  Should not be hard-coded here!
            1,                   //Light Type ?
            0.2f                 //Ambient light 
        };

        pcRaster.objIndex = inst.objIndex;  // Telling which object is drawn
        pcRaster.modelMatrix = inst.transform;

        m_cmd_buffer.pushConstants(m_scanline_pipeline_layout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(PushConstantRaster), &pcRaster);

        m_cmd_buffer.bindVertexBuffers(0, 1, &object.vertexBuffer.buffer, &offset);
        m_cmd_buffer.bindIndexBuffer(object.indexBuffer.buffer, 0, vk::IndexType::eUint32);
        m_cmd_buffer.drawIndexed(object.nbIndices, 1, 0, 0, 0);
    }
    m_cmd_buffer.endRenderPass();
}

void Graphics::TeardownScanlineResources() {
    for (auto t : m_objText) t.destroy(m_device);
    for (auto ob : m_objData) ob.destroy(m_device);

    m_device.destroyPipelineLayout(m_scanline_pipeline_layout);
    m_device.destroyPipeline(m_scanline_pipeline);
    m_scanline_desc.destroy(m_device);
    m_objDescriptionBW.destroy(m_device);
    m_matrixBW.destroy(m_device);
    m_device.destroyRenderPass(m_scanline_renderpass);
    m_device.destroyFramebuffer(m_scanline_framebuffer);
    m_sc_img_buffer.destroy(m_device);
}
