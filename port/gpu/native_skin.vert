#version 450
// Native renderer, set 2 (characters): the twelve VU1 vertex loops of
// port/render/vu1_ref/vu1_set2*.c in one vertex shader, GPU-skinned.
// See port/docs/NATIVE_RENDERER.md §7 and VU1_PROGRAMS.md §2.
//   group 0: up to 4 bones (B = qw addresses, W = weights), stop at the first zero address
//   group 1: rigid, one matrix (B.x)
//   group 2: "blend 8", two bones (B.x, B.y), weights B.z/B.w, always both
//   group 3: as 2, bone 1 only when B.y != 0
//   group 4: set 0 lit loops (vu1_set0_lit.c): no bones, P/N already object space;
//            per-vertex fog (cfg.w & 16) as the set-0 strip
//   variant 0 lit, 1 one-pass specular, 2 specular second strip (pass 1), 3 env second strip (pass 2)
// The facing test and the guard-band ADC are not recomputed: the CPU reference
// already decided them and xsg-gpu drops those triangles from the index list.
layout(location = 0) in vec4 a_p;      // xyz, w = S
layout(location = 1) in vec4 a_n;      // xyz, w = T
layout(location = 2) in vec4 a_c;      // colour (float RGBA)
layout(location = 3) in uvec4 a_b;     // bone stream
layout(location = 4) in vec4 a_w;      // weights (group 0)
layout(location = 5) in vec4 a_pp;     // previous frame's P (xyz), for motion vectors

layout(std140, set = 0, binding = 0) uniform Batch {
    vec4 m[4];
    vec4 scale;
    vec4 offset;
    vec4 cscale;
    vec4 uv;
    vec4 fog;
    vec4 target;        // x: XYOFFSET.x/16, y: XYOFFSET.y/16, z: 1/width, w: 1/height (GS pixels)
    vec4 zscale;        // x: GS Z -> depth
    vec4 fogcol;
    ivec4 mode;
    vec4 fix;
    ivec4 wrap;
    ivec4 region;
    vec4 mprev[4];
    vec4 mv;            // xy: 966.xy * scale, z: has previous (set 0), w: set-2 pass
} b;

layout(std140, set = 0, binding = 2) uniform SkinConst {
    vec4 m[4];          // vf28-31 object -> clip
    vec4 scale;         // vf26
    vec4 offset;        // vf27
    vec4 lmat[3];       // vf17-19 (.w: 973-975.w)
    vec4 lcol[4];       // vf20-22 light colours, vf23 ambient
    vec4 diffuse;       // vf25
    vec4 env;           // vf24
    vec4 eye;           // 976
    vec4 c970;
    vec4 uvo;           // 972
    vec4 c1008;
    vec4 mirror;        // 1017 (.z)
    vec4 e1004[3];
    vec4 c1009;         // env pass colour (integers as floats)
    ivec4 cfg;          // x group, y variant, z 1: per-pixel lighting (native.frag SKIN_PPL) (the pass is Batch.mv.w: 0 A, 1 spec strip, 2 env strip), w flags (1 renorm, 2 mirror, 4 has prev)
    ivec4 cfg2;         // x spec squarings, y ifirst (float bits), z fog F, w palette base (qw in Pal)
    ivec4 cfg3;         // x previous palette base
    vec4 pm[4];         // previous frame's vf28-31
    vec4 pmirror;
    vec4 fog;           // group 4: x min, y max, z bias, w scale
} s;
// All bone palettes (qw 732..961 of VU1 data memory) received recently, 230 qw each.
layout(std430, set = 0, binding = 3) readonly buffer Pal { vec4 q[]; } pal;

layout(location = 0) out vec4 v_col;
layout(location = 1) out vec2 v_st;
layout(location = 2) out float v_fog;
layout(location = 3) out vec4 v_cur;
layout(location = 4) out vec4 v_prev;
// Per-pixel lighting ([renderer] lighting = perpixel; native.frag built with SKIN_PPL):
// the skinned normal and position (object space) and the vertex colour C.
layout(location = 5) out vec3 v_n;
layout(location = 6) out vec3 v_p;
layout(location = 7) out vec4 v_c;

