#pragma once
#include "RenderPass.h"

class Graphics;

class DOFPass : public RenderPass {
private:
	ImageWrap m_buffer;
	void SetupBuffer();

	PushConstantDoF m_push_consts;
	
	DescriptorWrap m_descriptor;
	void SetupDescriptor();

	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;
	void SetupPipeline();

public:
	DOFPass(Graphics* _p_gfx, RenderPass* p_prev_pass=nullptr);
	~DOFPass();
	void Setup() override;
	void Render() override;
	void Teardown() override;

	void WriteToDescriptor(glm::uint index, const vk::DescriptorImageInfo img_desc_info);

	void DrawGUI();
};
