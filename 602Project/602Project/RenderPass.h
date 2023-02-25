#pragma once

class Graphics;

class RenderPass
{
public:
	Graphics* p_gfx;

	RenderPass(Graphics* _p_gfx, RenderPass* _p_prev_pass = nullptr);
	virtual ~RenderPass() = default;
	virtual void Setup() = 0;
	virtual void Render() = 0;
	virtual void Teardown() = 0;
	virtual void DrawGUI();
protected:
	RenderPass* p_prev_pass;
};

