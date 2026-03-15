#pragma once
#include "Tiny_GLTF/tiny_gltf.h"
#include "vkutilities.h"
#include <filesystem>

inline constexpr uint32_t kMaxMaterialCount = 128;
inline constexpr uint32_t kDefaultMaterialIndex = kMaxMaterialCount - 1u;
inline constexpr uint32_t kMaxMaterialIndex = kDefaultMaterialIndex - 1u;

inline constexpr uint32_t kInvalidId = ~0u;

class imp::Engine;

namespace SceneLoader
{
    struct Entity
    {
        uint32_t id             = kInvalidId;
        uint32_t meshId         = kInvalidId;
        uint32_t transformId    = kInvalidId;
        uint32_t materialId     = kInvalidId;
    };

    struct Mesh
    {
        uint32_t id             = kInvalidId;
        uint32_t vertexOffset;
        uint32_t indexOffset;
        uint32_t vertexCount;
        uint32_t indexCount;
    };

    struct Image
    {
        uint32_t id;
        uint32_t width;
        uint32_t height;
        std::string name;
        VU::Image image;
    };

    struct Camera
    {
        glm::mat4x4 Projection;
        glm::mat4x4 View;
		glm::mat4x4 Model;
    };

    struct ImageCreationRequest
    {
        uint32_t id;
        uint32_t width;
		uint32_t height;
		std::string name;
		std::vector<uint8_t> image;
    };

    struct SamplerCreationRequest
    {
        uint32_t id;
        std::string name;
        int magFilter;
        int minFilter;
        int wrapS;
        int wrapT;
        int wrapR;
    };

    struct TextureCreationRequest
    {
        uint32_t id;
        std::string name;
        uint32_t imageId;
        uint32_t samplerId;
    };

    struct MaterialCreationRequest
    {
        uint32_t id;
        std::string name;
        uint32_t baseColorTextureId;
        uint32_t normalTextureId;
        uint32_t metallicRoughnessTextureId;
        glm::vec4 baseColorFactor;
        float metallicFactor;
        float roughnessFactor;
    };

    struct MeshCreationRequest
    {
        uint32_t id;
        uint32_t materialId;
        std::vector<VU::Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    struct CreationRequests
    {
        std::vector<ImageCreationRequest> images;
        std::vector<SamplerCreationRequest> samplers;
        std::vector<TextureCreationRequest> textures;
        std::vector<MaterialCreationRequest> materials;
        std::vector<MeshCreationRequest> meshes;
    };

    // GPU material layout – must match std430 MaterialData in pbr.frag.
    // Struct size = 48 bytes (padded to a multiple of vec4 alignment).
    struct Material
    {
        glm::vec4 baseColorFactor;
        float metallicFactor;
        float roughnessFactor;
        uint32_t albedoIdx;              // index into samplerData[] (0xFFFFFFFF = none)
        uint32_t normalIdx;              // index into samplerData[] (0xFFFFFFFF = none)
        uint32_t metallicRoughnessIdx;   // index into samplerData[] (0xFFFFFFFF = none)
        uint32_t _pad[3];
    };
    static_assert(sizeof(Material) == 48);

    struct TextureRef
    {
        uint32_t imageId;
        uint32_t samplerId;
    };

    struct Scene
    {
        VU::Buffer vertexBuffer;
        VU::Buffer indexBuffer;
        
        std::vector<Entity> entities;
        std::vector<Mesh> meshes;
        std::vector<glm::mat4x4> transforms;
        std::vector<VkSampler> samplers;
        std::vector<Image> images;
        std::vector<TextureRef> textures;
        std::vector<Material> materials;

        bool cameraWasLoaded = false;
        Camera camera;
    };

    struct LoadStats
    {
        // Timing
        double parseTimeMs     = 0.0;   // TinyGLTF file parse + parallel image decode
        double gpuUploadTimeMs = 0.0;   // GPU buffer/image creation + command recording
        double totalTimeMs     = 0.0;

        // Counts
        uint32_t meshCount     = 0;
        uint32_t vertexCount   = 0;
        uint32_t indexCount    = 0;
        uint32_t imageCount    = 0;     // successfully uploaded to GPU
        uint32_t imageSkipped  = 0;     // decode produced zero dimensions (stbi failure)
        uint32_t imageFailed   = 0;     // GPU vkCreateImage / vkAllocateMemory failed
        uint32_t textureCount  = 0;
        uint32_t materialCount = 0;
        uint32_t samplerCount  = 0;

        // GPU memory
        VkDeviceSize vertexMemBytes  = 0;   // device-local vertex buffer
        VkDeviceSize indexMemBytes   = 0;   // device-local index buffer
        VkDeviceSize textureMemBytes = 0;   // sum of w*h*4 for all successfully uploaded images
        VkDeviceSize totalGpuBytes   = 0;   // vertex + index + texture
    };

    bool LoadScene(const std::filesystem::path& path, imp::Engine& engine, Scene& scene, LoadStats* outStats = nullptr);
    void LoadGLTFNode(const tinygltf::Node& node, const tinygltf::Model& model, std::unordered_map<uint32_t, uint32_t>& meshIdMap
        , std::vector<MeshCreationRequest>& reqs, Scene& scene, glm::mat4 parentTransform = glm::mat4(1.0f));
    
    // Helper function for sampler creation (GLTF specific)
    VkResult CreateSampler(VkDevice device, int magFilter, int minFilter, int wrapS, int wrapT, int wrapR, VkSampler& sampler);
}