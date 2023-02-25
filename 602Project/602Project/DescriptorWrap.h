
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
    void destroy(const vk::Device& device);

    void write(const vk::Device& device, glm::uint index, const vk::Buffer& buffer);
    void write(const vk::Device& device, glm::uint index, const vk::DescriptorImageInfo& textureDesc);
    void write(const vk::Device& device, glm::uint index, const vk::AccelerationStructureKHR& tlas);
    void write(const vk::Device& device, glm::uint index, const std::vector<ImageWrap>& textures);
};
