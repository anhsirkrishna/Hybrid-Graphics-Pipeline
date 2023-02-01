
#pragma once

#include <stdio.h>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class DescriptorWrap
{
public:
    std::vector<vk::DescriptorSetLayoutBinding> bindingTable;
    
    vk::DescriptorSetLayout descSetLayout;

    vk::DescriptorPool descPool;
    vk::DescriptorSet descSet;// Could be  vector<VkDescriptorSet> for multiple sets;

    void setBindings(const vk::Device device, std::vector<vk::DescriptorSetLayoutBinding> _bt);
    void destroy(vk::Device& device);

    void write(vk::Device& device, glm::uint index, const vk::Buffer& buffer);
    void write(vk::Device& device, glm::uint index, const vk::DescriptorImageInfo& textureDesc);
    void write(vk::Device& device, glm::uint index, const std::vector<ImageWrap>& textures);
    void write(vk::Device& device, glm::uint index, const vk::AccelerationStructureKHR& tlas);
};
