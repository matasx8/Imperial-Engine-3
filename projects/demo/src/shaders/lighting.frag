#version 450

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

// ---------------------------------------------------------
//  Descriptor sets
// ---------------------------------------------------------
layout(set = 0, binding = 0) uniform Globals
{
    mat4 viewProj;
    mat4 invViewProj;
    vec3 cameraPos;
    vec3 lightDir;
} globals;

// G-Buffer - sampled with nearest/clamp (set up in CreateLightingPipeline)
layout(set = 1, binding = 0) uniform sampler2D gbAlbedoMetallic;   // RGB = albedo,      A = metallic
layout(set = 1, binding = 1) uniform sampler2D gbNormalRoughness;  // RGB = world normal, A = roughness
layout(set = 1, binding = 2) uniform sampler2D gbDepth;            // depth in [0, 1]

// ---------------------------------------------------------
//  Cook-Torrance BRDF helpers
// ---------------------------------------------------------
const float PI = 3.14159265359;

// GGX / Trowbridge-Reitz normal distribution function
float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Smith-GGX geometry term
float G_SmithGGX(float NdotV, float NdotL, float roughness)
{
    // Schlick-GGX remapping for direct lights: k = (r+1)^2 / 8
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

// ---------------------------------------------------------
//  ACES filmic tone mapping - Narkowicz 2016
// ---------------------------------------------------------
vec3 ACES(vec3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ---------------------------------------------------------
//  Main
// ---------------------------------------------------------
void main()
{
    float depth = texture(gbDepth, inUV).r;

    // Sky / background - no geometry written to this pixel
    if (depth >= 1.0)
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Sample G-Buffer
    vec4 albedoMetallic  = texture(gbAlbedoMetallic,  inUV);
    vec4 normalRoughness = texture(gbNormalRoughness, inUV);

    vec3  albedo    = albedoMetallic.rgb;
    float metallic  = albedoMetallic.a;
    vec3  N         = normalize(normalRoughness.rgb);
    float roughness = normalRoughness.a;

    // Reconstruct world position from depth
    // inUV [0,1] -> NDC [-1,1]; depth is in [0,1] for Vulkan
    vec4 clipPos  = vec4(inUV * 2.0 - 1.0, depth, 1.0);
    vec4 worldPos = globals.invViewProj * clipPos;
    worldPos /= worldPos.w;

    // Cook-Torrance BRDF
    // F0: base reflectance at normal incidence - dielectrics use 0.04, metals use albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3  V     = normalize(globals.cameraPos - worldPos.xyz);
    vec3  L     = normalize(globals.lightDir);
    vec3  H     = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0001);
    float NdotL = max(dot(N, L), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float D   = D_GGX(NdotH, roughness);
    float G   = G_SmithGGX(NdotV, NdotL, roughness);
    vec3  F   = F_Schlick(VdotH, F0);

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

    // Tone mapping + gamma correction
    color = ACES(color);
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
