#pragma once

#include <mutex>
#include <vector>
#include <vulkan/vulkan_core.h>

#if VK_KHR_acceleration_structure

#include <type_traits>
#include <string.h>

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "ImageWrap.h"
#include "BufferWrap.h"

// Convert a Mat4x4 to the matrix required by acceleration structures
inline vk::TransformMatrixKHR toTransformMatrixKHR(glm::mat4 matrix)
{
    // vk::TransformMatrixKHR uses a row-major layout, while glm::mat4
    // uses a column-major layout. So we transpose the matrix to
    // memcpy its data directly.
    glm::mat4        temp = glm::transpose(matrix);
    vk::TransformMatrixKHR out_matrix;
    memcpy(&out_matrix, &temp, sizeof(vk::TransformMatrixKHR));
    return out_matrix;
}

struct WrapAccelerationStructure
{
    vk::AccelerationStructureKHR accel;
    BufferWrap bw;
};


// Inputs used to build Bottom-level acceleration structure.
// You manage the lifetime of the buffer(s) referenced by the VkAccelerationStructureGeometryKHRs within.
// In particular, you must make sure they are still valid and not being modified when the BLAS is built or updated.
struct BlasInput
{
    // Data used to build acceleration structure geometry
    std::vector<vk::AccelerationStructureGeometryKHR>       asGeometry;
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
    vk::BuildAccelerationStructureFlagsKHR                  flags{ 0 };
};


class Graphics;
// Ray tracing BLAS and TLAS builder
class RaytracingBuilderKHR
{
public:
    Graphics* p_gfx;
    // Initializing the allocator and querying the raytracing properties
    RaytracingBuilderKHR(Graphics* _p_gfx);
    ~RaytracingBuilderKHR();

    // Returning the constructed top-level acceleration structure
    const vk::AccelerationStructureKHR& GetAccelerationStructure() const;

    // Return the Acceleration Structure Device Address of a BLAS Id
    vk::DeviceAddress GetBlasDeviceAddress(uint32_t blasId);

    // Create all the BLAS from the vector of BlasInput
    void BuildBlas(const std::vector<BlasInput>& input,
        vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);

    // Build TLAS from an array of VkAccelerationStructureInstanceKHR
    // - Use motion=true with VkAccelerationStructureMotionInstanceNV
    // - The resulting TLAS will be stored in m_tlas
    // - update is to rebuild the Tlas with updated matrices, flag must have the 'allow_update'

    void BuildTlas(const std::vector<vk::AccelerationStructureInstanceKHR>& instances,
        vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
        bool                                 update = false,
        bool                                 motion = false);


protected:
    BufferWrap scratch_buffer;

    std::vector<WrapAccelerationStructure> m_blas;  // Bottom-level acceleration structure
    WrapAccelerationStructure              m_tlas;  // Top-level acceleration structure

    struct BuildAccelerationStructure
    {
        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo;
        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo;
        const vk::AccelerationStructureBuildRangeInfoKHR* rangeInfo;
        WrapAccelerationStructure as;  // result acceleration structure
        vk::AccelerationStructureKHR cleanupAS;
    };


    void CmdCreateBlas(vk::CommandBuffer                          cmdBuf,
        std::vector<uint32_t>                    indices,
        std::vector<BuildAccelerationStructure>& buildAs,
        vk::DeviceAddress                          scratchAddress,
        vk::QueryPool                              queryPool);
    void CmdCompactBlas(vk::CommandBuffer cmdBuf, std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs, vk::QueryPool queryPool);
    void DestroyNonCompacted(std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs);
    
    // Creating the TLAS, called by buildTlas
    void CmdCreateTlas(vk::CommandBuffer                      cmdBuf,          // Command buffer
        uint32_t                             countInstance,   // number of instances
        vk::DeviceAddress                      instBufferAddr,  // Buffer address of instances
        vk::BuildAccelerationStructureFlagsKHR flags,           // Build creation flag
        bool                                 update,          // Update == animation
        bool                                 motion           // Motion Blur
    );

    bool HasFlag(VkFlags item, VkFlags flag) { return (item & flag) == flag; }
    bool HasFlag(vk::BuildAccelerationStructureFlagsKHR item, vk::BuildAccelerationStructureFlagsKHR flag) { return (item & flag) == flag; }
};


#else
#error This include requires VK_KHR_acceleration_structure support in the Vulkan SDK.
#endif

