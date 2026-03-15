#version 450

layout(location = 0) out vec2 outUV;

// ---------------------------------------------------------
//  Main
// ---------------------------------------------------------
void main()
{
    // Fullscreen triangle trick - covers the screen with 3 vertices, no vertex buffer.
    // gl_VertexIndex:  0         1         2
    // outUV:           (0, 0)    (2, 0)    (0, 2)
    // gl_Position:     (-1, -1)  (3, -1)   (-1, 3)
    outUV.x = float((gl_VertexIndex << 1) & 2);
    outUV.y = float(gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}
