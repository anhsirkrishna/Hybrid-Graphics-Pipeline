#include "Graphics.h"

#include "LightingPass.h"
#include "TileMaxPass.h"

void LightingPass::SetupBuffer() {
    m_buffer.CreateTextureSampler();
    m_buffer.TransitionImageLayout(vk::ImageLayout::eGeneral);

    m_velocity_buffer.CreateTextureSampler();
    m_velocity_buffer.TransitionImageLayout(vk::ImageLayout::eGeneral);
}

void LightingPass::SetupAttachments() {
    vk::AttachmentDescription color_attachment;
    color_attachment.setFormat(vk::Format::eR32G32B32A32Sfloat);
    color_attachment.setSamples(vk::SampleCountFlagBits::e1);
    color_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    color_attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    color_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    color_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    color_attachment.setInitialLayout(vk::ImageLayout::eGeneral);
    color_attachment.setFinalLayout(vk::ImageLayout::eGeneral);
    m_framebuffer_attachments.push_back(color_attachment);

    vk::AttachmentDescription color_velo_attachment;
    color_velo_attachment.setFormat(vk::Format::eR32G32B32A32Sfloat);
    color_velo_attachment.setSamples(vk::SampleCountFlagBits::e1);
    color_velo_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    color_velo_attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    color_velo_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    color_velo_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    color_velo_attachment.setInitialLayout(vk::ImageLayout::eGeneral);
    color_velo_attachment.setFinalLayout(vk::ImageLayout::eGeneral);
    m_framebuffer_attachments.push_back(color_velo_attachment);

    vk::AttachmentDescription depth_attachment;
    depth_attachment.setFormat(p_gfx->GetDepthBuffer().GetFormat());
    depth_attachment.setSamples(vk::SampleCountFlagBits::e1);
    depth_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    depth_attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    depth_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depth_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depth_attachment.setInitialLayout(vk::ImageLayout::eUndefined);
    depth_attachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
    m_framebuffer_attachments.push_back(depth_attachment);
}

void LightingPass::SetupRenderPass() {
    vk::AttachmentReference color_attachment_ref;
    color_attachment_ref.setAttachment(0);
    color_attachment_ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentReference velo_attachment_ref;
    velo_attachment_ref.setAttachment(1);
    velo_attachment_ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    std::array<vk::AttachmentReference, 2> color_attachment_refs{ color_attachment_ref, velo_attachment_ref };

    vk::AttachmentReference depth_attachment_ref;
    depth_attachment_ref.setAttachment(2);
    depth_attachment_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::SubpassDescription subpass;
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpass.setColorAttachmentCount(2);
    subpass.setPColorAttachments(color_attachment_refs.data());
    subpass.setPDepthStencilAttachment(&depth_attachment_ref);

    vk::SubpassDependency subpass_dependency;
    subpass_dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
    subpass_dependency.setDstSubpass(0);
    subpass_dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests);
    subpass_dependency.setSrcAccessMask(vk::AccessFlagBits::eNoneKHR);
    subpass_dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests);
    subpass_dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
        vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    vk::RenderPassCreateInfo render_pass_info;
    render_pass_info.setAttachmentCount(static_cast<uint32_t>(m_framebuffer_attachments.size()));
    render_pass_info.setPAttachments(m_framebuffer_attachments.data());
    render_pass_info.setSubpassCount(1);
    render_pass_info.setPSubpasses(&subpass);
    render_pass_info.setDependencyCount(1);
    render_pass_info.setPDependencies(&subpass_dependency);

    if (p_gfx->GetDeviceRef().createRenderPass(
        &render_pass_info, nullptr, &m_render_pass) 
        != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create scanline render pass!");
    }
}

