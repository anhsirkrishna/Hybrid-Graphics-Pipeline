
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "shared_structs.h"

const vec3 ambientIntensity = vec3(0.2);

layout(push_constant) uniform _PushConstantRaster
{
  PushConstantRaster pcRaster;
};

// clang-format off
// Incoming 
layout(location=1) in vec4 worldPos;
layout(location=2) in vec3 worldNrm;
layout(location=3) in vec3 viewDir;
layout(location=4) in vec2 texCoord;
layout(location=5) in vec4 currPos;
layout(location=6) in vec4 prevPos;

// Outgoing
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragVeloDepth;

layout(buffer_reference, scalar) buffer Vertices {Vertex v[]; };    // Positions of an object
layout(buffer_reference, scalar) buffer Indices {uint i[]; };       // Triangle indices
layout(buffer_reference, scalar) buffer Materials {Material m[]; }; // Array of materials
layout(buffer_reference, scalar) buffer MatIndices {int i[]; };     // Material ID for each triangle

layout(binding=eObjDescs, scalar) buffer ObjDesc_ { ObjDesc i[]; } objDesc;
layout(binding=eTextures) uniform sampler2D[] textureSamplers;

float pi = 3.14159;
void main()
{
    // Material of the object
    ObjDesc    obj = objDesc.i[pcRaster.objIndex];
    MatIndices matIndices  = MatIndices(obj.materialIndexAddress);
    Materials  materials   = Materials(obj.materialAddress);
  
    int               matIndex = matIndices.i[gl_PrimitiveID];
    Material mat      = materials.m[matIndex];

    float lightIntensity = 2.5f;
    float ambientLight = 0.2f;
    vec3 lightPosition = vec3( 0.5f, 2.5f, 3.0f );

    vec3 N = normalize(worldNrm);
    vec3 V = normalize(viewDir);
    vec3 lDir = lightPosition - worldPos.xyz;
    vec3 L = normalize(lDir);
    vec3 H = normalize(L+V);
    
    float NL = max(dot(N, L), 0.0);
    float NH = max(dot(N, H), 0.0);
    float LH = max(dot(L, H), 0.0);
  
    vec3 Kd = mat.diffuse;
    vec3 Ks = mat.specular;
    const float alpha = mat.shininess;

    float epsilon = 0.00001;
  
    if (mat.textureId >= 0)
    {
    int  txtOffset  = obj.txtOffset;
    uint txtId      = txtOffset + mat.textureId;
    Kd = texture(textureSamplers[nonuniformEXT(txtId)], texCoord).xyz;
    }

    float ag = sqrt(2 / (mat.shininess + 2));
    float a_g2 = ag * ag; //2 / (alpha + 2);

    // Phong
    float d = (NH * NH) * (a_g2 - 1) + 1;
    float D = a_g2 / (pi * d * d);

    vec3 F = Ks + ((1.0 - Ks) * pow((1.0 - LH), 5));
    float Vis = 1.0 / (LH * LH);
        
    // BRDF w/view term approximation
    vec3 BRDF = (Kd / pi) + ((F * D * Vis) / 4);

    fragColor.xyz = vec3((ambientLight * Kd) + (lightIntensity * NL * BRDF)); 

    ivec2 img_space_curr = ivec2((((currPos.xy / currPos.w) * 0.5) + 0.5) * pcRaster.window_size);
    ivec2 img_space_prev = ivec2((((prevPos.xy / prevPos.w) * 0.5) + 0.5) * pcRaster.window_size);
    vec2 velo = img_space_curr - img_space_prev;
    velo = velo * pcRaster.exposure_time * pcRaster.frame_rate;
    vec2 pixel_size = 1.0f / pcRaster.window_size;
    vec2 half_pixel_size = 0.5f * pixel_size;
    vec2 out_velo = (velo * max(length(half_pixel_size), min(length(velo), pcRaster.tile_size))) 
                    / (length(velo) + epsilon); 
    fragVeloDepth = vec4(vec3(out_velo, 0.0f) , worldPos.w);
}
