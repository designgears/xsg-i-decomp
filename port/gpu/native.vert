#version 450
// Native renderer, set-0 strip (VU1 loop 0x0c18): port/docs/NATIVE_RENDERER.md.
// The same per-vertex math as port/render/vu1_ref/vu1_set0.c strip_loop, in
// IEEE floats instead of VU floats.
layout(location = 0) in vec4 a_p;      // xyz, w = S
layout(location = 1) in vec4 a_c;      // RGBA 0..255 (after ITOF)
layout(location = 2) in float a_t;     // T

layout(std140, set = 0, binding = 0) uniform Batch {
    vec4 m[4];          // qw 962-965: clip = m0*x + m1*y + m2*z + m3
    vec4 scale;         // 966
    vec4 offset;        // 967
    vec4 cscale;        // vf24
    vec4 uv;            // 972
    vec4 fog;           // 1007: min, max, bias, scale
    vec4 target;        // x: XYOFFSET.x/16, y: XYOFFSET.y/16, z: 1/width, w: 1/height (GS pixels)
    vec4 zscale;        // x: 1/2^24 (GS Z -> depth)
    vec4 fogcol;        // FOGCOL 0..255
    ivec4 mode;         // x: TFX | TCC<<2 | TME<<3 | FGE<<4, y: ATE|ATST<<1|AFAIL<<4, z: AREF, w: alpha source (0 As, 2 FIX)
    vec4 fix;           // x: ALPHA.FIX
    ivec4 wrap;         // x: CLAMP.WMS, y: CLAMP.WMT, z: TEX1.MMAG (1 = linear)
    ivec4 region;       // CLAMP MINU/MSKU, MAXU/FIXU, MINV/MSKV, MAXV/FIXV
} b;

layout(location = 0) out vec4 v_col;
layout(location = 1) out vec2 v_st;
layout(location = 2) out float v_fog;

void main() {
    vec4 clip = b.m[0] * a_p.x + b.m[1] * a_p.y + b.m[2] * a_p.z + b.m[3];
    float q = 1.0 / clip.w;
    v_st = vec2(a_p.w + b.uv.x, a_t + b.uv.y);           // S/Q, T/Q: interpolated perspective-correct
    v_col = floor(min(a_c * b.cscale, vec4(255.0)));       // ftoi0(min(C*vf24, 255))
    v_fog = floor(clamp(b.fog.z + b.fog.w * q, b.fog.x, b.fog.y));
    vec3 scr = b.offset.xyz + clip.xyz * q * b.scale.xyz;  // GS X/Y (pixels incl. offset), Z
    // The VU writes XYZF2 as ftoi4 (12.4 fixed point, truncated) and the GS
    // rasterises from that.  Snapping here makes vertices that two batches share
    // (computed with different part matrices) land on the same point, so no
    // cracks open between them at high resolution.
    scr.xy = trunc(scr.xy * 16.0) / 16.0;
    // GS samples a pixel at its integer corner, Vulkan at its centre: +0.5.
    vec2 px = scr.xy - b.target.xy + 0.5;
    vec2 ndc = px * b.target.zw * 2.0 - 1.0;
    float depth = clamp(floor(scr.z) * b.zscale.x, 0.0, 1.0);
    float w = clip.w;
    gl_Position = vec4(ndc * w, depth * w, w);
}
