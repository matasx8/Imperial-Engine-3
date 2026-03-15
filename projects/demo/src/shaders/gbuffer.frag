#version 450

#extension GL_EXT_nonuniform_qualifier : require

// ---------------------------------------------------------
//  Outputs - G-Buffer attachments
// ---------------------------------------------------------
layout(location = 0) out vec4 outAlbedoMetallic;   // RGB = albedo,      A = metallic
layout(location = 1) out vec4 outNormalRoughness;  // RGB = world normal, A = roughness

// ---------------------------------------------------------
//  Inputs (must match gbuffer.vert outputs)
// ---------------------------------------------------------
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inT;
layout(location = 3) in vec3 inB;
layout(location = 4) in vec3 inN;
layout(location = 5) flat in uint inDrawID;

// ---------------------------------------------------------
//  GPU structs - must match VU::DrawData / SceneLoader::Material (std430)
// ---------------------------------------------------------
struct DrawData
{
    mat4 transform;
    uint materialIdx;
};

// 48-byte layout - matches SceneLoader::Material + its static_assert
struct MaterialData
{
    vec4  baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    uint  albedoIdx;             // 0xFFFFFFFF = no texture
    uint  normalIdx;
    uint  metallicRoughnessIdx;
    uint  _pad[3];
};

const uint kInvalidId = 0xFFFFFFFFu;

// ---------------------------------------------------------
//  Descriptor sets
// ---------------------------------------------------------
layout(set = 1, binding = 1) readonly buffer DrawDatas     { DrawData     drawData[];     };
layout(set = 1, binding = 2) readonly buffer MaterialDatas { MaterialData materialData[]; };
layout(set = 1, binding = 3) uniform sampler2D samplerData[];

// ---------------------------------------------------------
//  Main
// ---------------------------------------------------------
void main()
{
    uint         matID = drawData[inDrawID].materialIdx;
    MaterialData mat   = materialData[matID];

    // Base colour
    vec4 baseColor = mat.baseColorFactor;
    if (mat.albedoIdx != kInvalidId)
        baseColor *= texture(samplerData[nonuniformEXT(mat.albedoIdx)], inUV);

    // Alpha cutout
    if (baseColor.a < 0.5)
        discard;

    // Normal
    vec3 N = normalize(inN);
    if (mat.normalIdx != kInvalidId)
    {
        // glTF normal maps are in tangent space, stored as [0,1] - remap to [-1,1]
        vec3 tangentNormal = texture(samplerData[nonuniformEXT(mat.normalIdx)], inUV).xyz * 2.0 - 1.0;
        mat3 TBN           = mat3(normalize(inT), normalize(inB), N);
        N                  = normalize(TBN * tangentNormal);
    }

    // Metallic / Roughness - glTF packs: G = roughness, B = metallic
    float metallic  = mat.metallicFactor;
    float roughness = mat.roughnessFactor;
    if (mat.metallicRoughnessIdx != kInvalidId)
    {
        vec4 mr  = texture(samplerData[nonuniformEXT(mat.metallicRoughnessIdx)], inUV);
        roughness *= mr.g;
        metallic  *= mr.b;
    }
    roughness = clamp(roughness, 0.04, 1.0);
    metallic  = clamp(metallic,  0.0,  1.0);

    // Pack into G-Buffer
    // Normal is stored as raw [-1,1] world-space vector - R16G16B16A16_SFLOAT holds this natively.
    outAlbedoMetallic  = vec4(baseColor.rgb, metallic);
    outNormalRoughness = vec4(N, roughness);
}