vec4 Q(bool prev, int i) { return pal.q[(prev ? s.cfg3.x : s.cfg2.w) + i]; }

// VU1 floats have no infinity: results clamp at +-FLT_MAX.  The game parks bones it
// hides (parts of KOS-MOS's hair, weapons) as matrices of +-FLT_MAX; on the VU those
// vertices collapse onto the screen offset, in IEEE floats they overflow to inf/NaN
// and the triangles explode across the screen or vanish.  S() clamps like the VU.
const float VMAX = 3.4028235e38;
vec3 S(vec3 v) { return clamp(v, vec3(-VMAX), vec3(VMAX)); }
vec4 S(vec4 v) { return clamp(v, vec4(-VMAX), vec4(VMAX)); }
float S(float v) { return clamp(v, -VMAX, VMAX); }

// One bone at qw address a: pos/nrm += w * M (P,1) / M3 N.
void Bone(bool prev, uint a, float w, vec3 p, vec3 n, inout vec3 pos, inout vec3 nrm) {
    int i = int(a & 0x3ffu) - 732;
    if (i < 0 || i + 3 >= 230 || w == 0.0) return;
    vec4 m0 = Q(prev, i), m1 = Q(prev, i + 1), m2 = Q(prev, i + 2), m3 = Q(prev, i + 3);
    vec3 t = S(S(S(S(m0.xyz * p.x) + S(m1.xyz * p.y)) + S(m2.xyz * p.z)) + m3.xyz);
    vec3 u = S(S(S(m0.xyz * n.x) + S(m1.xyz * n.y)) + S(m2.xyz * n.z));
    pos = S(pos + S(w * t));
    nrm = S(nrm + S(w * u));
}

void Skin(bool prev, vec3 p, vec3 n, out vec3 pos, out vec3 nrm) {
    pos = vec3(0.0);
    nrm = vec3(0.0);
    int g = s.cfg.x;
    if (g == 4) {
        pos = p;
        nrm = n;
    } else if (g == 0) {
        uint bone = a_b.x & 0xffffu;
        for (int k = 0; k < 4; ++k) {
            Bone(prev, bone, a_w[k], p, n, pos, nrm);
            bone = a_b[(k + 1) & 3] & 0xffffu;
            if (bone == 0u) break;
        }
    } else if (g == 1) {
        Bone(prev, a_b.x & 0xffffu, 1.0, p, n, pos, nrm);
    } else {
        Bone(prev, a_b.x & 0xffffu, uintBitsToFloat(a_b.z), p, n, pos, nrm);
        if (g == 2 || (a_b.y & 0xffffu) != 0u) Bone(prev, a_b.y & 0xffffu, uintBitsToFloat(a_b.w), p, n, pos, nrm);
    }
    if ((s.cfg.w & 1) != 0 && dot(nrm, nrm) > 0.0) nrm *= inversesqrt(dot(nrm, nrm));
}

vec3 L3(vec3 v) { return s.lmat[0].xyz * v.x + s.lmat[1].xyz * v.y + s.lmat[2].xyz * v.z; }
vec3 C3(vec3 v) { return s.lcol[0].xyz * v.x + s.lcol[1].xyz * v.y + s.lcol[2].xyz * v.z; }

