#pragma once
#include "RenderPass.h"

class MedianPass : public RenderPass {
private:
	ImageWrap m_buffer;
	void SetupBuffer();

	DescriptorWrap m_descriptor;
	void SetupDescriptor();

	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;
	void SetupPipeline();
public:
	MedianPass(Graphics* _p_gfx, RenderPass* p_prev_pass = nullptr);
	~MedianPass();

	void Setup() override;
	void Render() override;
	void Teardown() override;

	void DrawGUI();

	const ImageWrap& GetBuffer() const;
};

