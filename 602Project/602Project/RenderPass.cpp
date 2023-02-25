#include "RenderPass.h"

RenderPass::RenderPass(Graphics* _p_gfx, RenderPass* _p_prev_pass) : 
	p_gfx(_p_gfx), p_prev_pass(_p_prev_pass) {

}

void RenderPass::DrawGUI() {
}
