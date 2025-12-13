#version 450

layout(push_constant) uniform constants
{
    float xOffset;
    float zOffset;
};

layout(location = 0) out vec3 color;

vec3 triangle_vertices[] = vec3[3](
    vec3(   0, -0.5, 0.0),
    vec3( 0.5,  0.5, 0.0),
    vec3(-0.5,  0.5, 0.0)
);

vec3 triangle_colors[] = vec3[3](
    vec3( 1.0, 0.0, 0.0),
    vec3( 0.0, 1.0, 0.0),
    vec3( 0.0, 0.0, 1.0)
);

void main()
{
    gl_Position = vec4(triangle_vertices[gl_VertexIndex], 1.0);
    color = triangle_colors[gl_VertexIndex];
}