#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inLightVec;
layout(location = 3) in vec3 inViewVec;

void main()
{
    vec3 N = normalize(inNormal);
    vec3 L = normalize(inLightVec);
    vec3 V = normalize(inViewVec);

    float ambient = 0.1;

    float diffuse = max(dot(N, L), 0.0);

    vec3 R = reflect(-L, N);
    float specular = pow(max(dot(R, V), 0.0), 32.0);

    // Material parameters (constant for now)
    vec3 baseColor = vec3(1.0, 0.8, 0.6);

    vec3 color =
        ambient * baseColor +
        diffuse * baseColor +
        specular * vec3(1.0);

    outColor = vec4(color, 1.0);
}