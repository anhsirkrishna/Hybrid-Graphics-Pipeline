#include "ImageWrap.h"
#include "Graphics.h"

ImageWrap::ImageWrap(uint32_t width, uint32_t height, 
	vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect,
	vk::MemoryPropertyFlags properties, uint8_t mipLevels,
    Graphics* gfx) : p_gfx(gfx), image_size(width, height),
    image_layout(vk::ImageLayout::eUndefined), image_format(format), image_aspect(aspect),
    image_view(VK_NULL_HANDLE), sampler(VK_NULL_HANDLE), mip_levels(mipLevels) {
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

void ImageWrap::TransitionImageLayout(vk::ImageLayout new_layout) {
    vk::CommandBuffer commandBuffer = p_gfx->CreateTempCommandBuffer();

    vk::ImageMemoryBarrier barrier;
    barrier.setOldLayout(image_layout);
    barrier.setNewLayout(new_layout);
    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setImage(image);

    vk::ImageSubresourceRange subResRange;
    subResRange.setAspectMask(image_aspect);
    subResRange.setBaseMipLevel(0);
    subResRange.setLevelCount(mip_levels);
    subResRange.setBaseArrayLayer(0);
    subResRange.setLayerCount(1);
    barrier.setSubresourceRange(subResRange);
    barrier.setSrcAccessMask(vk::AccessFlagBits::eNone);
    barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

    vk::PipelineStageFlags sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    vk::PipelineStageFlags destinationStage = vk::PipelineStageFlagBits::eTransfer;

    commandBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &barrier);
    p_gfx->SubmitTempCommandBuffer(commandBuffer);
    image_layout = new_layout;
}

void ImageWrap::CopyFromBuffer(vk::Buffer buffer, uint32_t width, uint32_t height) {
    vk::CommandBuffer commandBuffer = p_gfx->CreateTempCommandBuffer();

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = image_aspect;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D(0, 0, 0);
    region.imageExtent = vk::Extent3D(width, height, 1);

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

    p_gfx->SubmitTempCommandBuffer(commandBuffer);
}

void ImageWrap::GenerateMipMaps() {
    // Check if image format supports linear blitting
    vk::FormatProperties formatProperties = p_gfx->GetPhysicalDeviceRef().getFormatProperties(image_format);
    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    vk::CommandBuffer commandBuffer = p_gfx->CreateTempCommandBuffer();

    vk::ImageMemoryBarrier barrier;
    barrier.setImage(image);
    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    subresourceRange.setLevelCount(1);
    subresourceRange.setLayerCount(1);
    subresourceRange.setBaseArrayLayer(0);

    barrier.setSubresourceRange(subresourceRange);

    int32_t mipWidth = image_size.width;
    int32_t mipHeight = image_size.height;

    for (uint32_t i = 1; i < mip_levels; i++) {
        subresourceRange.setBaseMipLevel(i - 1);
        barrier.setSubresourceRange(subresourceRange);
        barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
        barrier.setNewLayout(vk::ImageLayout::eTransferSrcOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
        barrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
            0, nullptr, 0, nullptr, 1, &barrier);

        vk::ImageBlit blit;

        std::array<vk::Offset3D, 2> srcOffsets;
        srcOffsets[0] = vk::Offset3D{ 0, 0, 0 };
        srcOffsets[1] = vk::Offset3D{ mipWidth, mipHeight, 1 };
        blit.setSrcOffsets(srcOffsets);
        vk::ImageSubresourceLayers srcsubresourceLayers;
        srcsubresourceLayers.setAspectMask(image_aspect);
        srcsubresourceLayers.setMipLevel(i - 1);
        srcsubresourceLayers.setBaseArrayLayer(0);
        srcsubresourceLayers.setLayerCount(1);
        blit.setSrcSubresource(srcsubresourceLayers);

        std::array<vk::Offset3D, 2> dstOffsets;
        dstOffsets[0] = vk::Offset3D{ 0, 0, 0 };
        dstOffsets[1] = vk::Offset3D{ mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.setDstOffsets(dstOffsets);
        vk::ImageSubresourceLayers dstsubresourceLayers;
        dstsubresourceLayers.setAspectMask(image_aspect);
        dstsubresourceLayers.setMipLevel(i);
        dstsubresourceLayers.setBaseArrayLayer(0);
        dstsubresourceLayers.setLayerCount(1);
        blit.setDstSubresource(dstsubresourceLayers);

        commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
            image, vk::ImageLayout::eTransferDstOptimal,
            1, &blit, vk::Filter::eLinear);

        barrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal);
        barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferRead);
        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(),
            0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    subresourceRange.setBaseMipLevel(mip_levels - 1);
    barrier.setSubresourceRange(subresourceRange);
    barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
    barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
    barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(),
        0, nullptr, 0, nullptr, 1, &barrier);

    p_gfx->SubmitTempCommandBuffer(commandBuffer);

    image_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
}

void ImageWrap::CreateTextureSampler(vk::ImageLayout set_layout) {
    vk::PhysicalDeviceProperties properties = p_gfx->GetPhysicalDeviceRef().getProperties();

    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

    sampler = p_gfx->m_device.createSampler(samplerInfo);
}

vk::ImageView ImageWrap::GetImageView() const {
    return image_view;
}

const vk::Format& ImageWrap::GetFormat() const {
    return image_format;
}
