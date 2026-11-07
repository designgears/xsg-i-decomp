#version 450
// Dear ImGui fragments for the in-game menu (menu_render.cpp).
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_col;
layout(location = 0) out vec4 o;
void main() { o = v_col * texture(tex, v_uv); }
