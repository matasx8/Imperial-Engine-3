#pragma once
#include "Tiny_GLTF/tiny_gltf.h"
#include "vkutilities.h"
#include <filesystem>

inline constexpr uint32_t kMaxMaterialCount = 128;
inline constexpr uint32_t kDefaultMaterialIndex = kMaxMaterialCount - 1u;
inline constexpr uint32_t kMaxMaterialIndex = kDefaultMaterialIndex - 1u;

class imp::Engine;

namespace SceneLoader
{
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
        uint32_t indexCount;
    };

    bool LoadScene(const std::filesystem::path& path, imp::Engine& engine, Scene& scene);
    void LoadGLTFNode(const tinygltf::Node& node, const tinygltf::Model& model, std::unordered_map<uint32_t, uint32_t>& meshIdMap
        , std::vector<MeshCreationRequest>& reqs);
}