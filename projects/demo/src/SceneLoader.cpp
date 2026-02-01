#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "SceneLoader.h"
#include "Engine.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>

namespace SceneLoader
{
    static inline std::atomic_uint32_t temporaryMeshCounter = 0;

    static bool ParseGLTF(const std::filesystem::path& path, std::vector<MeshCreationRequest>& reqs, Scene& scene)
    {
        if (!std::filesystem::exists(path))
            return false;

        if (path.extension() != ".gltf" && path.extension() != ".glb")
            return false;

        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
		std::string err;
		std::string warn;

        if(path.extension().string() == ".gltf")
			loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
		else
			loader.LoadBinaryFromFile(&model, &err, &warn, path.string());

		if (err.size()) printf("[Asset Importer] Error: %s\n", err.c_str());
		if (warn.size()) printf("[Asset Importer] Warning: %s\n", warn.c_str());

		std::unordered_map<uint32_t, uint32_t> meshIdMap;

        for (const auto& nodeIdx : model.scenes.front().nodes)
        {
            const auto& node = model.nodes[nodeIdx];
            LoadGLTFNode(node, model, meshIdMap, reqs, scene);
        }

        return true;
    }



    bool LoadScene(const std::filesystem::path& path, imp::Engine& engine, Scene& scene)
    {
        std::vector<MeshCreationRequest> reqs;
        if (!ParseGLTF(path, reqs, scene))
            return false;

        VkPhysicalDevice pDevice = engine.GetPhysicalDevice();
        VkDevice device = engine.GetWorkQueue().GetDevice();

        VkDeviceSize vertexBufferSize = 0;
        VkDeviceSize indexBufferSize = 0;
        for (const auto& req : reqs)
        {
            Mesh mesh {};
            mesh.id = scene.meshes.size();
            mesh.vertexOffset = static_cast<uint32_t>(vertexBufferSize / sizeof(VU::Vertex));
            mesh.indexOffset = static_cast<uint32_t>(indexBufferSize / sizeof(uint32_t));
            mesh.vertexCount = static_cast<uint32_t>(req.vertices.size());
            mesh.indexCount = static_cast<uint32_t>(req.indices.size());
            scene.meshes.push_back(mesh);

            vertexBufferSize += sizeof(VU::Vertex) * req.vertices.size();
            indexBufferSize += sizeof(uint32_t) * req.indices.size();
        }

        // Create vertex staging buffer
        VU::Buffer vertexStagingBuffer;
        VkResult result = CreateBuffer(pDevice, device,
                                        vertexBufferSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        vertexStagingBuffer);
        if (result != VK_SUCCESS)
            return result;

        // Map and copy vertex data
        void* vertexData;
        vkMapMemory(device, vertexStagingBuffer.memory, 0, VK_WHOLE_SIZE, 0, &vertexData);
        for (const auto& req : reqs)
        {
            memcpy(vertexData, req.vertices.data(), sizeof(VU::Vertex) * req.vertices.size());
            vertexData = static_cast<uint8_t*>(vertexData) + sizeof(VU::Vertex) * req.vertices.size();
        }
        vkUnmapMemory(device, vertexStagingBuffer.memory);

        // Create index buffer
        VU::Buffer indexStagingBuffer;
        result = CreateBuffer(pDevice, device,
                                indexBufferSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        indexStagingBuffer);

        if (result != VK_SUCCESS)
            return result;

        // Map and copy index data
        void* indexData;
        vkMapMemory(device, indexStagingBuffer.memory, 0, VK_WHOLE_SIZE, 0, &indexData);
        for (const auto& req : reqs)
        {
            memcpy(indexData, req.indices.data(), sizeof(uint32_t) * req.indices.size());
            indexData = static_cast<uint8_t*>(indexData) + sizeof(uint32_t) * req.indices.size();
        }
        vkUnmapMemory(device, indexStagingBuffer.memory);

        // Create device local vertex buffer
        result = CreateBuffer(pDevice, device,
                                vertexBufferSize,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                scene.vertexBuffer);

        if (result != VK_SUCCESS)
            return result;

        // Create device local index buffer
        result = CreateBuffer(pDevice, device,
                                indexBufferSize,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                scene.indexBuffer);

        if (result != VK_SUCCESS)
            return result;

         VkCommandBuffer cb = engine.AcquireCommandBuffer(imp::CommandBufferType::Graphics);

        // Copy vertex data to device local buffer
        VkBufferCopy copyRegion {};
        copyRegion.size = vertexBufferSize;
        vkCmdCopyBuffer(cb, vertexStagingBuffer.buffer, scene.vertexBuffer.buffer, 1, &copyRegion);

        // Copy index data to device local buffer
        copyRegion.size = indexBufferSize;
        vkCmdCopyBuffer(cb, indexStagingBuffer.buffer, scene.indexBuffer.buffer, 1, &copyRegion);

        // Add memory barrier to ensure copies are visible before reads
        VU::InsertPipelineBarrier2(cb,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT);

        // Submit copy commands
        vkEndCommandBuffer(cb);
        imp::SubmitParams submitParams {};
        submitParams.commandBufferCount = 1;
        submitParams.pCommandBuffers = &cb;
        submitParams.queue = engine.GetWorkQueue().GetGraphicsQueue();
        imp::SubmitSync sync = engine.Submit(&submitParams, 1);

        // Enqueue staging buffers for deferred destruction
        imp::SafeResourceDestroyer& destroyer = engine.GetSafeResourceDestroyer();
        imp::VulkanResource res {};
        res.buffer = vertexStagingBuffer.buffer;
        res.memory = vertexStagingBuffer.memory;
        res.type = imp::VulkanResourceType::Buffer;
        destroyer.EnqueueResourceForDestruction(res, sync.submit);
        res.buffer = indexStagingBuffer.buffer;
        res.memory = indexStagingBuffer.memory;
        destroyer.EnqueueResourceForDestruction(res, sync.submit);

        return true;
    }

