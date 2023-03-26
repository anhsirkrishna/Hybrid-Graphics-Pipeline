#include "Graphics.h"

#include "BufferDebugDraw.h"
#include "TileMaxPass.h"

void BufferDebugDraw::SetupDescriptor() {
    m_descriptor.setBindings(p_gfx->GetDeviceRef(), {
            {0, vk::DescriptorType::eCombinedImageSampler , 1, vk::ShaderStageFlagBits::eFragment}
        }, { vk::DescriptorBindingFlagBits::eUpdateAfterBind });
}

void BufferDebugDraw::SetupRenderPass() {
    std::array<vk::AttachmentDescription, 2> attachments{};
    // Color attachment
    attachments[0].setFormat(vk::Format::eB8G8R8A8Unorm);
    attachments[0].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[0].setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
    attachments[0].setSamples(vk::SampleCountFlagBits::e1);

    // Depth attachment
    
    attachments[1].setFormat(p_gfx->GetDepthBuffer().GetFormat());
    attachments[1].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[1].setStencilLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[1].setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
    attachments[1].setSamples(vk::SampleCountFlagBits::e1);

    const vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
    const vk::AttachmentReference depthReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);


    std::array<vk::SubpassDependency, 1> subpassDependencies;
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

    m_render_pass = p_gfx->GetDeviceRef().createRenderPass(renderPassInfo);
}

void BufferDebugDraw::SetupFramebuffer() {
    std::array<vk::ImageView, 2> fbattachments;

    // Create frame buffers for every swap chain image
    VkFramebufferCreateInfo _ci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    vk::FramebufferCreateInfo fbcreateInfo;
    fbcreateInfo.setRenderPass(m_render_pass);
    fbcreateInfo.setWidth(p_gfx->GetWindowExtent().width);
    fbcreateInfo.setHeight(p_gfx->GetWindowExtent().height);
    fbcreateInfo.setLayers(1);
    fbcreateInfo.setAttachmentCount(2);
    fbcreateInfo.setPAttachments(fbattachments.data());

    // Each of the three swapchain images gets an associated frame
    // buffer, all sharing one depth buffer.
    auto image_views = p_gfx->GetSwapChainImageViews();
    for (uint32_t i = 0; i < image_views.size(); i++) {
        fbattachments[0] = image_views[i]; // A color attachment from the swap chain
        fbattachments[1] = p_gfx->GetDepthBuffer().GetImageView();       // A depth attachment
        m_framebuffers.push_back(p_gfx->GetDeviceRef().createFramebuffer(fbcreateInfo));
    }
}

void BufferDebugDraw::SetupPipeline() {
    vk::PushConstantRange pc_range = {
       vk::ShaderStageFlagBits::eFragment,
       0,
       sizeof(PushConstantDrawBuffer) };
    std::vector<vk::PushConstantRange> pc_ranges{ pc_range };

    // Creating the pipeline layout
    vk::PipelineLayoutCreateInfo createInfo;
    createInfo.setSetLayoutCount(1);
    createInfo.setPSetLayouts(&m_descriptor.descSetLayout);
    createInfo.setPushConstantRanges(pc_ranges);

    // What we can do now as a first pass:
    m_pipeline_layout = p_gfx->GetDeviceRef().createPipelineLayout(createInfo);

    ////////////////////////////////////////////
    // Create the shaders
    ////////////////////////////////////////////
    vk::ShaderModule vertShaderModule = p_gfx->CreateShaderModule(LoadFileIntoString("spv/BufferDebugDraw.vert.spv"));
    vk::ShaderModule fragShaderModule = p_gfx->CreateShaderModule(LoadFileIntoString("spv/BufferDebugDraw.frag.spv"));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eVertex,
        vertShaderModule, "main");

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eFragment,
        fragShaderModule, "main");

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // No geometry in this pipeline's draw.
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList);

    vk::Viewport viewport;
    viewport.setWidth(p_gfx->GetWindowSize().x);
    viewport.setHeight(p_gfx->GetWindowSize().y);
    viewport.setMaxDepth(1.0f);

    vk::Rect2D scissor({ 0, 0 }, p_gfx->GetWindowExtent());

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
    pipelineCreateInfo.setLayout(m_pipeline_layout);
    pipelineCreateInfo.setRenderPass(m_render_pass);
    pipelineCreateInfo.setSubpass(0);
    pipelineCreateInfo.setBasePipelineHandle(nullptr);

    vk::Result   result;
    vk::Pipeline pipeline;
    std::tie(result, pipeline) = p_gfx->GetDeviceRef().createGraphicsPipeline(VK_NULL_HANDLE, pipelineCreateInfo);


    switch (result)
    {
    case vk::Result::eSuccess:
        m_pipeline = pipeline;
        break;
    default: assert(false);  // should never happen
    }

    p_gfx->GetDeviceRef().destroyShaderModule(fragShaderModule);
    p_gfx->GetDeviceRef().destroyShaderModule(vertShaderModule);
}

