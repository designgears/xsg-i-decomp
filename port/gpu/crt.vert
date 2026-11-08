#version 450
// CRT filter (crt.frag): one triangle covering the viewport, which is the game
// rectangle inside the window; v_uv runs 0..1 across it.
layout(location = 0) out vec2 v_uv;

void main() {
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    v_uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
