#include "AccelerationWrap.h"
#include "Graphics.h"

#include <numeric>

#undef MemoryBarrier

WrapAccelerationStructure createAcceleration(Graphics* _p_gfx,
    vk::AccelerationStructureCreateInfoKHR& accel_)
{
    //printf("createAcceleration (6)\n");
    WrapAccelerationStructure result;
    // Allocating the buffer to hold the acceleration structure
    result.bw = _p_gfx->CreateBufferWrap(accel_.size, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    // Create the acceleration structure
    accel_.buffer = result.bw.buffer;
    _p_gfx->GetDeviceRef().createAccelerationStructureKHR(&accel_, nullptr, &result.accel);
    return result;
}

RaytracingBuilderKHR::RaytracingBuilderKHR(Graphics* _p_gfx) :
    p_gfx(_p_gfx) {
}

//Destroy any allocations created
RaytracingBuilderKHR::~RaytracingBuilderKHR() {
    printf("RaytracingBuilderKHR::destroy (6)\n");
    for (auto& blas : m_blas) {
        blas.bw.destroy(p_gfx->GetDeviceRef());
        p_gfx->GetDeviceRef().destroyAccelerationStructureKHR(blas.accel, nullptr);
    }

    m_tlas.bw.destroy(p_gfx->GetDeviceRef());
    p_gfx->GetDeviceRef().destroyAccelerationStructureKHR(m_tlas.accel, nullptr);
    m_blas.clear();
}

const vk::AccelerationStructureKHR& RaytracingBuilderKHR::GetAccelerationStructure() const {
    return m_tlas.accel;
}

vk::DeviceAddress RaytracingBuilderKHR::GetBlasDeviceAddress(uint32_t blasId) {
    assert(size_t(blasId) < m_blas.size());
    vk::AccelerationStructureDeviceAddressInfoKHR addressInfo;
    addressInfo.setAccelerationStructure(m_blas[blasId].accel);
    return p_gfx->GetDeviceRef().getAccelerationStructureAddressKHR(&addressInfo);
}

//--------------------------------------------------------------------------------------------------
// Create all the BLAS from the vector of BlasInput
// - There will be one BLAS per input-vector entry
// - There will be as many BLAS as input.size()
// - The resulting BLAS (along with the inputs used to build) are stored in m_blas,
//   and can be referenced by index.
// - if flag has the 'Compact' flag, the BLAS will be compacted
void RaytracingBuilderKHR::BuildBlas(const std::vector<BlasInput>& input, vk::BuildAccelerationStructureFlagsKHR flags) {
    auto         nbBlas = static_cast<uint32_t>(input.size());
    vk::DeviceSize asTotalSize{ 0 };     // Memory size of all allocated BLAS
    uint32_t     nbCompactions{ 0 };   // Nb of BLAS requesting compaction
    vk::DeviceSize maxScratchSize{ 0 };  // Largest scratch size

    // Preparing the information for the acceleration build commands.
    std::vector<BuildAccelerationStructure> buildAs(nbBlas);
    for (uint32_t idx = 0; idx < nbBlas; idx++)
    {
        // Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
        // Other information will be filled in the createBlas (see #2)
        buildAs[idx].buildInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
        buildAs[idx].buildInfo.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);
        buildAs[idx].buildInfo.setFlags(input[idx].flags | flags);
        buildAs[idx].buildInfo.setGeometryCount(static_cast<uint32_t>(input[idx].asGeometry.size()));
        buildAs[idx].buildInfo.setPGeometries(input[idx].asGeometry.data());

        // Build range information
        buildAs[idx].rangeInfo = input[idx].asBuildOffsetInfo.data();

        // Finding sizes to create acceleration structures and scratch
        std::vector<uint32_t> maxPrimCount(input[idx].asBuildOffsetInfo.size());
        for (auto tt = 0; tt < input[idx].asBuildOffsetInfo.size(); tt++)
            maxPrimCount[tt] = input[idx].asBuildOffsetInfo[tt].primitiveCount; //# of triangles
        p_gfx->GetDeviceRef().getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &buildAs[idx].buildInfo,
            maxPrimCount.data(), &buildAs[idx].sizeInfo);
        // Extra info
        asTotalSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        maxScratchSize = std::max(maxScratchSize, buildAs[idx].sizeInfo.buildScratchSize);
        nbCompactions += HasFlag(buildAs[idx].buildInfo.flags, vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction);
    }

    // Allocate the scratch buffers holding the temporary data of the acceleration structure builder
    BufferWrap scratch_buff = p_gfx->CreateBufferWrap(maxScratchSize,
        vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal);


    vk::BufferDeviceAddressInfo bufferInfo;
    bufferInfo.setBuffer(scratch_buff.buffer);
    vk::DeviceAddress scratchAddress = p_gfx->GetDeviceRef().getBufferAddress(&bufferInfo);

    // Allocate a query pool for storing the needed size for every BLAS compaction.
    vk::QueryPool queryPool;
    if (nbCompactions > 0)  // Is compaction requested?
    {
        assert(nbCompactions == nbBlas);  // Don't allow mix of on/off compaction
        vk::QueryPoolCreateInfo qpci;
        qpci.setQueryCount(nbBlas);
        qpci.setQueryType(vk::QueryType::eAccelerationStructureCompactedSizeKHR);
        p_gfx->GetDeviceRef().createQueryPool(&qpci, nullptr, &queryPool);
    }

    // Batching creation/compaction of BLAS to allow staying in restricted amount of memory
    std::vector<uint32_t> indices;  // Indices of the BLAS to create
    vk::DeviceSize          batchSize{ 0 };
    vk::DeviceSize          batchLimit{ 256'000'000 };  // 256 MB
    for (uint32_t idx = 0; idx < nbBlas; idx++)
    {
        indices.push_back(idx);
        batchSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        // Over the limit or last BLAS element
        if (batchSize >= batchLimit || idx == nbBlas - 1)
        {
            vk::CommandBuffer cmdBuf = p_gfx->CreateTempCommandBuffer();
            CmdCreateBlas(cmdBuf, indices, buildAs, scratchAddress, queryPool);
            p_gfx->SubmitTempCommandBuffer(cmdBuf);

            if (queryPool)
            {
                vk::CommandBuffer cmdBuf = p_gfx->CreateTempCommandBuffer();
                CmdCompactBlas(cmdBuf, indices, buildAs, queryPool);
                p_gfx->SubmitTempCommandBuffer(cmdBuf);

                // Destroy the non-compacted version
                DestroyNonCompacted(indices, buildAs);
            }
            // Reset

            batchSize = 0;
            indices.clear();
        }
    }

    // Logging reduction
    if (queryPool)
    {
        vk::DeviceSize compactSize = std::accumulate(buildAs.begin(), buildAs.end(), 0ULL, [](const auto& a, const auto& b) {
            return a + b.sizeInfo.accelerationStructureSize;
            });
    }

    // Keeping all the created acceleration structures
    for (auto& b : buildAs)
    {
        m_blas.emplace_back(b.as);
    }

    // Clean up
    p_gfx->GetDeviceRef().destroyQueryPool(queryPool, nullptr);
    scratch_buff.destroy(p_gfx->GetDeviceRef());
}