void main() {
    vec3 pos, nrm;
    Skin(false, a_p.xyz, a_n.xyz, pos, nrm);
    if ((s.cfg.w & 2) != 0) pos.y = s.mirror.z - pos.y;
    vec4 clip = S(S(S(S(s.m[0] * pos.x) + S(s.m[1] * pos.y)) + S(s.m[2] * pos.z)) + s.m[3]);
    float q = 1.0 / clip.w;
    if (abs(q) < 1.1754944e-38) q = 0.0;   // the VU flushes denormals: such a vertex lands on the offset

    // Diffuse: 3 directional lights + ambient, times the material and C.
    vec3 L = max(L3(nrm), vec3(0.0));
    vec3 lit = (s.lcol[3].xyz + C3(L) * s.diffuse.xyz) * a_c.rgb;
    float alpha = min(s.diffuse.w * a_c.a, 128.0);
    int variant = s.cfg.y, pass = int(b.mv.w);
    vec3 rgb = min(lit, vec3(255.0));
    vec2 st = vec2(a_p.w + s.uvo.x, a_n.w + s.uvo.y);
    if (variant != 0) {
        // Specular: reflection of the eye vector, power 2^(1008.x + 1).
        vec3 v = s.eye.xyz - pos;
        float pinv = inversesqrt(max(dot(v, v), 1e-30));
        float d = dot(vec3(s.lmat[0].w, s.lmat[1].w, s.lmat[2].w) - pos, nrm);
        vec3 R = 2.0 * nrm * d * pinv - v * pinv;
        vec3 Sraw = L3(R);
        vec3 S = max(Sraw, vec3(0.0));
        for (int j = 0; j <= s.cfg2.x; ++j) S *= S;
        vec3 sp = C3(S) * (s.c970.xyz * s.c1008.y);
        if (pass == 0) {
            if (variant == 1) {
                float I = gl_VertexIndex == 0 ? uintBitsToFloat(uint(s.cfg2.y)) : 255.0;
                rgb = min(min(lit, vec3(I)) + sp * a_c.rgb, vec3(255.0));
            } else if (variant == 3) {
                rgb = min(min(lit, vec3(255.0)) + sp * 128.0, vec3(255.0));
            }
        } else if (pass == 1) {
            rgb = min(sp * a_c.rgb, vec3(255.0));
            alpha = min(a_c.a, 128.0);
        } else {
            vec3 e = s.e1004[0].xyz * Sraw.x + s.e1004[1].xyz * Sraw.y + s.e1004[2].xyz * Sraw.z;
            st = vec2((e.x + 1.0) * 0.5 * s.env.y + s.env.x, (e.y + 1.0) * 0.5 * s.env.w + s.env.z);
            rgb = s.c1009.rgb;
            alpha = s.c1009.a;
        }
    }
    v_col = vec4(floor(rgb), floor(alpha));
    v_n = nrm;
    v_p = pos;
    v_c = a_c;
    v_st = st;
    v_fog = (s.cfg.w & 16) != 0 ? floor(clamp(s.fog.z + s.fog.w * q, s.fog.x, s.fog.y)) : float(s.cfg2.z);

    vec3 scr = s.offset.xyz + S(S(clip.xyz * q) * s.scale.xyz);
    scr.xy = trunc(scr.xy * 16.0) / 16.0;
    vec2 px = scr.xy - b.target.xy + 0.5;
    vec2 ndc = px * b.target.zw * 2.0 - 1.0;
    float depth = clamp(floor(scr.z) * b.zscale.x, 0.0, 1.0);
    // A saturated vertex (q flushed to 0) has no usable w: place it in screen space.
    const float cw = q == 0.0 || abs(clip.w) > 1e30 ? 1.0 : clip.w;
    gl_Position = vec4(ndc * cw, depth * cw, cw);

    // Motion vectors: the same vertex last frame (previous P, palette, matrix).
    v_cur = clip;
    v_prev = clip;
    if ((s.cfg.w & 4) != 0) {
        vec3 pp, pn;
        Skin(true, a_pp.xyz, a_n.xyz, pp, pn);
        if ((s.cfg.w & 8) != 0) pp.y = s.pmirror.z - pp.y;
        v_prev = s.pm[0] * pp.x + s.pm[1] * pp.y + s.pm[2] * pp.z + s.pm[3];
    }
}
