#pragma once
#include "RenderPass.h"

class LightingPass : public RenderPass {
private:
	//Resources required for the scanline render pass
	uint8_t buffer_mip_levels;
	ImageWrap m_buffer;
	ImageWrap m_velocity_buffer;
	void SetupBuffer();

	PushConstantRaster m_push_consts;

	std::vector<vk::AttachmentDescription> m_framebuffer_attachments;
	void SetupAttachments();

	vk::RenderPass m_render_pass;
	void SetupRenderPass();

	vk::Framebuffer m_framebuffer;
	void SetupFramebuffer();

	DescriptorWrap m_descriptor;
	void SetupDescriptor();

	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;
	void SetupPipeline();
public:
	LightingPass(Graphics* _p_gfx);
	~LightingPass();
	void Setup() override;
	void Render() override;
	void Teardown() override;
	void DrawGUI() override;

	const ImageWrap& GetBufferRef() const;
	const ImageWrap& GetVeloDepthBufferRef() const;
};

