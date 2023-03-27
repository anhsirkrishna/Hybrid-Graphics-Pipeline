#pragma once
#include "RenderPass.h"

class RayMaskPass : public RenderPass {
private:
	ImageWrap m_buffer;
	void SetupBuffer();

	DescriptorWrap m_descriptor;
	void SetupDescriptor();

	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;
	void SetupPipeline();

	PushConstantRaymask m_push_consts;

	bool enabled;
public:
	RayMaskPass(Graphics* _p_gfx, RenderPass* p_prev_pass = nullptr);
	~RayMaskPass();
	void Setup() override;
	void Render() override;
	void Teardown() override;

	void WriteToDescriptor(glm::uint index, const vk::DescriptorImageInfo img_desc_info);

	const ImageWrap& GetBuffer() const;

	void DrawGUI();
};

