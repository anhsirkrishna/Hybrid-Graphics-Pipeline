#pragma once

#include <vulkan/vulkan.hpp>

class Graphics;

/*
* A wrapper class around some related vulkan structures
*/
class ImageWrap {
private:
    vk::Image               image;
    vk::DeviceMemory        memory;
    vk::Sampler             sampler;
    vk::ImageView           image_view;
    vk::ImageLayout         image_layout;
    vk::ImageAspectFlags    image_aspect;
    vk::Format              image_format;
    vk::Extent2D            image_size;
    uint8_t mip_levels;

    Graphics* p_gfx;
public:
    ImageWrap() = default;
    ImageWrap(uint32_t width, uint32_t height,
        vk::Format format,
        vk::ImageUsageFlags usage,
        vk::ImageAspectFlags aspect,
        vk::MemoryPropertyFlags properties, uint8_t mipLevels,
        Graphics* gfx);

    void TransitionImageLayout(vk::ImageLayout new_layout);

    void CopyFromBuffer(vk::Buffer, uint32_t width, uint32_t height);

    void GenerateMipMaps();

    void CreateTextureSampler();

    void destroy(const vk::Device& device) {
        device.destroyImage(image);
        device.destroyImageView(image_view);
        device.freeMemory(memory);
        device.destroySampler(sampler);
    }

    vk::DescriptorImageInfo Descriptor() const {
        return vk::DescriptorImageInfo({ sampler, image_view, image_layout });
    }
     
    vk::Image GetImage() const {
        return image;
    }
    
    vk::ImageView GetImageView() const;

    const vk::Format& GetFormat() const;
};

