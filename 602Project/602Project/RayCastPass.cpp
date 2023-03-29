#include "ImageWrap.h"
#include "DescriptorWrap.h"
#include "Graphics.h"

#include "RayCastPass.h"
#include "RayMaskPass.h"
#include "LightingPass.h"
#include "DOFPass.h"
#include "Camera.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// Convert an OBJ model into the ray tracing geometry used to build the BLAS
//
BlasInput ObjectToVkGeometryKHR(const ObjData& model, const vk::Device& device) {
    //printf("VkApp::objectToVkGeometryKHR (45)\n");
    // BLAS builder requires raw device addresses.
    vk::BufferDeviceAddressInfo _b1;
    _b1.setBuffer(model.vertexBuffer.buffer);
    vk::DeviceAddress vertexAddress = device.getBufferAddress(&_b1);
    //vk::DeviceAddress vertexAddress = vkGetBufferDeviceAddress(m_device, &_b1);

    vk::BufferDeviceAddressInfo _b2;
    _b2.setBuffer(model.indexBuffer.buffer);
    vk::DeviceAddress indexAddress = device.getBufferAddress(&_b2);
    //vk::DeviceAddress indexAddress  = vkGetBufferDeviceAddress(m_device, &_b2);

    uint32_t maxPrimitiveCount = model.nbIndices / 3;

    // Describe buffer as array of Vertex.
    vk::AccelerationStructureGeometryTrianglesDataKHR triangles;
    triangles.setVertexFormat(vk::Format::eR32G32B32A32Sfloat);  // vec3 vertex position data.
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.setVertexStride(sizeof(Vertex));
    // Describe index data (32-bit unsigned int)
    triangles.setIndexType(vk::IndexType::eUint32);
    triangles.indexData.deviceAddress = indexAddress;
    // Indicate identity transform by setting transformData to null device pointer.
    //triangles.transformData = {};
    triangles.setMaxVertex(model.nbVertices);

    // Identify the above data as containing opaque triangles.
    vk::AccelerationStructureGeometryKHR asGeom;
    asGeom.setGeometryType(vk::GeometryTypeKHR::eTriangles);
    asGeom.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
    asGeom.geometry.triangles = triangles;

    // The entire array will be used to build the BLAS.
    vk::AccelerationStructureBuildRangeInfoKHR offset;
    offset.firstVertex = 0;
    offset.primitiveCount = maxPrimitiveCount;
    offset.primitiveOffset = 0;
    offset.transformOffset = 0;

    // Our blas is made from only one geometry, but could be made of many geometries
    BlasInput input;
    input.asGeometry.emplace_back(asGeom);
    input.asBuildOffsetInfo.emplace_back(offset);

    return input;
}

void RayCastPass::SetupBuffer() {
    m_buffer_bg.CreateTextureSampler();
    m_buffer_bg.TransitionImageLayout(vk::ImageLayout::eGeneral);
    m_buffer_bg_prev.CreateTextureSampler();
    m_buffer_bg_prev.TransitionImageLayout(vk::ImageLayout::eGeneral);
    m_buffer_nd.CreateTextureSampler();
    m_buffer_nd.TransitionImageLayout(vk::ImageLayout::eGeneral);
    m_buffer_nd_prev.CreateTextureSampler();
    m_buffer_nd_prev.TransitionImageLayout(vk::ImageLayout::eGeneral);
}

