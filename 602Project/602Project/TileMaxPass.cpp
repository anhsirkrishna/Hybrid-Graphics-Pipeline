#include "Graphics.h"

#include "TileMaxPass.h"
#include "LightingPass.h"

void TileMaxPass::SetupBuffer() {
	m_buffer.CreateTextureSampler();
	m_buffer.TransitionImageLayout(vk::ImageLayout::eGeneral);
}

void TileMaxPass::SetupDescriptor() {
    m_descriptor.setBindings(p_gfx->GetDeviceRef(), {
        {0, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute},
        {1, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute}});
}

void TileMaxPass::SetupPipeline() {
    vk::PushConstantRange pc_info;
    pc_info.setStageFlags(vk::ShaderStageFlagBits::eCompute);
    pc_info.setOffset(0);
    pc_info.setSize(sizeof(PushConstantTileMax));

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
        p_gfx->CreateShaderModule(LoadFileIntoString("spv/TileMax.comp.spv"))
    );
    shader_stage.setPName("main");
    cp_create_info.stage = shader_stage;


    if (p_gfx->GetDeviceRef().createComputePipelines({}, 1, &cp_create_info, nullptr, &m_pipeline)
        != vk::Result::eSuccess) {
        printf("Failed to create Compute pipeline for DoF \n");
    };
    p_gfx->GetDeviceRef().destroyShaderModule(cp_create_info.stage.module, nullptr);
}

TileMaxPass::TileMaxPass(Graphics* _p_gfx, RenderPass* _p_prev_pass) : RenderPass(_p_gfx, _p_prev_pass),
m_buffer(p_gfx->GetWindowSize().x/ tile_size, p_gfx->GetWindowSize().y / tile_size,
    vk::Format::eR32G32B32A32Sfloat,
    vk::ImageUsageFlagBits::eTransferDst |
    vk::ImageUsageFlagBits::eSampled |
    vk::ImageUsageFlagBits::eStorage |
    vk::ImageUsageFlagBits::eTransferSrc |
    vk::ImageUsageFlagBits::eColorAttachment,
    vk::ImageAspectFlagBits::eColor,
    vk::MemoryPropertyFlagBits::eDeviceLocal,
    1, p_gfx), m_push_consts() {
    SetupBuffer();
    SetupDescriptor();

    m_push_consts.tile_size = tile_size;
    m_push_consts.lens_diameter = 0.1f;
    m_push_consts.focal_length = 0.1f;
    m_push_consts.focal_distance = 1.0f;
    m_push_consts.alignmentTest = 1234;
}

TileMaxPass::~TileMaxPass() {
    p_gfx->GetDeviceRef().destroyPipelineLayout(m_pipeline_layout);
    p_gfx->GetDeviceRef().destroyPipeline(m_pipeline);

    m_descriptor.destroy(p_gfx->GetDeviceRef());
    m_buffer.destroy(p_gfx->GetDeviceRef());
}

void TileMaxPass::Setup() {
    m_descriptor.write(p_gfx->GetDeviceRef(), 0,
        static_cast<LightingPass*>(p_prev_pass)->GetVeloDepthBufferRef().Descriptor());
    m_descriptor.write(p_gfx->GetDeviceRef(), 1, m_buffer.Descriptor());
    SetupPipeline();
}

void TileMaxPass::Render() {
    LightingPass* p_prev_lighting_pass = static_cast<LightingPass*>(p_prev_pass);
    vk::ImageSubresourceRange range;
    range.setAspectMask(vk::ImageAspectFlagBits::eColor);
    range.setBaseMipLevel(0);
    range.setLayerCount(1);
    range.setBaseArrayLayer(0);
    range.setLevelCount(1);

    vk::ImageMemoryBarrier img_mem_barrier;
    img_mem_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
    img_mem_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    img_mem_barrier.setImage(p_prev_lighting_pass->GetBufferRef().GetImage());
    img_mem_barrier.setOldLayout(vk::ImageLayout::eGeneral);
    img_mem_barrier.setNewLayout(vk::ImageLayout::eGeneral);
    img_mem_barrier.setSubresourceRange(range);

    p_gfx->GetCommandBuffer().pipelineBarrier(
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::DependencyFlagBits::eDeviceGroup,
        0, nullptr, 0, nullptr, 1, &img_mem_barrier);

    p_gfx->GetCommandBuffer().bindPipeline(
        vk::PipelineBindPoint::eCompute,
        m_pipeline);
    p_gfx->GetCommandBuffer().bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        m_pipeline_layout, 0, 1,
        &m_descriptor.descSet, 0, nullptr);
    p_gfx->GetCommandBuffer().pushConstants(m_pipeline_layout,
        vk::ShaderStageFlagBits::eCompute, 0,
        sizeof(PushConstantTileMax),
        &m_push_consts);

    p_gfx->GetCommandBuffer().dispatch(
        (p_gfx->GetWindowSize().x / tile_size),
        (p_gfx->GetWindowSize().y / tile_size), 1);

    img_mem_barrier.setImage(m_buffer.GetImage());
    p_gfx->GetCommandBuffer().pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlagBits::eDeviceGroup,
        0, nullptr, 0, nullptr, 1, &img_mem_barrier);
}

void TileMaxPass::Teardown() {
}

void TileMaxPass::DrawGUI() {
    /*ImGui::SliderFloat("Focal Distance : ", &m_push_consts.focal_distance,
        0.1f, 5.0f);
    ImGui::SliderFloat("Focal length : ", &m_push_consts.focal_length,
        0.1f, 1.0f);
    ImGui::SliderFloat("Lens Diameter : ", &m_push_consts.lens_diameter,
        0.1f, 0.5f);*/
}

const ImageWrap& TileMaxPass::GetBuffer() const {
    return m_buffer;
}
