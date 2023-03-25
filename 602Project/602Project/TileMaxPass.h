#pragma once
#include "RenderPass.h"

class DOFPass;

class TileMaxPass : public RenderPass{
private:
	ImageWrap m_buffer;
	void SetupBuffer();

	DescriptorWrap m_descriptor;
	void SetupDescriptor();

	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;
	void SetupPipeline();

	PushConstantTileMax m_push_consts;

	DOFPass* p_dof_pass;
public:
	TileMaxPass(Graphics* _p_gfx, RenderPass* p_prev_pass = nullptr);
	~TileMaxPass();

	void Setup() override;
	void Render() override;
	void Teardown() override;

	void DrawGUI();

	const ImageWrap& GetBuffer() const;

	static const int tile_size = 20;

	void SetDOFPass(DOFPass* _p_dof_pass);
};