BufferDebugDraw::BufferDebugDraw(Graphics* _p_gfx, RenderPass* p_prev_pass) :
    RenderPass(_p_gfx), draw_buffer(DrawBuffer::DISABLE) {
    SetupRenderPass();
    SetupFramebuffer();
    SetupDescriptor();
    SetupPipeline();
    m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
    m_push_consts.tile_size = TileMaxPass::tile_size;
    m_push_consts.alignmentTest = 1234;
}

BufferDebugDraw::~BufferDebugDraw() {
    p_gfx->GetDeviceRef().destroyPipelineLayout(m_pipeline_layout);
    p_gfx->GetDeviceRef().destroyPipeline(m_pipeline);

    for (auto& framebuffer : m_framebuffers)
        p_gfx->GetDeviceRef().destroyFramebuffer(framebuffer);
    p_gfx->GetDeviceRef().destroyRenderPass(m_render_pass);

    m_descriptor.destroy(p_gfx->GetDeviceRef());
}

void BufferDebugDraw::Setup() {
}

void BufferDebugDraw::Render() {
    if (draw_buffer == DrawBuffer::DISABLE)
        return;

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color = vk::ClearColorValue(std::array<float, 4>({ { 1.0f, 1.0f, 1.0f, 1.0f } }));
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderPassBeginInfo renderPassBeginInfo(
        m_render_pass, m_framebuffers[p_gfx->GetCurrentSwapchainIndex()],
        vk::Rect2D(vk::Offset2D(0, 0), p_gfx->GetWindowExtent()), clearValues);

    p_gfx->GetCommandBuffer().beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    {   // extra indent for renderpass commands

        p_gfx->GetCommandBuffer().bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);

        p_gfx->GetCommandBuffer().bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            m_pipeline_layout, 0, 1, &m_descriptor.descSet, 0, nullptr);

        p_gfx->GetCommandBuffer().pushConstants(m_pipeline_layout,
            vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(PushConstantDrawBuffer),
            &m_push_consts);
        // Weird! This draws 3 vertices but with no vertices/triangles buffers bound in.
        // Hint: The vertex shader fabricates vertices from gl_VertexIndex
        p_gfx->GetCommandBuffer().draw(3, 1, 0, 0);

#ifdef GUI
        ImGui::Render();  // Rendering UI
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_gfx->GetCommandBuffer());
#endif
    }
    p_gfx->GetCommandBuffer().endRenderPass();
}

void BufferDebugDraw::Teardown() {

}

void BufferDebugDraw::SetDrawBuffer(vk::DescriptorImageInfo& draw_descriptor) {
    m_descriptor.write(p_gfx->GetDeviceRef(), 0, draw_descriptor);
}

void BufferDebugDraw::SetVeloDepthBuffer(const ImageWrap& draw_buffer) {
    velo_depth_buffer_desc = draw_buffer.Descriptor();
}

void BufferDebugDraw::SetTileMaxBuffer(const ImageWrap& draw_buffer) {
    tile_max_buffer_desc = draw_buffer.Descriptor();
}

void BufferDebugDraw::SetNeighbourMaxBuffer(const ImageWrap& draw_buffer) {
    neighbour_max_buffer_desc = draw_buffer.Descriptor();
}

void BufferDebugDraw::SetPreDOFBuffer(const ImageWrap& draw_buffer) {
    pre_dof_buffer_desc = draw_buffer.Descriptor();
}

void BufferDebugDraw::SetPreDOFParamsBuffer(const ImageWrap& draw_buffer) {
    pre_dof_params_buffer_desc = draw_buffer.Descriptor();
}

