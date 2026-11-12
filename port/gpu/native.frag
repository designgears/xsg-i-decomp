#version 450
// Native renderer fragment stage: the GS texture function, alpha test and fog
// for one batch (TEX0/TEST/FOGCOL/ALPHA as the GS state tracker saw them).
layout(std140, set = 0, binding = 0) uniform Batch {
    vec4 m[4];
    vec4 scale;
    vec4 offset;
    vec4 cscale;
    vec4 uv;
    vec4 fog;
    vec4 target;
    vec4 zscale;
    vec4 fogcol;
    ivec4 mode;
    vec4 fix;
    ivec4 wrap;         // x: CLAMP.WMS, y: CLAMP.WMT, z: TEX1.MMAG (1 = linear)
    ivec4 region;       // CLAMP MINU/MSKU, MAXU/FIXU, MINV/MSKV, MAXV/FIXV
    vec4 mprev[4];      // previous frame's qw 962-965 (motion vectors)
    vec4 mv;            // xy: qw 966.xy * target scale (clip -> high-res pixels), z: 1 = has previous
    vec4 obj;           // x: the object id (actor + 1) / 65535, for the mouse's picking and outlines
} b;
layout(set = 0, binding = 1) uniform sampler2D tex;   // RGBA8, alpha = the GS byte (0x80 = 1.0)

layout(location = 0) in vec4 v_col;
layout(location = 1) in vec2 v_st;
layout(location = 2) in float v_fog;
layout(location = 3) in vec4 v_cur;    // VU clip position, this frame
layout(location = 4) in vec4 v_prev;   // the same point last frame
#ifdef SKIN_PPL
// Characters (set 2) with [renderer] lighting = perpixel: the VU1 programs' lighting
// (native_skin.vert) evaluated per pixel from the interpolated normal, pass 0 only.
layout(std140, set = 0, binding = 2) uniform SkinConst {
    vec4 m[4];
    vec4 scale;
    vec4 offset;
    vec4 lmat[3];
    vec4 lcol[4];
    vec4 diffuse;
    vec4 env;
    vec4 eye;
    vec4 c970;
    vec4 uvo;
    vec4 c1008;
    vec4 mirror;
    vec4 e1004[3];
    vec4 c1009;
    ivec4 cfg;
    ivec4 cfg2;
    ivec4 cfg3;
    vec4 pm[4];
    vec4 pmirror;
} s;
layout(location = 5) in vec3 v_n;
layout(location = 6) in vec3 v_p;
layout(location = 7) in vec4 v_c;
vec3 L3(vec3 v) { return s.lmat[0].xyz * v.x + s.lmat[1].xyz * v.y + s.lmat[2].xyz * v.z; }
vec3 C3(vec3 v) { return s.lcol[0].xyz * v.x + s.lcol[1].xyz * v.y + s.lcol[2].xyz * v.z; }
vec4 LitPerPixel(vec4 vertex_col) {
    // As the VU: the normal is renormalised only when the model asks for it (1008.z);
    // otherwise its length (scaled models) is part of the lighting.
    vec3 nrm = v_n;
    if ((s.cfg.w & 1) != 0 && dot(nrm, nrm) > 0.0) nrm = normalize(nrm);
    const vec3 L = max(L3(nrm), vec3(0.0));
    const vec3 lit = (s.lcol[3].xyz + C3(L) * s.diffuse.xyz) * v_c.rgb;
    vec3 rgb = min(lit, vec3(255.0));
    const int variant = s.cfg.y;
    if (variant == 1 || variant == 3) {
        vec3 v = s.eye.xyz - v_p;
        float pinv = inversesqrt(max(dot(v, v), 1e-30));
        float d = dot(vec3(s.lmat[0].w, s.lmat[1].w, s.lmat[2].w) - v_p, nrm);
        vec3 R = 2.0 * nrm * d * pinv - v * pinv;
        vec3 S = max(L3(R), vec3(0.0));
        for (int j = 0; j <= s.cfg2.x; ++j) S *= S;
        vec3 sp = C3(S) * (s.c970.xyz * s.c1008.y);
        rgb = variant == 1 ? min(min(lit, vec3(255.0)) + sp * v_c.rgb, vec3(255.0))
                           : min(min(lit, vec3(255.0)) + sp * 128.0, vec3(255.0));
    }
    return vec4(floor(rgb), vertex_col.a);
}
#endif
layout(location = 0) out vec4 o_col;
layout(location = 1) out vec2 o_mv;    // motion vector, high-res pixels (current - previous)
layout(location = 2) out float o_id;   // the object id (port/docs/MOUSE.md), R16 UNORM