void RaytracingBuilderKHR::BuildTlas(const std::vector<vk::AccelerationStructureInstanceKHR>& instances, vk::BuildAccelerationStructureFlagsKHR flags, bool update, bool motion) {

    printf("RaytracingBuilderKHR::buildTlas (30)\n");
    uint32_t countInstance = static_cast<uint32_t>(instances.size());

    // Command buffer to create the TLAS
    vk::CommandBuffer    cmdBuf = p_gfx->CreateTempCommandBuffer();

    // Create a buffer holding the actual instance data (matrices++) for use by the AS builder
    BufferWrap instancesBuffer = p_gfx->CreateStagedBufferWrap(cmdBuf, instances,
        vk::BufferUsageFlagBits::eShaderDeviceAddress | 
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);

    vk::BufferDeviceAddressInfo bufferInfo;
    bufferInfo.setBuffer(instancesBuffer.buffer);
    vk::DeviceAddress instBufferAddr = p_gfx->GetDeviceRef().getBufferAddress(&bufferInfo);
    // Make sure the copy of the instance buffer are copied before triggering the acceleration structure build
    vk::MemoryBarrier barrier;
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
    barrier.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureWriteKHR);
    cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
        vk::DependencyFlags(), 1, &barrier, 0, nullptr, 0, nullptr);

    // Creating the TLAS
    CmdCreateTlas(cmdBuf, countInstance, instBufferAddr, flags, update, motion);

    // Finalizing and destroying temporary data
    p_gfx->SubmitTempCommandBuffer(cmdBuf);
    instancesBuffer.destroy(p_gfx->GetDeviceRef());
    scratch_buffer.destroy(p_gfx->GetDeviceRef());
}

void RaytracingBuilderKHR::CmdCreateBlas(vk::CommandBuffer cmdBuf, std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs, vk::DeviceAddress scratchAddress, vk::QueryPool queryPool) {
    printf("RaytracingBuilderKHR::cmdCreateBlas (40)\n");
    if (queryPool)  // For querying the compaction size
        vkResetQueryPool(p_gfx->GetDeviceRef(), queryPool, 0, static_cast<uint32_t>(indices.size()));
    uint32_t queryCnt{ 0 };

    for (const auto& idx : indices)
    {
        // Actual allocation of buffer and acceleration structure.
        vk::AccelerationStructureCreateInfoKHR createInfo;
        createInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
        // Will be used to allocate memory.
        createInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize;
        buildAs[idx].as = createAcceleration(p_gfx, createInfo);

        // BuildInfo #2 part
        // Setting where the build lands
        buildAs[idx].buildInfo.dstAccelerationStructure = buildAs[idx].as.accel;
        // All build use the same scratch buffer
        buildAs[idx].buildInfo.scratchData.deviceAddress = scratchAddress;

        // Building the bottom-level-acceleration-structure
        cmdBuf.buildAccelerationStructuresKHR(1, &buildAs[idx].buildInfo,
            &buildAs[idx].rangeInfo);

        // Since the scratch buffer is reused across builds, we
        // need a barrier to ensure one build is finished before
        // starting the next one.
        vk::MemoryBarrier barrier;
        barrier.setSrcAccessMask(vk::AccessFlagBits::eAccelerationStructureWriteKHR);
        barrier.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR);
        cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
            vk::DependencyFlags(), 1, &barrier, 0, nullptr, 0, nullptr);


        if (queryPool)
        {
            cmdBuf.writeAccelerationStructuresPropertiesKHR(1, &buildAs[idx].buildInfo.dstAccelerationStructure,
                vk::QueryType::eAccelerationStructureCompactedSizeKHR, queryPool, queryCnt++);
        }
    }
}