void BufferDebugDraw::SetDOFBGBuffer(const ImageWrap& draw_buffer) {
    dof_bg_buffer_desc = draw_buffer.Descriptor();
}

void BufferDebugDraw::SetDOFFGBuffer(const ImageWrap& draw_buffer) {
    dof_fg_buffer_desc = draw_buffer.Descriptor();
}

void BufferDebugDraw::SetDOFBuffer(const ImageWrap& draw_buffer) {
    dof_buffer_desc = draw_buffer.Descriptor();
}

void BufferDebugDraw::SetMedianBuffer(const ImageWrap& draw_buffer) {
    median_buffer_desc = draw_buffer.Descriptor();
}

void BufferDebugDraw::DrawGUI() {
    if (ImGui::BeginMenu("Draw FBOs")) {
        if (ImGui::MenuItem("Disable", "", draw_buffer == DrawBuffer::DISABLE)) {
            draw_buffer = DrawBuffer::DISABLE;
            p_gfx->EnablePostProcess();
        }
        if (ImGui::MenuItem("Draw Velocity Buffer", "", draw_buffer == DrawBuffer::VELOCITY)) {
            draw_buffer = DrawBuffer::VELOCITY;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(velo_depth_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw Depth Buffer", "", draw_buffer == DrawBuffer::DEPTH)) {
            draw_buffer = DrawBuffer::DEPTH;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(velo_depth_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw TileMax CoC Buffer", "", draw_buffer == DrawBuffer::TILEMAX_COC)) {
            draw_buffer = DrawBuffer::TILEMAX_COC;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(tile_max_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw TileMax Velo Buffer", "", draw_buffer == DrawBuffer::TILEMAX_VELO)) {
            draw_buffer = DrawBuffer::TILEMAX_VELO;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(tile_max_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw NeighbourMax COC Buffer", "", draw_buffer == DrawBuffer::NEIGHBOURMAX_COC)) {
            draw_buffer = DrawBuffer::NEIGHBOURMAX_COC;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(neighbour_max_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw NeighbourMaxVelo Buffer", "", draw_buffer == DrawBuffer::NEIGHBOURMAX_VELO)) {
            draw_buffer = DrawBuffer::NEIGHBOURMAX_VELO;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(neighbour_max_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw PreDOF Buffer", "", draw_buffer == DrawBuffer::PRE_DOF)) {
            draw_buffer = DrawBuffer::PRE_DOF;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(pre_dof_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw PreDOF Params CoC Buffer", "", draw_buffer == DrawBuffer::PRE_DOF_COC)) {
            draw_buffer = DrawBuffer::PRE_DOF_COC;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(pre_dof_params_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw PreDOF Params BG Buffer", "", draw_buffer == DrawBuffer::PRE_DOF_BG)) {
            draw_buffer = DrawBuffer::PRE_DOF_BG;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(pre_dof_params_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw PreDOF Params FG Buffer", "", draw_buffer == DrawBuffer::PRE_DOF_FG)) {
            draw_buffer = DrawBuffer::PRE_DOF_FG;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(pre_dof_params_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw DOF BG Buffer", "", draw_buffer == DrawBuffer::DOF_BG)) {
            draw_buffer = DrawBuffer::DOF_BG;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(dof_bg_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw DOF FG Buffer", "", draw_buffer == DrawBuffer::DOF_FG)) {
            draw_buffer = DrawBuffer::DOF_FG;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(dof_fg_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw DOF Buffer", "", draw_buffer == DrawBuffer::DOF)) {
            draw_buffer = DrawBuffer::DOF;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(dof_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw DOF Alpha Buffer", "", draw_buffer == DrawBuffer::DOF_ALPHA)) {
            draw_buffer = DrawBuffer::DOF_ALPHA;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(dof_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        if (ImGui::MenuItem("Draw Median Buffer", "", draw_buffer == DrawBuffer::MEDIAN)) {
            draw_buffer = DrawBuffer::MEDIAN;
            p_gfx->DisablePostProcess();
            SetDrawBuffer(median_buffer_desc);
            m_push_consts.draw_buffer = static_cast<int>(draw_buffer);
        }
        ImGui::EndMenu();
    }
}
