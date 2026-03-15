#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out vec4 outColor;

// ──────────────────────────────────────────────────────────
//  Inputs (must match pbr.vert outputs)
// ──────────────────────────────────────────────────────────
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inT;
layout(location = 3) in vec3 inB;
layout(location = 4) in vec3 inN;
layout(location = 5) flat in uint inDrawID;

// ──────────────────────────────────────────────────────────
//  GPU structs – must match VU::DrawData / SceneLoader::Material (std430)
// ──────────────────────────────────────────────────────────
struct DrawData
{
    mat4 transform;
    uint materialIdx;
};

// 48-byte layout – matches SceneLoader::Material + its static_assert
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

// ──────────────────────────────────────────────────────────
//  Descriptor sets
// ──────────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform Globals
{
    mat4 viewProj;
    vec3 cameraPos;
    vec3 lightDir;
} globals;

layout(set = 1, binding = 1) readonly buffer DrawDatas    { DrawData     drawData[];     };
layout(set = 1, binding = 2) readonly buffer MaterialDatas { MaterialData materialData[]; };
layout(set = 1, binding = 3) uniform sampler2D samplerData[];

// ──────────────────────────────────────────────────────────
//  Cook-Torrance BRDF helpers
// ──────────────────────────────────────────────────────────
const float PI = 3.14159265359;

// GGX / Trowbridge-Reitz normal distribution function
float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Smith–GGX geometry term (combines masking G1 for V and L)
float G_SmithGGX(float NdotV, float NdotL, float roughness)
{
    // Schlick-GGX remapping for direct lights: k = (r+1)² / 8
    float r  = roughness + 1.0;
    float k  = (r * r) / 8.0;
    float gV = NdotV / (NdotV * (1.0 - k) + k);
    float gL = NdotL / (NdotL * (1.0 - k) + k);
    return gV * gL;
}

// Schlick Fresnel approximation
vec3 F_Schlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ──────────────────────────────────────────────────────────
//  ACES filmic tone mapping
//  Fitted curve by Krzysztof Narkowicz (2016)
// ──────────────────────────────────────────────────────────
vec3 ACES(vec3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ──────────────────────────────────────────────────────────
//  Main
// ──────────────────────────────────────────────────────────
void main()
{
    uint         matID = drawData[inDrawID].materialIdx;
    MaterialData mat   = materialData[matID];

    // ── Base colour ───────────────────────────────────────
    vec4 baseColor = mat.baseColorFactor;
    if (mat.albedoIdx != kInvalidId)
        baseColor *= texture(samplerData[nonuniformEXT(mat.albedoIdx)], inUV);

    // Alpha cutout
    if (baseColor.a < 0.5)
        discard;

    // ── Normal ────────────────────────────────────────────
    vec3 N = normalize(inN);
    if (mat.normalIdx != kInvalidId)
    {
        // glTF normal maps are in tangent space, stored as [0,1] → remap to [-1,1]
        vec3 tangentNormal = texture(samplerData[nonuniformEXT(mat.normalIdx)], inUV).xyz * 2.0 - 1.0;
        mat3 TBN           = mat3(normalize(inT), normalize(inB), N);
        N                  = normalize(TBN * tangentNormal);
    }

    // ── Metallic / Roughness ──────────────────────────────
    // glTF packs: G = roughness, B = metallic
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

    // ── Cook-Torrance BRDF ────────────────────────────────
    vec3 albedo = baseColor.rgb;

    // F0: base reflectance at normal incidence.
    // Dielectrics use a fixed 0.04; metals use their albedo colour.
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3  V     = normalize(globals.cameraPos - inWorldPos);
    vec3  L     = normalize(globals.lightDir);
    vec3  H     = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0001);
    float NdotL = max(dot(N, L), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float D        = D_GGX(NdotH, roughness);
    float G        = G_SmithGGX(NdotV, NdotL, roughness);
    vec3  F        = F_Schlick(VdotH, F0);

    // Specular lobe
    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL);

    // Energy-conserving diffuse: metals have no diffuse contribution
    vec3 kD      = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    // Single white directional light
    vec3 lightColor = vec3(10.0);
    vec3 Lo         = (diffuse + specular) * lightColor * NdotL;

    // Ambient approximation (IBL would replace this)
    vec3 ambient = vec3(0.15) * albedo;

    vec3 color = ambient + Lo;

    // ── Tone mapping + gamma correction ───────────────────
    color = ACES(color);
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
