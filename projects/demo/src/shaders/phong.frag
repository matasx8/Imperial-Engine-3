#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inLightVec;
layout(location = 3) in vec3 inViewVec;
layout(location = 4) in vec2 inUV;
// gl_DrawIDARB not available in fragment shader
layout(location = 5) flat in uint DrawID;

struct DrawData
{
    mat4 Transform;
    uint materialIdx;
};

struct MaterialData
{
    uint albedoIx;
};

layout(set = 1, binding = 1) readonly buffer DrawDatas
{
    DrawData drawData[];
};

layout(set = 1, binding = 2) readonly buffer MaterialDatas
{
    MaterialData materialData[];
};

// Here we have multiple combined image sampler descriptors so this 
// should be the last binding
layout(set = 1, binding = 3) uniform sampler2D samplerData[];

void main()
{
    uint materialID = drawData[DrawID].materialIdx;
	uint samplerID = materialData[materialID].albedoIx;
    vec3 baseColor = texture(samplerData[nonuniformEXT(samplerID)], inUV).xyz;

    vec3 N = normalize(inNormal);
    vec3 L = normalize(inLightVec);
    vec3 V = normalize(inViewVec);

    float ambient = 0.1;

    float diffuse = max(dot(N, L), 0.0);

    vec3 R = reflect(-L, N);
    float specular = pow(max(dot(R, V), 0.0), 32.0);

    vec3 color =
        ambient * baseColor +
        diffuse * baseColor +
        specular * vec3(1.0);

    outColor = vec4(color, 1.0);
}