#pragma once

#include <vulkan/vulkan.hpp>

class Graphics;

/*
* A wrapper class around some related vulkan structures
*/
class ImageWrap {
public:
    vk::Image          image;
    vk::DeviceMemory   memory;
    vk::Sampler        sampler;
    vk::ImageView      image_view;
    vk::ImageLayout    image_layout;

    ImageWrap() = default;
    ImageWrap(uint32_t width, uint32_t height,
        vk::Format format,
        vk::ImageUsageFlags usage,
        vk::ImageAspectFlags aspect,
        vk::MemoryPropertyFlags properties, uint8_t mipLevels,
        const Graphics* gfx);

    void destroy(const vk::Device& device) {
        device.destroyImage(image);
        device.destroyImageView(image_view);
        device.freeMemory(memory);
        device.destroySampler(sampler);
    }

    vk::DescriptorImageInfo Descriptor() const {
        return vk::DescriptorImageInfo({ sampler, image_view, image_layout });
    }
};

