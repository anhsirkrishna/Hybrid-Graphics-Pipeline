#include "ImageWrap.h"
#include "DescriptorWrap.h"
#include "Graphics.h"
#include "Camera.h"

#include "DOFPass.h"
#include "PreDOFPass.h"
#include "TileMaxPass.h"

void DOFPass::SetupBuffer() {
    m_buffer_bg.CreateTextureSampler();
    m_buffer_bg.TransitionImageLayout(vk::ImageLayout::eGeneral);

    m_buffer_fg.CreateTextureSampler();
    m_buffer_fg.TransitionImageLayout(vk::ImageLayout::eGeneral);

    m_buffer.CreateTextureSampler();
    m_buffer.TransitionImageLayout(vk::ImageLayout::eGeneral);
}

void DOFPass::WriteToDescriptor(glm::uint index, const vk::DescriptorImageInfo img_desc_info) {
    m_descriptor.write(p_gfx->GetDeviceRef(), index, img_desc_info);
}

void DOFPass::SetNeighbourMaxBufferDesc(const ImageWrap& buffer) {
    neighbour_max_buffer_desc = buffer.Descriptor();
}

const PushConstantDoF& DOFPass::GetDOFParams() {
    return m_push_consts;
}

const ImageWrap& DOFPass::GetBGBuffer() const {
    return m_buffer_bg;
}

const ImageWrap& DOFPass::GetFGBuffer() const {
    return m_buffer_fg;
}

const ImageWrap& DOFPass::GetBuffer() const {
    return m_buffer;
}

void DOFPass::DrawGUI() {
    ImGui::Checkbox("DOF Enabled", &enabled);
    ImGui::SliderFloat("CoC sample scale : ", &m_push_consts.coc_sample_scale,
        0.0f, 1000.0f);
    ImGui::SliderFloat("Focal Distance : ", &m_push_consts.focal_distance,
        0.1f, 2.0f);
    ImGui::SliderFloat("Focal length : ", &m_push_consts.focal_length,
        0.01f, 0.09f);
    ImGui::SliderFloat("Lens Diameter : ", &m_push_consts.lens_diameter,
        0.01f, 0.2f);
    ImGui::SliderFloat("DOF soft z extent", &m_push_consts.soft_z_extent,
        0.01f, 0.05f);

    //Set the position of the camera to demonstrate DOF
    if (ImGui::Button("Set DOF Eye Pos")) {
        p_gfx->GetCamera()->SetEyePos(vec3(1.9739350, 1.014416, -1.624937));
        p_gfx->GetCamera()->SetSpin(-44);
        p_gfx->GetCamera()->SetTilt(20);
    }

    //Set the position of the camera to demonstrate DOF
    if (ImGui::Button("Set DOF Eye Pos 2")) {
        p_gfx->GetCamera()->SetEyePos(vec3(2.009752, 0.771944, -1.423020));
        p_gfx->GetCamera()->SetSpin(-44);
        p_gfx->GetCamera()->SetTilt(20);
    }
}

void DOFPass::SetupDescriptor() {
    m_descriptor.setBindings(p_gfx->GetDeviceRef(), {
        {0, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute},
        {1, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute},
        {2, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute},
        {3, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute},
        {4, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute},
        {5, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute} });
}

void DOFPass::SetupPipeline() {
    vk::PushConstantRange pc_info;
    pc_info.setStageFlags(vk::ShaderStageFlagBits::eCompute);
    pc_info.setOffset(0);
    pc_info.setSize(sizeof(PushConstantDoF));

    vk::PipelineLayoutCreateInfo pl_create_info;
    pl_create_info.setSetLayoutCount(1);
    pl_create_info.setPSetLayouts(&m_descriptor.descSetLayout);
    pl_create_info.setPushConstantRangeCount(1);
    pl_create_info.setPPushConstantRanges(&pc_info);
    m_pipeline_layout = p_gfx->GetDeviceRef().createPipelineLayout(pl_create_info);

    vk::ComputePipelineCreateInfo cp_create_info;
    cp_create_info.setLayout(m_pipeline_layout);

    vk::PipelineShaderStageCreateInfo shader_stage;
    shader_stage.setStage(vk::ShaderStageFlagBits::eCompute);
    shader_stage.setModule(
        p_gfx->CreateShaderModule(LoadFileIntoString("spv/DOF.comp.spv"))
    );    
    shader_stage.setPName("main");
    cp_create_info.stage = shader_stage;

    
    if (p_gfx->GetDeviceRef().createComputePipelines({}, 1, &cp_create_info, nullptr, &m_pipeline) 
        != vk::Result::eSuccess) {
        printf("Failed to create Compute pipeline for DoF \n");
    };
    p_gfx->GetDeviceRef().destroyShaderModule(cp_create_info.stage.module, nullptr);
}

