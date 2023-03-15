#pragma once
#include "RenderPass.h"

class MBlurPass : public RenderPass {
private:
	ImageWrap m_buffer;
	void SetupBuffer();

	PushConstantMBlur m_push_consts;

	DescriptorWrap m_descriptor;
	void SetupDescriptor();

	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;
	void SetupPipeline();

	bool enabled;

	vk::DescriptorImageInfo neighbour_max_desc;
public:
	MBlurPass(Graphics* _p_gfx, RenderPass* p_prev_pass = nullptr);
	~MBlurPass();
	void Setup() override;
	void Render() override;
	void Teardown() override;

	void DrawGUI();

	void SetNeighbourMaxDesc(const ImageWrap& _buffer);
};

