#pragma once
#include "RenderPass.h"

class Graphics;

class BufferDebugDraw : public RenderPass {
public:
	enum class DrawBuffer {
		DISABLE,
		VELOCITY,
		DEPTH,
		TILEMAX_COC,
		TILEMAX_VELO,
		NEIGHBOURMAX_COC,
		NEIGHBOURMAX_VELO,
		PRE_DOF,
		PRE_DOF_COC,
		PRE_DOF_BG,
		PRE_DOF_FG,
		DOF_BG,
		DOF_FG,
		DOF,
		DOF_ALPHA,
		MEDIAN
	};

private:
	vk::DescriptorImageInfo velo_depth_buffer_desc;
	vk::DescriptorImageInfo tile_max_buffer_desc;
	vk::DescriptorImageInfo neighbour_max_buffer_desc;
	vk::DescriptorImageInfo pre_dof_buffer_desc;
	vk::DescriptorImageInfo pre_dof_params_buffer_desc;
	vk::DescriptorImageInfo dof_bg_buffer_desc;
	vk::DescriptorImageInfo dof_fg_buffer_desc;
	vk::DescriptorImageInfo dof_buffer_desc;
	vk::DescriptorImageInfo median_buffer_desc;
	void SetDrawBuffer(vk::DescriptorImageInfo& draw_descriptor);

	DescriptorWrap m_descriptor;
	void SetupDescriptor();

	vk::RenderPass m_render_pass;
	void SetupRenderPass();

	std::vector<vk::Framebuffer> m_framebuffers;
	void SetupFramebuffer();

	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;
	void SetupPipeline();

	DrawBuffer draw_buffer;

	PushConstantDrawBuffer m_push_consts;
public:
	BufferDebugDraw(Graphics* _p_gfx, RenderPass* p_prev_pass = nullptr);
	~BufferDebugDraw();
	void Setup() override;
	void Render() override;
	void Teardown() override;

	void SetVeloDepthBuffer(const ImageWrap& draw_buffer);
	void SetTileMaxBuffer(const ImageWrap& draw_buffer);
	void SetNeighbourMaxBuffer(const ImageWrap& draw_buffer);
	void SetPreDOFBuffer(const ImageWrap& draw_buffer);
	void SetPreDOFParamsBuffer(const ImageWrap& draw_buffer);
	void SetDOFBGBuffer(const ImageWrap& draw_buffer);
	void SetDOFFGBuffer(const ImageWrap& draw_buffer);
	void SetDOFBuffer(const ImageWrap& draw_buffer);
	void SetMedianBuffer(const ImageWrap& draw_buffer);

	void DrawGUI();
};