void RayCastPass::CreateRaytraceAS() {
    //printf("VkApp::createRtAccelerationStructure (25)\n");
    // BLAS - Storing each primitive in a geometry
    std::vector<BlasInput> allBlas;
    allBlas.reserve(p_gfx->m_objData.size());
    for (const auto& obj : p_gfx->m_objData) {
        BlasInput blas = ObjectToVkGeometryKHR(obj, p_gfx->GetDeviceRef());
        // We could add more geometry in each BLAS, but we add only one for now
        allBlas.emplace_back(blas);
    }
    m_rt_builder.BuildBlas(allBlas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);

    // TLAS 
    std::vector<vk::AccelerationStructureInstanceKHR> tlas;
    tlas.reserve(p_gfx->m_objInst.size());
    for (const ObjInst& inst : p_gfx->m_objInst) {
        vk::AccelerationStructureInstanceKHR _i{};
        _i.transform = toTransformMatrixKHR(inst.transform);  // Position of the instance
        _i.instanceCustomIndex = inst.objIndex;
        _i.accelerationStructureReference = m_rt_builder.GetBlasDeviceAddress(inst.objIndex);
        _i.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        _i.mask = 0xFF;       //  Only be hit if rayMask & instance.mask != 0
        _i.instanceShaderBindingTableRecordOffset = 0; // Use the same hit group for all objects
        tlas.emplace_back(_i);
    }

    m_rt_builder.BuildTlas(
        tlas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
        false, false);
}

void RayCastPass::SetupDescriptor() {
    auto device = p_gfx->GetDeviceRef();
    m_descriptor.setBindings(device, {
        {0, vk::DescriptorType::eAccelerationStructureKHR, 1,
         vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR},
        {1, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eRaygenKHR},
        {2, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eRaygenKHR}, 
        {3, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eRaygenKHR},
        {4, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eRaygenKHR},
        {5, vk::DescriptorType::eStorageImage, 1,
         vk::ShaderStageFlagBits::eRaygenKHR}
        });
}

void RayCastPass::SetupPipeline() {
    ////////////////////////////////////////////////////////////////////////////////////////////
    // stages: Array of shaders: 1 raygen, 1 miss, 1 hit (later: an additional hit/miss pair.)

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Group the shaders.  Raygen and miss shaders get their own
    // groups. Hit shaders can group with any-hit and intersection
    // shaders -- of which we have none -- so the hit shader(s) get
    // their own group also.
    std::vector<vk::PipelineShaderStageCreateInfo> stages{};
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups{};

    vk::PipelineShaderStageCreateInfo stage;
    stage.setPName("main");  // All the same entry point

    vk::RayTracingShaderGroupCreateInfoKHR group;
    group.anyHitShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;

    // Raygen shader stage and group appended to stages and groups lists
    stage.module = p_gfx->CreateShaderModule(LoadFileIntoString("spv/raytrace.rgen.spv"));
    stage.setStage(vk::ShaderStageFlagBits::eRaygenKHR);
    stages.push_back(stage);

    group.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
    group.generalShader = stages.size() - 1;    // Index of raygen shader
    groups.push_back(group);
    group.generalShader = VK_SHADER_UNUSED_KHR;

    // Miss shader stage and group appended to stages and groups lists
    stage.module = p_gfx->CreateShaderModule(LoadFileIntoString("spv/raytrace.rmiss.spv"));
    stage.setStage(vk::ShaderStageFlagBits::eMissKHR);
    stages.push_back(stage);

    group.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
    group.generalShader = stages.size() - 1;    // Index of miss shader
    groups.push_back(group);
    group.generalShader = VK_SHADER_UNUSED_KHR;

    // Closest hit shader stage and group appended to stages and groups lists
    stage.module = p_gfx->CreateShaderModule(LoadFileIntoString("spv/raytrace.rchit.spv"));
    stage.setStage(vk::ShaderStageFlagBits::eClosestHitKHR);
    stages.push_back(stage);

    group.setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
    group.closestHitShader = stages.size() - 1;   // Index of hit shader
    groups.push_back(group);

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Create the ray tracing pipeline layout.
    // Push constant: we want to be able to update constants used by the shaders
    vk::PushConstantRange pushConstant;
    pushConstant.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR | 
                               vk::ShaderStageFlagBits::eClosestHitKHR | 
                               vk::ShaderStageFlagBits::eMissKHR);
    pushConstant.setOffset(0);
    pushConstant.setSize(sizeof(PushConstantRay));

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

    // Descriptor sets: one specific to ray tracing, and one shared with the rasterization pipeline
    std::vector<vk::DescriptorSetLayout> rtDescSetLayouts =
    { m_descriptor.descSetLayout, lighting_pass_desc_layout };
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(rtDescSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = rtDescSetLayouts.data();

    m_pipeline_layout = p_gfx->GetDeviceRef().createPipelineLayout(pipelineLayoutCreateInfo);

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Create the ray tracing pipeline.
    // Assemble the shader stages and recursion depth info into the ray tracing pipeline
    vk::RayTracingPipelineCreateInfoKHR rayPipelineInfo;
    rayPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());  // Stages are shaders
    rayPipelineInfo.pStages = stages.data();

    rayPipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    rayPipelineInfo.pGroups = groups.data();

    rayPipelineInfo.maxPipelineRayRecursionDepth = 10;  // Ray depth
    rayPipelineInfo.layout = m_pipeline_layout;

    if (p_gfx->GetDeviceRef().createRayTracingPipelinesKHR({}, {}, 1, &rayPipelineInfo, nullptr, &m_pipeline)
        != vk::Result::eSuccess) {
        printf("Failed to create RT pipeline for Raycast pass\n");
    }
    for (auto& s : stages)
        p_gfx->GetDeviceRef().destroyShaderModule(s.module, nullptr);
}