void RaytracingBuilderKHR::CmdCompactBlas(vk::CommandBuffer cmdBuf, std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs, vk::QueryPool queryPool) {
    //printf("RaytracingBuilderKHR::cmdCompactBlas\n");
    uint32_t queryCtn{ 0 };

    // Get the compacted size result back
    std::vector<vk::DeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
    p_gfx->GetDeviceRef().getQueryPoolResults(queryPool, 0, (uint32_t)compactSizes.size(), compactSizes.size() * sizeof(vk::DeviceSize),
        compactSizes.data(), sizeof(vk::DeviceSize), vk::QueryResultFlagBits::eWait);

    for (auto idx : indices)
    {
        buildAs[idx].cleanupAS = buildAs[idx].as.accel;           // previous AS to destroy
        buildAs[idx].sizeInfo.accelerationStructureSize = compactSizes[queryCtn++];  // new reduced size

        // Creating a compact version of the AS
        vk::AccelerationStructureCreateInfoKHR asCreateInfo;
        asCreateInfo.setSize(buildAs[idx].sizeInfo.accelerationStructureSize);
        asCreateInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
        buildAs[idx].as = createAcceleration(p_gfx, asCreateInfo);

        // Copy the original BLAS to a compact version
        vk::CopyAccelerationStructureInfoKHR copyInfo;
        copyInfo.setSrc(buildAs[idx].buildInfo.dstAccelerationStructure);
        copyInfo.setDst(buildAs[idx].as.accel);
        copyInfo.setMode(vk::CopyAccelerationStructureModeKHR::eCompact);
        cmdBuf.copyAccelerationStructureKHR(&copyInfo);
    }
}

void RaytracingBuilderKHR::DestroyNonCompacted(std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs) {
    printf("RaytracingBuilderKHR::destroyNonCompacted\n");
    for (auto& i : indices)
    {
        p_gfx->GetDeviceRef().destroyAccelerationStructureKHR(buildAs[i].cleanupAS, nullptr);
    }
}

void RaytracingBuilderKHR::CmdCreateTlas(vk::CommandBuffer cmdBuf, uint32_t countInstance, vk::DeviceAddress instBufferAddr, vk::BuildAccelerationStructureFlagsKHR flags, bool update, bool motion) {
    printf("RaytracingBuilderKHR::cmdCreateTlas (75)\n");
    // Wraps a device pointer to the above uploaded instances.
    vk::AccelerationStructureGeometryInstancesDataKHR instancesVk;
    instancesVk.data.deviceAddress = instBufferAddr;

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
    vk::AccelerationStructureGeometryKHR topASGeometry;
    topASGeometry.setGeometryType(vk::GeometryTypeKHR::eInstances);
    topASGeometry.geometry.instances = instancesVk;

    // Find sizes
    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo;
    buildInfo.setFlags(flags);
    buildInfo.setGeometryCount(1);
    buildInfo.setPGeometries(&topASGeometry);
    buildInfo.setMode(update ? vk::BuildAccelerationStructureModeKHR::eUpdate : vk::BuildAccelerationStructureModeKHR::eBuild);
    buildInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
    buildInfo.setSrcAccelerationStructure(VK_NULL_HANDLE);

    vk::AccelerationStructureBuildSizesInfoKHR sizeInfo;
    p_gfx->GetDeviceRef().getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &buildInfo, &countInstance, &sizeInfo);
    // Create TLAS
    if (update == false)
    {

        vk::AccelerationStructureCreateInfoKHR createInfo;
        createInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
        createInfo.setSize(sizeInfo.accelerationStructureSize);
        m_tlas = createAcceleration(p_gfx, createInfo);
    }

    // Allocate the scratch memory
    scratch_buffer = p_gfx->CreateBufferWrap(sizeInfo.buildScratchSize,
        vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::BufferDeviceAddressInfo bufferInfo;
    bufferInfo.setBuffer(scratch_buffer.buffer);
    vk::DeviceAddress scratchAddress = p_gfx->GetDeviceRef().getBufferAddress(&bufferInfo);

    // Update build information
    buildInfo.srcAccelerationStructure = update ? m_tlas.accel : VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = m_tlas.accel;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    // Build Offsets info: n instances
    vk::AccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{ countInstance, 0, 0, 0 };
    const vk::AccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

    // Build the TLAS
    cmdBuf.buildAccelerationStructuresKHR(1, &buildInfo, &pBuildOffsetInfo);
}
