#pragma once
#include "RenderPass.h"

class DOFPass;

class UpscalePass : public RenderPass {
private:
	ImageWrap m_buffer;
	void SetupBuffer();

	DescriptorWrap m_descriptor;
	void SetupDescriptor();

	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;
	void SetupPipeline();

	PushConstantDoF m_push_consts;

	vk::DescriptorImageInfo neighbour_max_buffer_desc;
	vk::DescriptorImageInfo fullres_buffer_desc;
	vk::DescriptorImageInfo fullres_depth_buffer_desc;

	DOFPass* p_dof_pass;

	bool enabled;
public:
	UpscalePass(Graphics* _p_gfx, RenderPass* p_prev_pass = nullptr);
	~UpscalePass();

	void Setup() override;
	void Render() override;
	void Teardown() override;

	void DrawGUI();

	const ImageWrap& GetBuffer() const;

	void SetFullResBufferDesc(const ImageWrap& _buffer);
	void SetFullResDepthBufferDesc(const ImageWrap& _buffer);
	void SetNeighbourBufferDesc(const ImageWrap& _buffer);

	void SetDOFPass(DOFPass* _p_dof_pass);
};