template <class integral>
constexpr integral align_up(integral x, size_t a) noexcept
{
    return integral((x + (integral(a) - 1)) & ~integral(a - 1));
}

//--------------------------------------------------------------------------------------------------
// The Shader Binding Table (SBT)
// - getting all shader handles and write them in a SBT buffer
//
void RayCastPass::CreateRtShaderBindingTable() {
    uint32_t missCount{ 1 };
    uint32_t hitCount{ 1 };
    auto     handleCount = 1 + missCount + hitCount;

    auto allRtProps = p_gfx->GetPhysicalDeviceRef().getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rtPLProps = allRtProps.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    uint32_t handleSize = rtPLProps.shaderGroupHandleSize;
    uint32_t handleAlignment = rtPLProps.shaderGroupHandleAlignment;
    uint32_t baseAlignment = rtPLProps.shaderGroupBaseAlignment;

    // The SBT (buffer) needs to have starting group to be aligned
    // and handles in the group to be aligned.
    uint32_t handleSizeAligned = align_up(handleSize, handleAlignment);  // handleAlignment==32

    m_rgen_region.stride = align_up(handleSizeAligned, baseAlignment); //baseAlignment==64
    m_rgen_region.size = m_rgen_region.stride;  // The size member must be equal to its stride member

    m_miss_region.stride = handleSizeAligned;
    m_miss_region.size = align_up(missCount * handleSizeAligned, baseAlignment);

    m_hit_region.stride = handleSizeAligned;
    m_hit_region.size = align_up(hitCount * handleSizeAligned, baseAlignment);

    printf("Shader binding table:\n");
    printf("  alignments:\n");
    printf("    handleAlignment: %d\n", handleAlignment);
    printf("    baseAlignment:   %d\n", baseAlignment);
    printf("  counts:\n");
    printf("    miss:   %d\n", missCount);
    printf("    hit:    %d\n", hitCount);
    printf("    handle: %d = 1+missCount+hitCount\n", handleCount);
    printf("  regions stride:size:\n");
    printf("    rgen %2ld:%2ld\n", m_rgen_region.stride, m_rgen_region.size);
    printf("    miss %2ld:%2ld\n", m_miss_region.stride, m_miss_region.size);
    printf("    hit  %2ld:%2ld\n", m_hit_region.stride, m_hit_region.size);
    printf("    call %2ld:%2ld\n", m_call_region.stride, m_call_region.size);

    // Get the shader group handles.  This is a byte array retrieved
    // from the pipeline.
    uint32_t             dataSize = handleCount * handleSize;
    std::vector<uint8_t> handles(dataSize);
    auto result = p_gfx->GetDeviceRef().getRayTracingShaderGroupHandlesKHR(m_pipeline,
        0, handleCount, dataSize, handles.data());
    assert(result == vk::Result::eSuccess);

    // Allocate a buffer for storing the SBT, and a staging buffer for transferring data to it.
    vk::DeviceSize sbtSize = m_rgen_region.size + m_miss_region.size
        + m_hit_region.size + m_call_region.size;

    BufferWrap staging = p_gfx->CreateBufferWrap(sbtSize, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible
        | vk::MemoryPropertyFlagBits::eHostCoherent);

    m_shaderBindingTableBW = p_gfx->CreateBufferWrap(sbtSize,
        vk::BufferUsageFlagBits::eTransferDst
        | vk::BufferUsageFlagBits::eShaderDeviceAddress
        | vk::BufferUsageFlagBits::eShaderBindingTableKHR,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    // Find the SBT addresses of each group
    vk::BufferDeviceAddressInfo info;
    info.buffer = m_shaderBindingTableBW.buffer;
    vk::DeviceAddress sbtAddress = p_gfx->GetDeviceRef().getBufferAddress(&info);

    m_rgen_region.deviceAddress = sbtAddress;
    m_miss_region.deviceAddress = sbtAddress + m_rgen_region.size;
    m_hit_region.deviceAddress = sbtAddress + m_rgen_region.size + m_miss_region.size;

    // Helper to retrieve the handle data
    auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

    // Map the SBT buffer and write in the handles.
    uint8_t* mappedMemAddress;
    p_gfx->GetDeviceRef().mapMemory(staging.memory, 0, sbtSize, vk::MemoryMapFlags(), (void**)&mappedMemAddress);
    uint8_t offset = 0;

    // Raygen
    uint32_t handleIdx{ 0 };
    memcpy(mappedMemAddress + offset, getHandle(handleIdx++), handleSize);

    // Miss
    offset = m_rgen_region.size;
    for (uint32_t c = 0; c < missCount; c++) {
        memcpy(mappedMemAddress + offset, getHandle(handleIdx++), handleSize);
        offset += m_miss_region.stride;
    }

    // Hit
    offset = m_rgen_region.size + m_miss_region.size;
    for (uint32_t c = 0; c < hitCount; c++) {
        memcpy(mappedMemAddress + offset, getHandle(handleIdx++), handleSize);
        offset += m_hit_region.stride;
    }

    p_gfx->GetDeviceRef().unmapMemory(staging.memory);

    p_gfx->CopyBuffer(staging.buffer, m_shaderBindingTableBW.buffer, sbtSize);

    staging.destroy(p_gfx->GetDeviceRef());
}

RayCastPass::RayCastPass(Graphics* _p_gfx, RenderPass* _p_prev_pass) : 
    RenderPass(_p_gfx, _p_prev_pass), 
    m_buffer_bg(p_gfx->GetWindowSize().x / 2, p_gfx->GetWindowSize().y / 2,
        vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        1, p_gfx),
    m_buffer_bg_prev(p_gfx->GetWindowSize().x / 2, p_gfx->GetWindowSize().y / 2,
        vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        1, p_gfx),
    m_buffer_nd(p_gfx->GetWindowSize().x / 2, p_gfx->GetWindowSize().y / 2,
        vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        1, p_gfx),
    m_buffer_nd_prev(p_gfx->GetWindowSize().x / 2, p_gfx->GetWindowSize().y / 2,
        vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        1, p_gfx),
    m_rt_builder(_p_gfx), enabled(true) {

    m_push_consts.ray_count_factor = 10;
    m_push_consts.clear = 0;

    CreateRaytraceAS();
    SetupBuffer();
    SetupDescriptor();
}

RayCastPass::~RayCastPass() {
    p_gfx->GetDeviceRef().destroyPipelineLayout(m_pipeline_layout);
    p_gfx->GetDeviceRef().destroyPipeline(m_pipeline);
    m_descriptor.destroy(p_gfx->GetDeviceRef());
    
    m_buffer_bg.destroy(p_gfx->GetDeviceRef());
    m_buffer_bg_prev.destroy(p_gfx->GetDeviceRef());
    m_buffer_nd.destroy(p_gfx->GetDeviceRef());
    m_buffer_nd_prev.destroy(p_gfx->GetDeviceRef());
    m_shaderBindingTableBW.destroy(p_gfx->GetDeviceRef());
}
 
void RayCastPass::Setup(){
    raymask_buffer_desc = static_cast<DOFPass*>(p_dof_pass)->GetRaymaskBuffer().Descriptor();

    auto device = p_gfx->GetDeviceRef();
    m_descriptor.write(device, 0, m_rt_builder.GetAccelerationStructure());
    m_descriptor.write(device, 1, m_buffer_bg.Descriptor());
    m_descriptor.write(device, 2, m_buffer_bg_prev.Descriptor());
    m_descriptor.write(device, 3, raymask_buffer_desc);
    m_descriptor.write(device, 4, m_buffer_nd.Descriptor());
    m_descriptor.write(device, 5, m_buffer_nd_prev.Descriptor());

    lighting_pass_desc_layout = p_lighting_pass->GetDescriptor().descSetLayout;
    lighting_pass_desc_set = p_lighting_pass->GetDescriptor().descSet;

    SetupPipeline();
    CreateRtShaderBindingTable();
}

void RayCastPass::Render() {
    if (not enabled)
        return;

    if (p_gfx->GetCamera()->WasUpdated())
        m_push_consts.clear = true;

    // The push constants for the ray tracing pipeline.
    m_push_consts.alignmentTest = 1234;

    m_push_consts.lightPosition = p_lighting_pass->GetPCParams().lightPosition;
    m_push_consts.lightIntensity = p_lighting_pass->GetPCParams().lightIntensity;
    m_push_consts.ambientIntensity = p_lighting_pass->GetPCParams().ambientIntensity;

    m_push_consts.focal_distance = p_dof_pass->GetDOFParams().focal_distance;
    m_push_consts.focal_length = p_dof_pass->GetDOFParams().focal_length;
    m_push_consts.lens_diameter = p_dof_pass->GetDOFParams().lens_diameter;
    m_push_consts.coc_sample_scale = p_dof_pass->GetDOFParams().coc_sample_scale;
    m_push_consts.soft_z_extent = p_dof_pass->GetDOFParams().soft_z_extent;

    m_push_consts.frameSeed = rand() % 32768;

    // Bind the ray tracing pipeline
    auto cmd_buff = p_gfx->GetCommandBuffer();
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, m_pipeline);

    // Bind the descriptor sets (the ray tracing specific one, and the
    // full model descriptor)
    std::vector<vk::DescriptorSet> descSets{ m_descriptor.descSet , lighting_pass_desc_set };
    cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR,
        m_pipeline_layout, 0, (uint32_t)descSets.size(),
        descSets.data(), 0, nullptr);

    // Push the push constants
    cmd_buff.pushConstants(m_pipeline_layout,
        vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR,
        0, sizeof(PushConstantRay), &m_push_consts);

    cmd_buff.traceRaysKHR(
        &m_rgen_region, &m_miss_region, &m_hit_region, &m_call_region, 
        p_gfx->GetWindowExtent().width/2, p_gfx->GetWindowExtent().height/2, 1);

    p_gfx->CommandCopyImage(m_buffer_bg, m_buffer_bg_prev);
    p_gfx->CommandCopyImage(m_buffer_nd, m_buffer_nd_prev);

    m_push_consts.clear = 0;
}

void RayCastPass::Teardown() {

}

void RayCastPass::DrawGUI() {
    ImGui::Checkbox("Raycast enabled", &enabled);
    ImGui::SliderInt("Raycast count factor", &m_push_consts.ray_count_factor, 1, 50);

    if (ImGui::Button("Reset accumulation"))
        m_push_consts.clear = 1;
}

void RayCastPass::SetLightingPass(LightingPass* _p_lighting_pass) {
    p_lighting_pass = _p_lighting_pass;
}

void RayCastPass::SetDOFPass(DOFPass* _p_dof_pass) {
    p_dof_pass = _p_dof_pass;
}

const ImageWrap& RayCastPass::GetBGBuffer() const {
    return m_buffer_bg;
}