DOFPass::DOFPass(Graphics* _p_gfx, RenderPass* _p_prev_pass) : RenderPass(_p_gfx, _p_prev_pass),
    m_buffer_bg(p_gfx->GetWindowSize().x/2, p_gfx->GetWindowSize().y/2,
	            vk::Format::eR32G32B32A32Sfloat, 
                vk::ImageUsageFlagBits::eTransferDst |
                vk::ImageUsageFlagBits::eSampled |
                vk::ImageUsageFlagBits::eStorage |
                vk::ImageUsageFlagBits::eTransferSrc |
                vk::ImageUsageFlagBits::eColorAttachment, 
                vk::ImageAspectFlagBits::eColor, 
                vk::MemoryPropertyFlagBits::eDeviceLocal, 
                1, p_gfx), 
    m_buffer_fg(p_gfx->GetWindowSize().x/2, p_gfx->GetWindowSize().y/2,
                vk::Format::eR32G32B32A32Sfloat,
                vk::ImageUsageFlagBits::eTransferDst |
                vk::ImageUsageFlagBits::eSampled |
                vk::ImageUsageFlagBits::eStorage |
                vk::ImageUsageFlagBits::eTransferSrc |
                vk::ImageUsageFlagBits::eColorAttachment,
                vk::ImageAspectFlagBits::eColor,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                1, p_gfx),
    m_buffer(p_gfx->GetWindowSize().x / 2, p_gfx->GetWindowSize().y / 2,
        vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        1, p_gfx),
    m_push_consts(), enabled(true) {
    m_push_consts.lens_diameter = 0.035f;
    m_push_consts.focal_length = 0.05f;
    m_push_consts.focal_distance = 1.0f;
    m_push_consts.coc_sample_scale = 800.0f;
    m_push_consts.soft_z_extent = 0.001f;
    m_push_consts.tile_size = TileMaxPass::tile_size;
    m_push_consts.alignmentTest = 1234;
    SetupBuffer();
    SetupDescriptor();
}

DOFPass::~DOFPass() {
    p_gfx->GetDeviceRef().destroyPipelineLayout(m_pipeline_layout);
    p_gfx->GetDeviceRef().destroyPipeline(m_pipeline);

    m_descriptor.destroy(p_gfx->GetDeviceRef());
    m_buffer_bg.destroy(p_gfx->GetDeviceRef());
    m_buffer_fg.destroy(p_gfx->GetDeviceRef());
    m_buffer.destroy(p_gfx->GetDeviceRef());
}

void DOFPass::Setup() {
    WriteToDescriptor(0, m_buffer_bg.Descriptor());
    WriteToDescriptor(1, m_buffer_fg.Descriptor());
    WriteToDescriptor(2,
        static_cast<PreDOFPass*>(p_prev_pass)->GetBuffer().Descriptor());
    WriteToDescriptor(3, 
        static_cast<PreDOFPass*>(p_prev_pass)->GetParamsBuffer().Descriptor());
    WriteToDescriptor(4, neighbour_max_buffer_desc);
    WriteToDescriptor(5, m_buffer.Descriptor());
    SetupPipeline();
}

void DOFPass::Render() {
    if (not enabled)
        return;

    PreDOFPass* p_pre_dof_pass = static_cast<PreDOFPass*>(p_prev_pass);
    vk::ImageSubresourceRange range;
    range.setAspectMask(vk::ImageAspectFlagBits::eColor);
    range.setBaseMipLevel(0);
    range.setLayerCount(1);
    range.setBaseArrayLayer(0);
    range.setLevelCount(1);

    vk::ImageMemoryBarrier img_mem_barrier;
    img_mem_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
    img_mem_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    img_mem_barrier.setImage(p_pre_dof_pass->GetBuffer().GetImage());
    img_mem_barrier.setOldLayout(vk::ImageLayout::eGeneral);
    img_mem_barrier.setNewLayout(vk::ImageLayout::eGeneral);
    img_mem_barrier.setSubresourceRange(range);

    p_gfx->GetCommandBuffer().pipelineBarrier(
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::DependencyFlagBits::eDeviceGroup,
        0, nullptr, 0, nullptr, 1, &img_mem_barrier);

    // Select the compute shader, and its descriptor set and push constant
    p_gfx->GetCommandBuffer().bindPipeline(
        vk::PipelineBindPoint::eCompute, 
        m_pipeline);
    p_gfx->GetCommandBuffer().bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        m_pipeline_layout, 0, 1,
        &m_descriptor.descSet, 0, nullptr);
    p_gfx->GetCommandBuffer().pushConstants(m_pipeline_layout,
        vk::ShaderStageFlagBits::eCompute, 0, 
        sizeof(PushConstantDoF),
        &m_push_consts);

    // This MUST match the shaders's line:
    //    layout(local_size_x=GROUP_SIZE, local_size_y=1, local_size_z=1) in;
    glm::uint group_size = 128;
    p_gfx->GetCommandBuffer().dispatch(
        ((p_gfx->GetWindowSize().x / 2) + group_size - 1) / group_size,
        (p_gfx->GetWindowSize().y / 2), 1);

    img_mem_barrier.setImage(m_buffer_bg.GetImage());
    p_gfx->GetCommandBuffer().pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlagBits::eDeviceGroup,
        0, nullptr, 0, nullptr, 1, &img_mem_barrier);

    img_mem_barrier.setImage(m_buffer_fg.GetImage());
    p_gfx->GetCommandBuffer().pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlagBits::eDeviceGroup,
        0, nullptr, 0, nullptr, 1, &img_mem_barrier);

    img_mem_barrier.setImage(m_buffer.GetImage());
    p_gfx->GetCommandBuffer().pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlagBits::eDeviceGroup,
        0, nullptr, 0, nullptr, 1, &img_mem_barrier);
}

void DOFPass::Teardown()
{
}
