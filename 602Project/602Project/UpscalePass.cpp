#include "ImageWrap.h"
#include "DescriptorWrap.h"
#include "Graphics.h"

#include "UpscalePass.h"
#include "MedianPass.h"
#include "TileMaxPass.h"
#include "DOFPass.h"

void UpscalePass::SetupBuffer() {
	m_buffer.CreateTextureSampler();
	m_buffer.TransitionImageLayout(vk::ImageLayout::eGeneral);
}

void UpscalePass::SetupDescriptor() {
    m_descriptor.setBindings(p_gfx->GetDeviceRef(), {
        {0, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute},
        {1, vk::DescriptorType::eCombinedImageSampler, 1,
         vk::ShaderStageFlagBits::eCompute},
        {2, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute},
        {3, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute},
        {4, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eCompute} });
}

void UpscalePass::SetupPipeline() {
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
        p_gfx->CreateShaderModule(LoadFileIntoString("spv/Upscale.comp.spv"))
    );
    shader_stage.setPName("main");
    cp_create_info.stage = shader_stage;


    if (p_gfx->GetDeviceRef().createComputePipelines({}, 1, &cp_create_info, nullptr, &m_pipeline)
        != vk::Result::eSuccess) {
        printf("Failed to create Compute pipeline for DoF \n");
    };
    p_gfx->GetDeviceRef().destroyShaderModule(cp_create_info.stage.module, nullptr);
}

UpscalePass::UpscalePass(Graphics* _p_gfx, RenderPass* _p_prev_pass) : RenderPass(_p_gfx, _p_prev_pass),
m_buffer(p_gfx->GetWindowSize().x, p_gfx->GetWindowSize().y,
    vk::Format::eR32G32B32A32Sfloat,
    vk::ImageUsageFlagBits::eTransferDst |
    vk::ImageUsageFlagBits::eSampled |
    vk::ImageUsageFlagBits::eStorage |
    vk::ImageUsageFlagBits::eTransferSrc |
    vk::ImageUsageFlagBits::eColorAttachment,
    vk::ImageAspectFlagBits::eColor,
    vk::MemoryPropertyFlagBits::eDeviceLocal,
    1, p_gfx), m_push_consts(), enabled(true) {

    m_push_consts.lens_diameter = 0.035f;
    m_push_consts.focal_length = 0.05f;
    m_push_consts.focal_distance = 1.0f;
    m_push_consts.coc_sample_scale = 800.0f;
    m_push_consts.soft_z_extent = 0.35f;
    m_push_consts.tile_size = TileMaxPass::tile_size;
    m_push_consts.alignmentTest = 1234;

    SetupBuffer();
    SetupDescriptor();
}

UpscalePass::~UpscalePass() {
    p_gfx->GetDeviceRef().destroyPipelineLayout(m_pipeline_layout);
    p_gfx->GetDeviceRef().destroyPipeline(m_pipeline);

    m_descriptor.destroy(p_gfx->GetDeviceRef());
    m_buffer.destroy(p_gfx->GetDeviceRef());
}

void UpscalePass::Setup() {
    m_descriptor.write(p_gfx->GetDeviceRef(), 0, m_buffer.Descriptor());
    m_descriptor.write(p_gfx->GetDeviceRef(), 1,
        static_cast<MedianPass*>(p_prev_pass)->GetBuffer().Descriptor());
    m_descriptor.write(p_gfx->GetDeviceRef(), 2, fullres_buffer_desc);
    m_descriptor.write(p_gfx->GetDeviceRef(), 3, fullres_depth_buffer_desc);
    m_descriptor.write(p_gfx->GetDeviceRef(), 4, neighbour_max_buffer_desc);
    SetupPipeline();
}

void UpscalePass::Render() {
    if (not enabled)
        return;

    //Set the push consts based on DOF params
    m_push_consts.coc_sample_scale = p_dof_pass->GetDOFParams().coc_sample_scale;
    m_push_consts.focal_length = p_dof_pass->GetDOFParams().focal_length;
    m_push_consts.focal_distance = p_dof_pass->GetDOFParams().focal_distance;
    m_push_consts.lens_diameter = p_dof_pass->GetDOFParams().lens_diameter;

    MedianPass* p_median_pass = static_cast<MedianPass*>(p_prev_pass);
    vk::ImageSubresourceRange range;
    range.setAspectMask(vk::ImageAspectFlagBits::eColor);
    range.setBaseMipLevel(0);
    range.setLayerCount(1);
    range.setBaseArrayLayer(0);
    range.setLevelCount(1);

    vk::ImageMemoryBarrier img_mem_barrier;
    img_mem_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
    img_mem_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    img_mem_barrier.setImage(p_median_pass->GetBuffer().GetImage());
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
        sizeof(PushConstantDoF),
        &m_push_consts);

    p_gfx->GetCommandBuffer().dispatch(
        (p_gfx->GetWindowSize().x),
        (p_gfx->GetWindowSize().y), 1);

    img_mem_barrier.setImage(m_buffer.GetImage());
    p_gfx->GetCommandBuffer().pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlagBits::eDeviceGroup,
        0, nullptr, 0, nullptr, 1, &img_mem_barrier);
}

void UpscalePass::Teardown()
{
}

void UpscalePass::DrawGUI()
{
}

const ImageWrap& UpscalePass::GetBuffer() const {
    return m_buffer;
}

void UpscalePass::SetFullResBufferDesc(const ImageWrap& _buffer) {
    fullres_buffer_desc = _buffer.Descriptor();
}

void UpscalePass::SetFullResDepthBufferDesc(const ImageWrap& _buffer) {
    fullres_depth_buffer_desc = _buffer.Descriptor();
}

void UpscalePass::SetNeighbourBufferDesc(const ImageWrap& _buffer) {
    neighbour_max_buffer_desc = _buffer.Descriptor();
}

void UpscalePass::SetDOFPass(DOFPass* _p_dof_pass) {
    p_dof_pass = _p_dof_pass;
}
