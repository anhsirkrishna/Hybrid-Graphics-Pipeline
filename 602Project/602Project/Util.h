#pragma once

#include <string>
#include <glm/glm.hpp>
#include "BufferWrap.h"

std::string LoadFileIntoString(const std::string& filename);

struct ObjData
{
    uint32_t     nbIndices{ 0 };
    uint32_t     nbVertices{ 0 };
    glm::mat4 transform;      // Instance matrix of the object
    BufferWrap vertexBuffer;    // Device buffer of all 'Vertex'
    BufferWrap indexBuffer;     // Device buffer of the indices forming triangles
    BufferWrap matColorBuffer;  // Device buffer of array of 'Wavefront material'
    BufferWrap matIndexBuffer;  // Device buffer of array of 'Wavefront material'
    BufferWrap lightBuffer;     // Device buffer of all the light triangle indeces.

    void destroy(vk::Device& device) {
        vertexBuffer.destroy(device);
        indexBuffer.destroy(device);
        matColorBuffer.destroy(device);
        matIndexBuffer.destroy(device);
        lightBuffer.destroy(device);
    }
};

struct ObjInst
{
    glm::mat4 transform;    // Matrix of the instance
    uint32_t  objIndex;     // Model index
};