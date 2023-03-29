#pragma once
#include "RenderPass.h"

class MedianPass : public RenderPass {
private:
	ImageWrap m_buffer;
	ImageWrap m_bg_buffer;
	void SetupBuffer();

	DescriptorWrap m_descriptor;
	void SetupDescriptor();

	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;
	void SetupPipeline();

	vk::DescriptorImageInfo rt_bg_desc;
public:
	MedianPass(Graphics* _p_gfx, RenderPass* p_prev_pass = nullptr);
	~MedianPass();

	void Setup() override;
	void Render() override;
	void Teardown() override;

	void DrawGUI();

	const ImageWrap& GetBuffer() const;
	const ImageWrap& GetBGBuffer() const;

	void SetRaycastBGDesc(const ImageWrap& _buffer);
};

