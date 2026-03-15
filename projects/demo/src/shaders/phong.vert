#version 450

#extension GL_ARB_shader_draw_parameters: require
#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_nonuniform_qualifier: require

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outLightVec;
layout(location = 3) out vec3 outViewVec;
layout(location = 4) out vec2 outUV;
layout(location = 5) flat out uint outDrawID;

struct Vertex
{
	float vx, vy, vz;
	float nx, ny, nz;
    float tu, tv;
};

struct DrawData
{
    mat4 Transform;
    uint materialIdx;
};

struct MaterialData
{
    uint albedoIx;
};

layout (set = 0, binding = 0) uniform Globals
{
    mat4 viewProj;
    vec3 cameraPos;
    vec3 lightPos;
} globals;

layout(set = 1, binding = 0) readonly buffer Vertices
{
	Vertex vertices[];
};

layout(set = 1, binding = 1) readonly buffer DrawDatas
{
    DrawData drawData[];
};

void main()
{
    Vertex v = vertices[gl_VertexIndex];

    vec3 pos = vec3(vertices[gl_VertexIndex].vx, vertices[gl_VertexIndex].vy, vertices[gl_VertexIndex].vz);
    vec3 norm = vec3(vertices[gl_VertexIndex].nx, vertices[gl_VertexIndex].ny, vertices[gl_VertexIndex].nz);

    uint ddi = gl_DrawIDARB;

    mat4 model      = drawData[ddi].Transform;

    // Transform to world space
    vec3 worldPos = vec3(model * vec4(pos, 1.0));

    mat3 normalMat = mat3(model);
    vec3 worldNormal = normalize(normalMat * norm);

    vec3 lightVec = normalize(globals.lightPos - worldPos);
    vec3 viewVec  = normalize(globals.cameraPos - worldPos);

    outWorldPos = worldPos;
    outNormal   = worldNormal;
    outLightVec = lightVec;
    outViewVec  = viewVec;
    outUV       = vec2(v.tu, v.tv);
    outDrawID   = ddi;

    gl_Position = globals.viewProj * vec4(worldPos, 1.0);
}