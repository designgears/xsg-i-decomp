#version 450
// Dear ImGui vertices for the in-game menu (menu_render.cpp).
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 col;
layout(push_constant) uniform PC { vec2 scale; vec2 translate; } pc;
layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_col;
void main() {
    v_uv = uv;
    v_col = col;
    gl_Position = vec4(pos * pc.scale + pc.translate, 0.0, 1.0);
}
