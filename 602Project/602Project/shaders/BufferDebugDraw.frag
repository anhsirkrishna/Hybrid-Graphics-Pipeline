

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "shared_structs.h"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(set=0, binding=0) uniform sampler2D renderedImage;

layout(push_constant) uniform _PushConstantDrawBuffer
{
  PushConstantDrawBuffer pcDebugBuffer;
};

void main() {
    if (pcDebugBuffer.alignmentTest != 1234){
        fragColor = vec4(1, 1, 1, 1);
        return;
    }

    if (pcDebugBuffer.draw_buffer == 1)
    {
        //Draw the velocity buffer
        fragColor   = vec4(texture(renderedImage, uv).xyz, 1.0f);
    }        
    else if (pcDebugBuffer.draw_buffer == 2)
    {
        //Draw the depth buffer
        float rel_depth = texture(renderedImage, uv).w;
        fragColor = vec4(vec3(rel_depth), 1.0f);
    }
    else if (pcDebugBuffer.draw_buffer == 3) 
    {
        //Draw the TILEMAX_COC buffer
        fragColor = vec4(texture(renderedImage, uv).w);
    }
    else if (pcDebugBuffer.draw_buffer == 4) 
    {
        //Draw the TILEMAX_VELO buffer
        fragColor = vec4(texture(renderedImage, uv).rg, 0.0f, 1.0f);
    }
    else if (pcDebugBuffer.draw_buffer == 5) 
    {
        //Draw the NEIGHBOURMAX_COC buffer
        fragColor = vec4(texture(renderedImage, uv).w);
    }
    else if (pcDebugBuffer.draw_buffer == 6) 
    {
        //Draw the NEIGHBOURMAX_VELO buffer
        fragColor = vec4(texture(renderedImage, uv).rg, 0.0f, 1.0f);
    }
    else if (pcDebugBuffer.draw_buffer == 7)
    {
        //Draw the downscaled filtered pre dof buffer
        fragColor = vec4(texture(renderedImage, uv).rgb, 1.0f);
    }
    else if (pcDebugBuffer.draw_buffer == 8)
    {
        //Draw the downscaled filtered pre dof COC buffer
        fragColor = vec4(texture(renderedImage, uv).r, 0.0f, 0.0f, 1.0f);
    }
    else if (pcDebugBuffer.draw_buffer == 9)
    {
        //Draw the downscaled filtered pre dof BG buffer
        fragColor = vec4(0.0f, texture(renderedImage, uv).g, 0.0f, 1.0f);
    }
    else if (pcDebugBuffer.draw_buffer == 10)
    {
        //Draw the downscaled filtered pre dof FG buffer
        fragColor = vec4(0.0f, 0.0f, texture(renderedImage, uv).b, 1.0f);
    }
}
