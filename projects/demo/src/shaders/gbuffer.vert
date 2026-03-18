#version 450

#extension GL_ARB_shader_draw_parameters : require

// ---------------------------------------------------------
//  Outputs
// ---------------------------------------------------------
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outT;       // world-space tangent
layout(location = 3) out vec3 outB;       // world-space bitangent
layout(location = 4) out vec3 outN;       // world-space normal
layout(location = 5) flat out uint outDrawID;

// ---------------------------------------------------------
//  GPU structs - must match VU::Vertex / VU::DrawData (std430)
// ---------------------------------------------------------
struct Vertex
{
    float vx, vy, vz;      // position
    float nx, ny, nz;      // normal
    float tu, tv;           // uv
    float tx, ty, tz, tw;  // tangent (w = handedness sign)
};

struct DrawData
{
    mat4  transform;
    uint  materialIdx;
};

// ---------------------------------------------------------
//  Descriptor sets
// ---------------------------------------------------------
layout(set = 0, binding = 0) uniform Globals
{
    mat4 viewProj;
    mat4 invViewProj;  // not used here, declared to match GlobalUniformsData layout
    vec3 cameraPos;
    vec3 lightDir;
} globals;

layout(set = 1, binding = 0) readonly buffer Vertices    { Vertex   vertices[];    };
layout(set = 1, binding = 1) readonly buffer DrawDatas   { DrawData drawData[];    };
// Maps compacted gl_DrawIDARB (post-culling slot index) back to the original DrawData index.
// Written each frame by the culling compute shader alongside the compacted command buffer.
layout(set = 1, binding = 5) readonly buffer DrawIndices { uint     drawIndices[]; };

// ---------------------------------------------------------
//  Main
// ---------------------------------------------------------
void main()
{
    Vertex v   = vertices[gl_VertexIndex];
    uint   ddi = drawIndices[gl_DrawIDARB];  // remap post-culling slot → original DrawData index

    vec3  pos      = vec3(v.vx, v.vy, v.vz);
    vec3  norm     = vec3(v.nx, v.ny, v.nz);
    vec3  tang     = vec3(v.tx, v.ty, v.tz);
    float tangSign = v.tw;

    mat4 model     = drawData[ddi].transform;
    // Inverse-transpose of the 3x3 model sub-matrix - handles non-uniform scale correctly.
    mat3 normalMat = transpose(inverse(mat3(model)));

    vec3 worldPos = vec3(model * vec4(pos, 1.0));

    vec3 N = normalize(normalMat * norm);
    vec3 T = normalize(normalMat * tang);
    T = normalize(T - dot(T, N) * N);    // Gram-Schmidt re-orthogonalise
    vec3 B = cross(N, T) * tangSign;

    outWorldPos = worldPos;
    outUV       = vec2(v.tu, v.tv);
    outT        = T;
    outB        = B;
    outN        = N;
    outDrawID   = ddi;

    gl_Position = globals.viewProj * vec4(worldPos, 1.0);
}
