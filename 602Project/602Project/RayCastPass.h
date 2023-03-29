#pragma once
#include "RenderPass.h"
#include "AccelerationWrap.h"

class LightingPass;
class DOFPass;

class RayCastPass : public RenderPass
{
private:
    ImageWrap m_buffer_bg;
    ImageWrap m_buffer_bg_prev;
    ImageWrap m_buffer_nd;
    ImageWrap m_buffer_nd_prev;
    vk::DescriptorImageInfo raymask_buffer_desc;
    void SetupBuffer();

    PushConstantRay m_push_consts;  // Push constant for ray tracer
    RaytracingBuilderKHR m_rt_builder;

    // Accelleration structure objects and functions
    void CreateRaytraceAS();

    DescriptorWrap m_descriptor;
    vk::DescriptorSetLayout lighting_pass_desc_layout;
    vk::DescriptorSet lighting_pass_desc_set;
    void SetupDescriptor();

    vk::PipelineLayout m_pipeline_layout;
    vk::Pipeline m_pipeline;
    void SetupPipeline();

    BufferWrap m_shaderBindingTableBW;
    vk::StridedDeviceAddressRegionKHR m_rgen_region;
    vk::StridedDeviceAddressRegionKHR m_miss_region;
    vk::StridedDeviceAddressRegionKHR m_hit_region;
    vk::StridedDeviceAddressRegionKHR m_call_region;
    void CreateRtShaderBindingTable();

    LightingPass* p_lighting_pass;
    DOFPass* p_dof_pass;

    bool enabled;
public:
    RayCastPass(Graphics* _p_gfx, RenderPass* p_prev_pass = nullptr);
    ~RayCastPass();

    void Setup() override;
    void Render() override;
    void Teardown() override;

    void DrawGUI() override;

    void SetLightingPass(LightingPass* _p_lighting_pass);
    void SetDOFPass(DOFPass* _p_lighting_pass);

    const ImageWrap& GetBGBuffer() const;
};

