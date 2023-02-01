//////////////////////////////////////////////////////////////////////
// Uses the ASSIMP library to read mesh models in of 30+ file types
// into a structure suitable for the raytracer.
////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <math.h>

#include <filesystem>
namespace fs = std::filesystem;

#include "Graphics.h"

#include <assimp/Importer.hpp>
#include <assimp/version.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
using namespace glm;

#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "app.h"
#include "shaders/shared_structs.h"

// Local objects and procedures defined and used here:
struct ModelData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indicies;
    std::vector<Material> materials;
    std::vector<int32_t>     matIndx;
    std::vector<std::string> textures;

    void readAssimpFile(const std::string& path, const mat4& M);
};

void recurseModelNodes(ModelData* meshdata,
    const  aiScene* aiscene,
    const  aiNode* node,
    const aiMatrix4x4& parentTr,
    const int level = 0);


// Returns an address (as VkDeviceAddress=uint64_t) of a buffer on the GPU.
VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = buffer;
    return vkGetBufferDeviceAddress(device, &info);
}

vk::DeviceAddress getBufferDeviceAddress(vk::Device device, vk::Buffer buffer) {
    vk::BufferDeviceAddressInfo info;
    info.setBuffer(buffer);

    return device.getBufferAddress(&info);
}

void Graphics::LoadModel(const std::string& filename, glm::mat4 transform)
{
    ModelData meshdata;
    meshdata.readAssimpFile(filename.c_str(), glm::mat4());

    printf("vertices: %ld\n", meshdata.vertices.size());
    printf("indices: %ld (%ld)\n", meshdata.indicies.size(), meshdata.indicies.size() / 3);
    printf("materials: %ld\n", meshdata.materials.size());
    printf("matIndx: %ld\n", meshdata.matIndx.size());
    printf("textures: %ld\n", meshdata.textures.size());

    std::vector<Emitter> emitterList;
    Emitter tempEmitter;
    std::vector<uint32_t> lightTriangleIndeces;
    vec3 tempCross;
    for (int i = 0; i < meshdata.matIndx.size(); i++) {
        if (meshdata.materials[meshdata.matIndx[i]].emission.r > 0 ||
            meshdata.materials[meshdata.matIndx[i]].emission.g > 0 ||
            meshdata.materials[meshdata.matIndx[i]].emission.b > 0) {
            //Found an emittor
            //Scale the emission by a factor of 5
            meshdata.materials[meshdata.matIndx[i]].emission *= 5;

            //Store the index of the emittor 
            lightTriangleIndeces.push_back(i);

            tempEmitter.v0 = meshdata.vertices[meshdata.indicies[3 * i + 0]].pos;
            tempEmitter.v1 = meshdata.vertices[meshdata.indicies[3 * i + 1]].pos;
            tempEmitter.v2 = meshdata.vertices[meshdata.indicies[3 * i + 2]].pos;
            tempEmitter.emission = meshdata.materials[meshdata.matIndx[i]].emission;
            tempEmitter.index = i;
            tempCross = glm::cross((tempEmitter.v1 - tempEmitter.v0), (tempEmitter.v2 - tempEmitter.v0));
            tempEmitter.normal = glm::normalize(tempCross);
            tempEmitter.area = glm::length(tempCross) / 2;
            emitterList.push_back(tempEmitter);
        }
    }

    ObjData object;
    object.nbIndices = static_cast<uint32_t>(meshdata.indicies.size());
    object.nbVertices = static_cast<uint32_t>(meshdata.vertices.size());

    // Create the buffers on Device and copy vertices, indices and materials
    vk::CommandBuffer cmdBuf = CreateTempCommandBuffer();

    vk::BufferUsageFlags flag = vk::BufferUsageFlagBits::eStorageBuffer |
        vk::BufferUsageFlagBits::eShaderDeviceAddress;

    vk::BufferUsageFlags rtFlags = flag | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;

    object.vertexBuffer = CreateStagedBufferWrap(cmdBuf, meshdata.vertices,
        vk::BufferUsageFlagBits::eVertexBuffer | rtFlags);
    object.indexBuffer = CreateStagedBufferWrap(cmdBuf, meshdata.indicies,
        vk::BufferUsageFlagBits::eIndexBuffer | rtFlags);
    object.matColorBuffer = CreateStagedBufferWrap(cmdBuf, meshdata.materials, flag);
    object.matIndexBuffer = CreateStagedBufferWrap(cmdBuf, meshdata.matIndx, flag);
    object.lightBuffer = CreateStagedBufferWrap(cmdBuf, lightTriangleIndeces, flag);

    SubmitTempCommandBuffer(cmdBuf);

    //Create buffer for the emitter list and send it
    cmdBuf = CreateTempCommandBuffer();
    m_lightBW = CreateBufferWrap(sizeof(emitterList[0]) * emitterList.size(),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    cmdBuf.updateBuffer(m_lightBW.buffer, 0,
        sizeof(emitterList[0]) * emitterList.size(), emitterList.data());

    SubmitTempCommandBuffer(cmdBuf);

    // Creates all textures on the GPU
    auto txtOffset = static_cast<uint32_t>(m_objText.size());  // Offset is current size
    for (const auto& texName : meshdata.textures)
        m_objText.push_back(CreateTextureImage(texName));

    // Assuming one instance of an object with its supplied transform.
    // Could provide multiple transform here to make a vector of instances of this object.
    ObjInst instance;
    instance.transform = transform;
    instance.objIndex = static_cast<uint32_t>(m_objData.size()); // Index of current object
    m_objInst.push_back(instance);

    // Creating information for device access
    ObjDesc desc;
    desc.txtOffset = txtOffset;
    desc.vertexAddress = getBufferDeviceAddress(m_device, object.vertexBuffer.buffer);
    desc.indexAddress = getBufferDeviceAddress(m_device, object.indexBuffer.buffer);
    desc.materialAddress = getBufferDeviceAddress(m_device, object.matColorBuffer.buffer);
    desc.materialIndexAddress = getBufferDeviceAddress(m_device, object.matIndexBuffer.buffer);
    desc.lightTriangleIndexAddress = getBufferDeviceAddress(m_device, object.lightBuffer.buffer);

    m_objData.emplace_back(object);
    m_objDesc.emplace_back(desc);
}

void ModelData::readAssimpFile(const std::string& path, const mat4& M)
{
    printf("ReadAssimpFile File:  %s \n", path.c_str());

    aiMatrix4x4 modelTr(M[0][0], M[1][0], M[2][0], M[3][0],
        M[0][1], M[1][1], M[2][1], M[3][1],
        M[0][2], M[1][2], M[2][2], M[3][2],
        M[0][3], M[1][3], M[2][3], M[3][3]);

    // Does the file exist?
    std::ifstream find_it(path.c_str());
    if (find_it.fail()) {
        std::cerr << "File not found: " << path << std::endl;
        exit(-1);
    }

    // Invoke assimp to read the file.
    printf("Assimp %d.%d Reading %s\n", aiGetVersionMajor(), aiGetVersionMinor(), path.c_str());
    Assimp::Importer importer;
    const aiScene* aiscene = importer.ReadFile(path.c_str(),
        aiProcess_Triangulate | aiProcess_GenSmoothNormals);

    if (!aiscene) {
        printf("... Failed to read.\n");
        exit(-1);
    }

    if (!aiscene->mRootNode) {
        printf("Scene has no rootnode.\n");
        exit(-1);
    }

    printf("Assimp mNumMeshes: %d\n", aiscene->mNumMeshes);
    printf("Assimp mNumMaterials: %d\n", aiscene->mNumMaterials);
    printf("Assimp mNumTextures: %d\n", aiscene->mNumTextures);

    for (int i = 0; i < aiscene->mNumMaterials; i++) {
        aiMaterial* mtl = aiscene->mMaterials[i];
        aiString name;
        mtl->Get(AI_MATKEY_NAME, name);
        aiColor3D emit(0.f, 0.f, 0.f);
        aiColor3D diff(0.f, 0.f, 0.f), spec(0.f, 0.f, 0.f);
        float alpha = 20.0;
        bool he = mtl->Get(AI_MATKEY_COLOR_EMISSIVE, emit);
        bool hd = mtl->Get(AI_MATKEY_COLOR_DIFFUSE, diff);
        bool hs = mtl->Get(AI_MATKEY_COLOR_SPECULAR, spec);
        bool ha = mtl->Get(AI_MATKEY_SHININESS, &alpha, NULL);
        aiColor3D trans;
        bool ht = mtl->Get(AI_MATKEY_COLOR_TRANSPARENT, trans);

        Material newmat;
        if (!emit.IsBlack()) { // An emitter
            newmat.diffuse = { 1,1,1 };  // An emitter needs (1,1,1), else black screen!  WTF???
            newmat.specular = { 0,0,0 };
            newmat.shininess = 0.0;
            newmat.emission = { emit.r, emit.g, emit.b };
            newmat.textureId = -1;
        }

        else {
            vec3 Kd(0.5f, 0.5f, 0.5f);
            vec3 Ks(0.03f, 0.03f, 0.03f);
            if (AI_SUCCESS == hd) Kd = vec3(diff.r, diff.g, diff.b);
            if (AI_SUCCESS == hs) Ks = vec3(spec.r, spec.g, spec.b);
            newmat.diffuse = { Kd[0], Kd[1], Kd[2] };
            newmat.specular = { Ks[0], Ks[1], Ks[2] };
            newmat.shininess = alpha; //sqrtf(2.0f/(2.0f+alpha));
            newmat.emission = { 0,0,0 };
            newmat.textureId = -1;
        }

        aiString texPath;
        if (AI_SUCCESS == mtl->GetTexture(aiTextureType_DIFFUSE, 0, &texPath)) {
            fs::path fullPath = path;
            fullPath.replace_filename(texPath.C_Str());
            newmat.textureId = textures.size();
            auto xxx = fullPath.u8string();
            textures.push_back(std::string(xxx));
        }

        materials.push_back(newmat);
    }

    recurseModelNodes(this, aiscene, aiscene->mRootNode, modelTr);

}

// Recursively traverses the assimp node hierarchy, accumulating
// modeling transformations, and creating and transforming any meshes
// found.  Meshes comming from assimp can have associated surface
// properties, so each mesh *copies* the current BRDF as a starting
// point and modifies it from the assimp data structure.
void recurseModelNodes(ModelData* meshdata,
    const aiScene* aiscene,
    const aiNode* node,
    const aiMatrix4x4& parentTr,
    const int level)
{
    // Print line with indentation to show structure of the model node hierarchy.
    //for (int i=0;  i<level;  i++) printf("| ");
    //printf("%s \n", node->mName.data);

    // Accumulating transformations while traversing down the hierarchy.
    aiMatrix4x4 childTr = parentTr * node->mTransformation;
    aiMatrix3x3 normalTr = aiMatrix3x3(childTr); // Really should be inverse-transpose for full generality

    // Loop through this node's meshes
    for (unsigned int m = 0; m < node->mNumMeshes; ++m) {
        aiMesh* aimesh = aiscene->mMeshes[node->mMeshes[m]];
        //printf("  %d: %d:%d\n", m, aimesh->mNumVertices, aimesh->mNumFaces);

        // Loop through all vertices and record the
        // vertex/normal/texture/tangent data with the node's model
        // transformation applied.
        uint faceOffset = meshdata->vertices.size();
        for (unsigned int t = 0; t < aimesh->mNumVertices; ++t) {
            aiVector3D aipnt = childTr * aimesh->mVertices[t];
            aiVector3D ainrm = aimesh->HasNormals() ? normalTr * aimesh->mNormals[t] : aiVector3D(0, 0, 1);
            aiVector3D aitex = aimesh->HasTextureCoords(0) ? aimesh->mTextureCoords[0][t] : aiVector3D(0, 0, 0);
            aiVector3D aitan = aimesh->HasTangentsAndBitangents() ? normalTr * aimesh->mTangents[t] : aiVector3D(1, 0, 0);


            meshdata->vertices.push_back({ {aipnt.x, aipnt.y, aipnt.z},
                                          {ainrm.x, ainrm.y, ainrm.z},
                                          {aitex.x, aitex.y} });
        }

        // Loop through all faces, recording indices
        for (unsigned int t = 0; t < aimesh->mNumFaces; ++t) {
            aiFace* aiface = &aimesh->mFaces[t];
            for (int i = 2; i < aiface->mNumIndices; i++) {
                meshdata->matIndx.push_back(aimesh->mMaterialIndex);
                meshdata->indicies.push_back(aiface->mIndices[0] + faceOffset);
                meshdata->indicies.push_back(aiface->mIndices[i - 1] + faceOffset);
                meshdata->indicies.push_back(aiface->mIndices[i] + faceOffset);
            }
        };
    }


    // Recurse onto this node's children
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
        recurseModelNodes(meshdata, aiscene, node->mChildren[i], childTr, level + 1);
}

ImageWrap Graphics::CreateTextureImage(std::string fileName) {
    int texWidth, texHeight, texChannels;

    stbi_set_flip_vertically_on_load(true);
    stbi_uc* pixels = stbi_load(fileName.c_str(), &texWidth, &texHeight, &texChannels,
        STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    BufferWrap staging = CreateBufferWrap(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible
        | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data;
    m_device.mapMemory(staging.memory, 0, imageSize, vk::MemoryMapFlags(), &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    m_device.unmapMemory(staging.memory);

    stbi_image_free(pixels);

    uint mipLevels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
    /*
    ImageWrap myImage = createImageWrap(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                  | VK_IMAGE_USAGE_SAMPLED_BIT
                                  | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  mipLevels);*/

    ImageWrap myImage(texWidth, texHeight, vk::Format::eR8G8B8A8Unorm,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        mipLevels, this);

    TransitionImageLayout(myImage.image, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal, mipLevels);
    CopyBufferToImage(staging.buffer, myImage.image, static_cast<uint32_t>(texWidth),
        static_cast<uint32_t>(texHeight));

    staging.destroy(m_device);

    GenerateMipmaps(myImage.image, vk::Format::eR8G8B8A8Unorm, texWidth, texHeight, mipLevels);

    vk::ImageAspectFlags aspect;
    myImage.sampler = CreateTextureSampler();
    myImage.image_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    return myImage;
}