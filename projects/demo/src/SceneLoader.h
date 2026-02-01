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

    struct Camera
    {
        glm::mat4x4 Projection;
        glm::mat4x4 View;
		glm::mat4x4 Model;
    };

    struct MeshCreationRequest
    {
        uint32_t id;
        uint32_t materialId;
        std::vector<VU::Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    struct Scene
    {
        VU::Buffer vertexBuffer;
        VU::Buffer indexBuffer;
        
        std::vector<Entity> entities;
        std::vector<Mesh> meshes;
        std::vector<glm::mat4x4> transforms;

        uint32_t indexCount;

        bool cameraWasLoaded = false;
        Camera camera;
    };

    bool LoadScene(const std::filesystem::path& path, imp::Engine& engine, Scene& scene);
    void LoadGLTFNode(const tinygltf::Node& node, const tinygltf::Model& model, std::unordered_map<uint32_t, uint32_t>& meshIdMap
        , std::vector<MeshCreationRequest>& reqs, Scene& scene);
}