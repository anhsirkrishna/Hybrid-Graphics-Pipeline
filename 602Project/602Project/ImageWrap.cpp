#include "ImageWrap.h"
#include "Graphics.h"

ImageWrap::ImageWrap(uint32_t width, uint32_t height, 
	vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect,
	vk::MemoryPropertyFlags properties, uint8_t mipLevels,
    const Graphics* gfx) :
    image_view(VK_NULL_HANDLE), sampler(VK_NULL_HANDLE) {
    vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        format,
        vk::Extent3D(width, height, 1),
        mipLevels,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        usage);
    
    image = gfx->GetDeviceRef().createImage(imageCreateInfo);

    vk::PhysicalDeviceMemoryProperties memoryProperties = gfx->GetPhysicalDeviceRef().getMemoryProperties();
    vk::MemoryRequirements memoryRequirements = gfx->GetDeviceRef().getImageMemoryRequirements(image);
    uint32_t typeIndex = gfx->FindMemoryType(memoryRequirements.memoryTypeBits, properties);

    memory = gfx->GetDeviceRef().allocateMemory(
        vk::MemoryAllocateInfo(memoryRequirements.size, typeIndex));

    gfx->GetDeviceRef().bindImageMemory(image, memory, 0);

    image_view = gfx->GetDeviceRef().createImageView(vk::ImageViewCreateInfo(
        vk::ImageViewCreateFlags(),
        image,
        vk::ImageViewType::e2D,
        format, {},
        { aspect, 0, 1, 0, 1 }));
}
