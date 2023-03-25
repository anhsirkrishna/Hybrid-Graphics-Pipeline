#pragma once
#include "RenderPass.h"

class PreDOFPass : public RenderPass
{
private:
	ImageWrap m_buffer;
	ImageWrap m_params_buffer;
	void SetupBuffer();

	PushConstantPreDoF m_push_consts;

	DescriptorWrap m_descriptor;
	void SetupDescriptor();

	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;
	void SetupPipeline();

	vk::DescriptorImageInfo neighbour_max_buffer_desc;

	bool enabled;

	class DOFPass* p_dof_pass;
public:
	PreDOFPass(Graphics* _p_gfx, RenderPass* p_prev_pass = nullptr);
	~PreDOFPass();
	void Setup() override;
	void Render() override;
	void Teardown() override;

	void WriteToDescriptor(glm::uint index, const vk::DescriptorImageInfo img_desc_info);
	void SetNeighbourMaxBufferDesc(const ImageWrap& buffer);

	void SetDOFPass(DOFPass* _p_dof_pass);

	const ImageWrap& GetBuffer() const;
	const ImageWrap& GetParamsBuffer() const;

	void DrawGUI();
};

