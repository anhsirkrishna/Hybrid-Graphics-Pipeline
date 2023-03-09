

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

float ComputeDepthBlur(float depth) {
    float depth_blur;
    if (depth < pcDebugBuffer.focal_plane) {
        depth_blur = (depth - pcDebugBuffer.focal_plane) / (pcDebugBuffer.focal_plane - pcDebugBuffer.near_plane);
    }
    else {
        depth_blur = (depth - pcDebugBuffer.focal_plane) / (pcDebugBuffer.far_plane - pcDebugBuffer.focal_plane);
        depth_blur = min(depth_blur, 1);
    }

    //Scale and bias into range of 0 - 1
    return (depth_blur * 0.5) + 0.5 ;
}

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
        float rel_depth = ComputeDepthBlur(texture(renderedImage, uv).w);
        fragColor = vec4(vec3(rel_depth), 1.0f);
    }
}
