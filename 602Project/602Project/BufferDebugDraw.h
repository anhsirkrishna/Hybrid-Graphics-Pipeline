#pragma once
#include "RenderPass.h"

class Graphics;
class DOFPass;

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
		UPSCALED,
		EDGE,
		RAYCAST_BG,
		RAYMASK
	};

private:
	vk::DescriptorImageInfo velo_depth_buffer_desc;
	vk::DescriptorImageInfo tile_max_buffer_desc;
	vk::DescriptorImageInfo neighbour_max_buffer_desc;
	vk::DescriptorImageInfo pre_dof_buffer_desc;
	vk::DescriptorImageInfo pre_dof_params_buffer_desc;
	vk::DescriptorImageInfo median_bg_buffer_desc;
	vk::DescriptorImageInfo median_fg_buffer_desc;
	vk::DescriptorImageInfo dof_buffer_desc;
	vk::DescriptorImageInfo upscaled_buffer_desc;
	vk::DescriptorImageInfo edge_buffer_desc;
	vk::DescriptorImageInfo raycast_bg_buffer_desc;
	vk::DescriptorImageInfo raymask_buffer_desc;
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

	DOFPass* p_dof_pass;
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
	void SetDOFBuffer(const ImageWrap& draw_buffer);
	void SetMedianBGBuffer(const ImageWrap& draw_buffer);
	void SetMedianFGBuffer(const ImageWrap& draw_buffer);
	void SetUpscaledBuffer(const ImageWrap& draw_buffer);
	void SetRaymaskBuffer(const ImageWrap& draw_buffer);
	void SetRaycastBGBuffer(const ImageWrap& draw_buffer);
	void SetEdgeBuffer(const ImageWrap& draw_buffer);

	void DrawGUI();

	void SetDOFPass(DOFPass* _p_dof_pass);
};