    void LoadGLTFNode(const tinygltf::Node& node, const tinygltf::Model& model, std::unordered_map<uint32_t, uint32_t>& meshIdMap
        , std::vector<MeshCreationRequest>& reqs, Scene& scene)
    {
        auto transform = glm::mat4x4(1.0f);

		if (node.matrix.size())
			transform = glm::make_mat4x4(node.matrix.data());
		else
		{
			if (node.scale.size())
				transform = glm::scale(transform, glm::vec3(glm::make_vec3(node.scale.data())));

			if (node.rotation.size())
				transform *= glm::mat4((glm::quat)glm::make_quat(node.rotation.data()));

			if (node.translation.size())
				transform = glm::translate(transform, glm::vec3(glm::make_vec3(node.translation.data())));
		}

        if (std::string(node.name).find("Camera") != std::string::npos &&
            scene.cameraWasLoaded == false)
        {
            scene.camera.Model = transform;
            scene.cameraWasLoaded = true;
        }

        for (const auto child : node.children)
            LoadGLTFNode(model.nodes[child], model, meshIdMap, reqs, scene);

        // This is so we don't load geometry for linked/duplicate meshes.
        if (meshIdMap.find(node.mesh) != meshIdMap.end())
        {
            const auto& mesh = model.meshes[node.mesh];

            int primIndex = 0;
            for (const auto& prim : mesh.primitives)
            {
                MeshCreationRequest req;
                // Adds primIndex so it maps to the temporary mesh IDs generated below. 
                req.id = meshIdMap[node.mesh] + primIndex;
                req.materialId = model.meshes[node.mesh].primitives.front().material;
                reqs.push_back(req);

                scene.transforms.push_back(transform);

                Entity entity;
                entity.id = static_cast<uint32_t>(scene.entities.size());
                entity.meshId = req.id;
                entity.transformId = static_cast<uint32_t>(scene.transforms.size() - 1);
                entity.materialId = req.materialId;
                scene.entities.push_back(entity);

                primIndex++;
            }

            return;
        }

        if (node.mesh > -1)
        {
            const auto& mesh = model.meshes[node.mesh];

			for (const auto& prim : mesh.primitives)
            {
                MeshCreationRequest req;
				req.id = temporaryMeshCounter.fetch_add(1);
				req.materialId = prim.material >= 0 ? prim.material : kDefaultMaterialIndex;
				if (req.materialId >= kMaxMaterialIndex) // last one is reserved for default
				{
					req.materialId = kDefaultMaterialIndex;
					printf("[Scene Loader] Error: Trying to assign material with ID '%u' when max is '%u'. Will assign default material.\n", prim.material, kMaxMaterialIndex);
				}

                meshIdMap[node.mesh] = req.id; // can keep rewriting this

                const float* positionBuffer = nullptr;
                const float* normalsBuffer = nullptr;
                const float* texCoordsBuffer = nullptr;
                size_t vertexCount = 0;

                if (prim.attributes.find("POSITION") != prim.attributes.end()) 
                {
                    const auto& accessor = model.accessors[prim.attributes.find("POSITION")->second];
                    const auto& view = model.bufferViews[accessor.bufferView];
                    positionBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    vertexCount = accessor.count;
                }

                if (prim.attributes.find("NORMAL") != prim.attributes.end()) 
                {
                    const auto& accessor = model.accessors[prim.attributes.find("NORMAL")->second];
                    const auto& view = model.bufferViews[accessor.bufferView];
                    normalsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                if (prim.attributes.find("TEXCOORD_0") != prim.attributes.end()) 
                {
                    const auto& accessor = model.accessors[prim.attributes.find("TEXCOORD_0")->second];
                    const auto& view = model.bufferViews[accessor.bufferView];
                    texCoordsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                for (size_t i = 0; i < vertexCount; i++)
                {
                    VU::Vertex vertex;
                    std::memcpy(&vertex.position, &positionBuffer[i * 3], sizeof(float) * 3);
                    std::memcpy(&vertex.normals, &normalsBuffer[i * 3], sizeof(float) * 3);

                    vertex.normals.x = prim.attributes.find("TEXCOORD_0") != prim.attributes.end() ? texCoordsBuffer[i * 2] : 0.0f;
                    vertex.normals.y = prim.attributes.find("TEXCOORD_0") != prim.attributes.end() ? texCoordsBuffer[i * 2 + 1] : 0.0f;
                    //vertex.normals.x = prim.attributes.find("TEXCOORD_0") != prim.attributes.end() ? meshopt_quantizeHalf(texCoordsBuffer[i * 2]) : 0.0f;
                    //vertex.normals.y = prim.attributes.find("TEXCOORD_0") != prim.attributes.end() ? meshopt_quantizeHalf(texCoordsBuffer[i * 2 + 1]) : 0.0f;

                    req.vertices.push_back(vertex);
                }

                
				const auto& accessor = model.accessors[prim.indices];
				const auto& bufferView = model.bufferViews[accessor.bufferView];
				const auto& buffer = model.buffers[bufferView.buffer];

				const auto indexCount = accessor.count;

				const auto FillIndices = [&](const auto& buf)
				{
					for (size_t index = 0; index < accessor.count; index++)
					{
						req.indices.push_back(buf[index]);
					}
				};
				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: 
				{
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					FillIndices(buf);
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: 
				{
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					FillIndices(buf);
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: 
				{
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					FillIndices(buf);
					break;
				}
				default:
					printf("[Scene Loader] Error: Index component type %i not supported!\n", accessor.componentType);
					return;
				}

                
                scene.transforms.push_back(transform);

                Entity entity;
                entity.id = static_cast<uint32_t>(scene.entities.size());
                entity.meshId = req.id;
                entity.transformId = static_cast<uint32_t>(scene.transforms.size() - 1);
                entity.materialId = req.materialId;
                scene.entities.push_back(entity);

				reqs.push_back(req);
            }
        }
    }
}