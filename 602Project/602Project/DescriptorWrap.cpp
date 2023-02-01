#include "ImageWrap.h"
#include "DescriptorWrap.h"
#include "Graphics.h"
#include <assert.h>

void DescriptorWrap::setBindings(const vk::Device device, std::vector<vk::DescriptorSetLayoutBinding> _bt) {
    glm::uint maxSets = 1;
    bindingTable = _bt;

    // Build descSetLayout
    vk::DescriptorSetLayoutCreateInfo createInfo;
    createInfo.setBindingCount(uint32_t(bindingTable.size()));
    createInfo.setPBindings(bindingTable.data());

    device.createDescriptorSetLayout(&createInfo, nullptr, &descSetLayout);

    // Collect the size required for each descriptorType into a vector of poolSizes
    std::vector<vk::DescriptorPoolSize> poolSizes;

    for (auto it = bindingTable.cbegin(); it != bindingTable.cend(); ++it)  {
        bool found = false;
        for (auto itpool = poolSizes.begin(); itpool != poolSizes.end(); ++itpool) {
            if(itpool->type == it->descriptorType) {
                itpool->descriptorCount += it->descriptorCount * maxSets;
                found = true;
                break; } }
    
        if (!found) {
            vk::DescriptorPoolSize poolSize;
            poolSize.setType(it->descriptorType);
            poolSize.setDescriptorCount(it->descriptorCount * maxSets);
            poolSizes.push_back(poolSize); } }

    
    // Build descPool
    vk::DescriptorPoolCreateInfo descrPoolInfo;
    descrPoolInfo.setMaxSets(maxSets);
    descrPoolInfo.setPoolSizeCount(poolSizes.size());
    descrPoolInfo.setPPoolSizes(poolSizes.data());
    device.createDescriptorPool(&descrPoolInfo, nullptr, &descPool);

    // Allocate DescriptorSet/
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.setDescriptorPool(descPool);
    allocInfo.setDescriptorSetCount(1);
    allocInfo.setPSetLayouts(&descSetLayout);

    // Warning: The next line creates a single descriptor set from the
    // above pool since that's all our program needs.  This is too
    // restrictive in general, but fine for this program.
    device.allocateDescriptorSets(&allocInfo, &descSet);
}

void DescriptorWrap::destroy(vk::Device& device) {
    device.destroyDescriptorSetLayout(descSetLayout, nullptr);
    device.destroyDescriptorPool(descPool, nullptr);
}

void DescriptorWrap::write(vk::Device& device, glm::uint index, const vk::Buffer& buffer) {
    vk::DescriptorBufferInfo desBuf;
    desBuf.setBuffer(buffer);
    desBuf.setOffset(0);
    desBuf.setRange(VK_WHOLE_SIZE);
    
    vk::WriteDescriptorSet writeSet;
    writeSet.setDstSet(descSet);
    writeSet.setDstBinding(index);
    writeSet.setDstArrayElement(0);
    writeSet.setDescriptorCount(1);
    writeSet.setDescriptorType(vk::DescriptorType(bindingTable[index].descriptorType));
    writeSet.setPBufferInfo(&desBuf);

    assert(bindingTable[index].binding == index);

    assert(writeSet.descriptorType == vk::DescriptorType::eStorageBuffer ||
            writeSet.descriptorType == vk::DescriptorType::eStorageBufferDynamic ||
            writeSet.descriptorType == vk::DescriptorType::eUniformBuffer ||
            writeSet.descriptorType == vk::DescriptorType::eUniformBufferDynamic);
    
    device.updateDescriptorSets(1, &writeSet, 0, nullptr);

}

void DescriptorWrap::write(vk::Device& device, glm::uint index, const vk::DescriptorImageInfo& textureDesc) {
    vk::WriteDescriptorSet writeSet;
    writeSet.setDstSet(descSet);
    writeSet.setDstBinding(index);
    writeSet.setDstArrayElement(0);
    writeSet.setDescriptorCount(1);
    writeSet.setDescriptorType(vk::DescriptorType(bindingTable[index].descriptorType));
    writeSet.setPImageInfo(&textureDesc);

    assert(bindingTable[index].binding == index);

    assert(writeSet.descriptorType == vk::DescriptorType::eSampler ||
        writeSet.descriptorType == vk::DescriptorType::eCombinedImageSampler||
        writeSet.descriptorType == vk::DescriptorType::eSampledImage ||
        writeSet.descriptorType == vk::DescriptorType::eStorageImage ||
        writeSet.descriptorType == vk::DescriptorType::eInputAttachment);
    
    device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}

void DescriptorWrap::write(vk::Device& device, glm::uint index, const std::vector<ImageWrap>& textures) {
    std::vector<vk::DescriptorImageInfo> des;
    for (auto& texture : textures)
        des.emplace_back(texture.Descriptor());

    vk::WriteDescriptorSet writeSet;
    writeSet.setDstSet(descSet);
    writeSet.setDstBinding(index);
    writeSet.setDstArrayElement(0);
    writeSet.setDescriptorCount(des.size());
    writeSet.setDescriptorType(vk::DescriptorType(bindingTable[index].descriptorType));
    writeSet.setPImageInfo(des.data());


    assert(bindingTable[index].binding == index);

    assert(writeSet.descriptorType == vk::DescriptorType::eSampler ||
        writeSet.descriptorType == vk::DescriptorType::eCombinedImageSampler || 
        writeSet.descriptorType == vk::DescriptorType::eSampledImage ||
        writeSet.descriptorType == vk::DescriptorType::eStorageImage || 
        writeSet.descriptorType == vk::DescriptorType::eInputAttachment);

    device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}

void DescriptorWrap::write(vk::Device& device, glm::uint index, const vk::AccelerationStructureKHR& tlas) {
    vk::WriteDescriptorSetAccelerationStructureKHR descASInfo;
    descASInfo.setAccelerationStructureCount(1);
    descASInfo.setPAccelerationStructures(&tlas);

    vk::WriteDescriptorSet writeSet;
    writeSet.setDstSet(descSet);
    writeSet.setDstBinding(index);
    writeSet.setDstArrayElement(0);
    writeSet.setDescriptorCount(1);
    writeSet.setDescriptorType(vk::DescriptorType(bindingTable[index].descriptorType));
    writeSet.setPNext(&descASInfo);

    assert(bindingTable[index].binding == index);

    assert(writeSet.descriptorType == vk::DescriptorType::eAccelerationStructureKHR);
    device.updateDescriptorSets(1, &writeSet, 0, nullptr);
}