void LightingPass::SetupFramebuffer() {
    std::vector<vk::ImageView> attachments = { 
        m_buffer.GetImageView(), 
        m_velocity_buffer.GetImageView(),
        p_gfx->GetDepthBuffer().GetImageView()};

    vk::FramebufferCreateInfo info;
    info.setRenderPass(m_render_pass);
    info.setAttachmentCount(attachments.size());
    info.setPAttachments(attachments.data());
    info.setWidth(p_gfx->GetWindowSize().x);
    info.setHeight(p_gfx->GetWindowSize().y);
    info.setLayers(1);

    if (p_gfx->GetDeviceRef().createFramebuffer(&info, nullptr, &m_framebuffer) 
        != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create scanline render pass!");
    }
}

void LightingPass::SetupDescriptor() {
    auto texture_count = static_cast<uint32_t>(p_gfx->m_objText.size());

    auto device_ref = p_gfx->GetDeviceRef();
    m_descriptor.setBindings(device_ref, {
            {ScBindings::eMatrices, vk::DescriptorType::eUniformBuffer, 1,
                vk::ShaderStageFlagBits::eVertex },
            {ScBindings::eObjDescs, vk::DescriptorType::eStorageBuffer, 1,
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment },
            {ScBindings::eTextures, vk::DescriptorType::eCombinedImageSampler, texture_count,
                vk::ShaderStageFlagBits::eFragment }
        });

    m_descriptor.write(device_ref, ScBindings::eMatrices, p_gfx->m_matrixBW.buffer);
    m_descriptor.write(device_ref, ScBindings::eObjDescs, p_gfx->m_objDescriptionBW.buffer);
    m_descriptor.write(device_ref, ScBindings::eTextures, p_gfx->m_objText);
}

void LightingPass::SetupPipeline() {
    vk::PushConstantRange pushConstantRanges = {
       vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 
       0, 
       sizeof(PushConstantRaster) };

    // Creating the Pipeline Layout
    vk::PipelineLayoutCreateInfo createInfo;
    createInfo.setSetLayoutCount(1);
    createInfo.setPSetLayouts(&m_descriptor.descSetLayout);
    createInfo.setPushConstantRangeCount(1);
    createInfo.setPPushConstantRanges(&pushConstantRanges);
    m_pipeline_layout = p_gfx->GetDeviceRef().createPipelineLayout(createInfo);


    vk::ShaderModule vertShaderModule = 
        p_gfx->CreateShaderModule(LoadFileIntoString("spv/scanline.vert.spv"));
    vk::ShaderModule fragShaderModule = 
        p_gfx->CreateShaderModule(LoadFileIntoString("spv/scanline.frag.spv"));

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
    viewport.setWidth((float)p_gfx->GetWindowSize().x);
    viewport.setHeight((float)p_gfx->GetWindowSize().y);
    viewport.setMinDepth(0.0f);
    viewport.setMaxDepth(1.0f);

    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 });
    scissor.setExtent(p_gfx->GetWindowExtent());

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

    vk::PipelineColorBlendAttachmentState velo_blend_attachment;
    velo_blend_attachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    velo_blend_attachment.setBlendEnable(VK_FALSE);

    std::array< vk::PipelineColorBlendAttachmentState, 2> blend_attachments{ colorBlendAttachment, velo_blend_attachment };

    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.setLogicOpEnable(VK_FALSE);
    colorBlending.setLogicOp(vk::LogicOp::eCopy);
    colorBlending.setAttachmentCount(2);
    colorBlending.setPAttachments(blend_attachments.data());
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
    pipelineInfo.setLayout(m_pipeline_layout);
    pipelineInfo.setRenderPass(m_render_pass);
    pipelineInfo.setSubpass(0);
    pipelineInfo.setBasePipelineHandle(VK_NULL_HANDLE);

    if (p_gfx->GetDeviceRef().createGraphicsPipelines(
        VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create scanline pipeline!");
    }

    // Done with the temporary spv shader modules.
    p_gfx->GetDeviceRef().destroyShaderModule(fragShaderModule);
    p_gfx->GetDeviceRef().destroyShaderModule(vertShaderModule);

}

