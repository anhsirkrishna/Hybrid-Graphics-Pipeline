#include "Graphics.h"

void Graphics::CreateObjDescriptionBuffer() {
    vk::CommandBuffer cmdBuf = CreateTempCommandBuffer();

    m_objDescriptionBW = CreateStagedBufferWrap(cmdBuf, m_objDesc, vk::BufferUsageFlagBits::eStorageBuffer);

    SubmitTempCommandBuffer(cmdBuf);
}

void Graphics::CreateMatrixBuffer() {
    m_matrixBW = CreateBufferWrap(sizeof(MatrixUniforms),
        vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
}
