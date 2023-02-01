
#version 450
layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(set=0, binding=0) uniform sampler2D renderedImage;

void main()
{
    // The power exponent, 1/2.2 converts linear color space to SRGB
    // colorspace for proper display on a non-linear device.
    fragColor   = pow(texture(renderedImage, uv).rgba, vec4(1.0/2.2));
}