LightingPass::LightingPass(Graphics* _p_gfx) : RenderPass(_p_gfx),
    buffer_mip_levels(std::floor(std::log2(std::max(p_gfx->GetWindowSize().x, p_gfx->GetWindowSize().y))) + 1),
    m_buffer(p_gfx->GetWindowSize().x, p_gfx->GetWindowSize().y,
        vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        buffer_mip_levels, p_gfx),
    m_velocity_buffer(p_gfx->GetWindowSize().x, p_gfx->GetWindowSize().y,
        vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        1, p_gfx) {

    SetupBuffer();
    SetupAttachments();
    SetupRenderPass();
    SetupFramebuffer();
    SetupDescriptor();
    SetupPipeline();

    m_push_consts.exposure_time = 1.0f / 144;
    m_push_consts.exposure_time = 100.0f;
    m_push_consts.window_size = p_gfx->GetWindowSize();
    m_push_consts.tile_size = TileMaxPass::tile_size;
}

LightingPass::~LightingPass() {
    p_gfx->GetDeviceRef().destroyPipelineLayout(m_pipeline_layout);
    p_gfx->GetDeviceRef().destroyPipeline(m_pipeline);
    p_gfx->GetDeviceRef().destroyFramebuffer(m_framebuffer);
    p_gfx->GetDeviceRef().destroyRenderPass(m_render_pass);
    
    m_descriptor.destroy(p_gfx->GetDeviceRef());
    m_buffer.destroy(p_gfx->GetDeviceRef());
    m_velocity_buffer.destroy(p_gfx->GetDeviceRef());
}

void LightingPass::Setup() {
}

void LightingPass::Render() {
    vk::DeviceSize offset{ 0 };

    std::array<vk::ClearValue, 3> clearValues;
    vk::ClearColorValue colorVal;
    colorVal.setFloat32({ 0.0f,0,0,1 });
    clearValues[0].setColor(colorVal);
    clearValues[1].setColor(colorVal);
    clearValues[2].setDepthStencil(vk::ClearDepthStencilValue({ 1.0f, 0 }));

    vk::RenderPassBeginInfo _i;
    _i.setClearValueCount(3);
    _i.setPClearValues(clearValues.data());
    _i.setRenderPass(m_render_pass);
    _i.setFramebuffer(m_framebuffer);
    _i.renderArea = { {0, 0}, p_gfx->GetWindowExtent()};

    auto gfx_command_buffer = p_gfx->GetCommandBuffer();
    gfx_command_buffer.beginRenderPass(&_i, vk::SubpassContents::eInline);

    gfx_command_buffer.bindPipeline(
        vk::PipelineBindPoint::eGraphics, 
        m_pipeline);

    gfx_command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        m_pipeline_layout, 0, 1,
        &m_descriptor.descSet, 0, nullptr);


    m_push_consts.frame_rate = 1.0f/ImGui::GetIO().Framerate;
    for (const ObjInst& inst : p_gfx->m_objInst) {
        auto& object = p_gfx->m_objData[inst.objIndex];

        m_push_consts.modelMatrix = inst.transform;
        m_push_consts.objIndex = inst.objIndex;

        gfx_command_buffer.pushConstants(m_pipeline_layout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(PushConstantRaster), &m_push_consts);

        gfx_command_buffer.bindVertexBuffers(0, 1, &object.vertexBuffer.buffer, &offset);
        gfx_command_buffer.bindIndexBuffer(object.indexBuffer.buffer, 0, vk::IndexType::eUint32);
        gfx_command_buffer.drawIndexed(object.nbIndices, 1, 0, 0, 0);
    }
    gfx_command_buffer.endRenderPass();
}

void LightingPass::Teardown() {
    
}

void LightingPass::DrawGUI() {
    ImGui::SliderFloat("Exposure time : ", &m_push_consts.exposure_time,
        1.0f, 100.0f);
    ImGui::Text("frame rate : %f", m_push_consts.frame_rate);
}

const ImageWrap& LightingPass::GetBufferRef() const {
    return m_buffer;
}

const ImageWrap& LightingPass::GetVeloDepthBufferRef() const {
    return m_velocity_buffer;
}
