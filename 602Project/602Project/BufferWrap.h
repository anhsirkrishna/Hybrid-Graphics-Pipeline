#pragma once

#include <vulkan/vulkan.hpp>

struct BufferWrap
{
    vk::Buffer buffer;
    vk::DeviceMemory memory;

    void destroy(const vk::Device& device)
    {
        device.destroyBuffer(buffer);
        device.freeMemory(memory);
    }

};