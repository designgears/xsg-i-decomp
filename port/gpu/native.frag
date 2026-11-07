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
} b;
layout(set = 0, binding = 1) uniform sampler2D tex;   // RGBA8, alpha = the GS byte (0x80 = 1.0)

layout(location = 0) in vec4 v_col;
layout(location = 1) in vec2 v_st;
layout(location = 2) in float v_fog;
layout(location = 0) out vec4 o_col;

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
}
