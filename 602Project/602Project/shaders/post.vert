
#version 450
layout(location = 0) out vec2 uv;

vec2 points[3] = vec2[]( vec2(0,0), vec2(0,2), vec2(2,0) );

void main()
{
  uv = points[gl_VertexIndex];
  gl_Position.xyz = vec3(uv*2.0f-1.0f, 1.0);
}
