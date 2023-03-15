#include "Graphics.h"

#include "NeighbourMax.h"
#include "TileMaxPass.h"

void NeighbourMax::SetupBuffer() {
	m_buffer.TransitionImageLayout(vk::ImageLayout::eGeneral);
}

void NeighbourMax::SetupDescriptor() {
    m_descriptor.setBindings(p_gfx->GetDeviceRef(), {
        {0, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute},
        {1, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute} });
}

void NeighbourMax::SetupPipeline() {
    vk::PushConstantRange pc_info;
    pc_info.setStageFlags(vk::ShaderStageFlagBits::eCompute);
    pc_info.setOffset(0);
    pc_info.setSize(sizeof(PushConstantNeighbourMax));

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
        p_gfx->CreateShaderModule(LoadFileIntoString("spv/NeighbourMax.comp.spv"))
    );
    shader_stage.setPName("main");
    cp_create_info.stage = shader_stage;


    if (p_gfx->GetDeviceRef().createComputePipelines({}, 1, &cp_create_info, nullptr, &m_pipeline)
        != vk::Result::eSuccess) {
        printf("Failed to create Compute pipeline for DoF \n");
    };
    p_gfx->GetDeviceRef().destroyShaderModule(cp_create_info.stage.module, nullptr);
}

NeighbourMax::NeighbourMax(Graphics* _p_gfx, RenderPass* _p_prev_pass) : 
    RenderPass(_p_gfx, _p_prev_pass),
    m_buffer(p_gfx->GetWindowSize().x / TileMaxPass::tile_size, 
             p_gfx->GetWindowSize().y / TileMaxPass::tile_size,
    vk::Format::eR32G32B32A32Sfloat,
    vk::ImageUsageFlagBits::eTransferDst |
    vk::ImageUsageFlagBits::eStorage |
    vk::ImageUsageFlagBits::eTransferSrc |
    vk::ImageUsageFlagBits::eColorAttachment,
    vk::ImageAspectFlagBits::eColor,
    vk::MemoryPropertyFlagBits::eDeviceLocal,
    1, p_gfx), m_push_consts() {
    SetupBuffer();
    SetupDescriptor();

    m_push_consts.tile_size = TileMaxPass::tile_size;
    m_push_consts.alignmentTest = 1234;
}

NeighbourMax::~NeighbourMax() {
    p_gfx->GetDeviceRef().destroyPipelineLayout(m_pipeline_layout);
    p_gfx->GetDeviceRef().destroyPipeline(m_pipeline);

    m_descriptor.destroy(p_gfx->GetDeviceRef());
    m_buffer.destroy(p_gfx->GetDeviceRef());
}

void NeighbourMax::Setup() {
    m_descriptor.write(p_gfx->GetDeviceRef(), 0,
        static_cast<TileMaxPass*>(p_prev_pass)->GetBuffer().Descriptor());
    m_descriptor.write(p_gfx->GetDeviceRef(), 1, m_buffer.Descriptor());
    SetupPipeline();
}
void NeighbourMax::Render() {
    TileMaxPass* p_prev_tilemax_pass = static_cast<TileMaxPass*>(p_prev_pass);
    vk::ImageSubresourceRange range;
    range.setAspectMask(vk::ImageAspectFlagBits::eColor);
    range.setBaseMipLevel(0);
    range.setLayerCount(1);
    range.setBaseArrayLayer(0);
    range.setLevelCount(1);

    vk::ImageMemoryBarrier img_mem_barrier;
    img_mem_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
    img_mem_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    img_mem_barrier.setImage(p_prev_tilemax_pass->GetBuffer().GetImage());
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
        (p_gfx->GetWindowSize().x / TileMaxPass::tile_size),
        (p_gfx->GetWindowSize().y / TileMaxPass::tile_size), 1);

    img_mem_barrier.setImage(m_buffer.GetImage());
    p_gfx->GetCommandBuffer().pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlagBits::eDeviceGroup,
        0, nullptr, 0, nullptr, 1, &img_mem_barrier);
}

void NeighbourMax::Teardown()
{
}

void NeighbourMax::DrawGUI() {
}

const ImageWrap& NeighbourMax::GetBuffer() const {
    return m_buffer;
}