// One texel with the GS wrap modes (CLAMP): 0 repeat, 1 clamp, 2 region clamp
// (MIN..MAX), 3 region repeat ((i & MSK) | FIX).
int Wrap(int i, int size, int mode, int lo, int hi) {
    if (mode == 0) return i & (size - 1);
    if (mode == 1) return clamp(i, 0, size - 1);
    if (mode == 2) return clamp(i, lo, hi);
    return (i & lo) | hi;
}
vec4 Texel(ivec2 i, ivec2 size) {
    return texelFetch(tex, ivec2(Wrap(i.x, size.x, b.wrap.x, b.region.x, b.region.y),
                                 Wrap(i.y, size.y, b.wrap.y, b.region.z, b.region.w)), 0);
}
// The GS texture lookup: texel space, nearest or bilinear (4-bit fraction).
vec4 Sample(vec2 st) {
    ivec2 size = textureSize(tex, 0);
    vec2 uv = st * vec2(size);
    if (b.wrap.z == 0) return Texel(ivec2(floor(uv)), size);
    uv -= 0.5;
    vec2 i0 = floor(uv);
    vec2 f = floor((uv - i0) * 16.0) / 16.0;
    ivec2 i = ivec2(i0);
    vec4 a = mix(Texel(i, size), Texel(i + ivec2(1, 0), size), f.x);
    vec4 c = mix(Texel(i + ivec2(0, 1), size), Texel(i + ivec2(1, 1), size), f.x);
    return mix(a, c, f.y);
}

void main() {
    int m = b.mode.x;
    int tfx = m & 3;
    bool tcc = (m & 4) != 0, tme = (m & 8) != 0, fge = (m & 16) != 0;
    vec4 c = v_col;   // 0..255
#ifdef SKIN_PPL
    if (s.cfg.z != 0 && b.mv.w == 0.0) c = LitPerPixel(v_col);
#endif
    if (tme) {
        vec4 t = floor(Sample(v_st) * 255.0 + 0.5);
        if (tfx == 0) {
            c.rgb = min(floor(t.rgb * c.rgb / 128.0), vec3(255.0));
            if (tcc) c.a = min(floor(t.a * c.a / 128.0), 255.0);
        } else if (tfx == 1) {
            c.rgb = t.rgb;
            if (tcc) c.a = t.a;
        } else {
            c.rgb = min(floor(t.rgb * c.rgb / 128.0) + c.a, vec3(255.0));
            if (tcc) c.a = tfx == 2 ? min(t.a + c.a, 255.0) : t.a;
        }
    }
    int at = b.mode.y;
    if ((at & 1) != 0) {
        int atst = (at >> 1) & 7, afail = (at >> 4) & 3;
        float aref = float(b.mode.z);
        bool pass = atst == 1 || (atst == 2 && c.a < aref) || (atst == 3 && c.a <= aref) ||
                    (atst == 4 && c.a == aref) || (atst == 5 && c.a >= aref) || (atst == 6 && c.a > aref) ||
                    (atst == 7 && c.a != aref);
        if (!pass && afail != 1) discard;   // FB_ONLY (1) is approximated as a pass
    }
    if (fge) c.rgb = floor((v_fog * c.rgb + (255.0 - v_fog) * b.fogcol.rgb) / 256.0);
    float a = b.mode.w == 2 ? b.fix.x : c.a;
    o_col = vec4(c.rgb / 255.0, min(a / 128.0, 1.0));
    o_mv = v_prev.w > 0.0 && v_cur.w > 0.0 ? (v_cur.xy / v_cur.w - v_prev.xy / v_prev.w) * b.mv.xy : vec2(0.0);
    o_id = b.obj.x;
}
