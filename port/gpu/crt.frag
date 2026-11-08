#version 450
// CRT filter ([display] crt, port/docs/MENU.md): the picture as the game was made
// to be seen, on an interlaced tube television.  Its 2D art (the fonts, the HUD,
// the window patterns) relies on that: neighbouring lines blended by the two
// fields, beams that bloom on bright content, a soft composite signal.
//
// Per output pixel:
//   1. curvature: the screen's bulge (crt_curvature);
//   2. beams: the image is sampled at the PS2's line structure, not at the
//      window's.  With interlace blending each beam is one field line and
//      averages the frame's odd and even line (the fonts' striped fill turns
//      solid, as on a TV); a beam's height grows with its brightness;
//      crt_scanlines sets how dark the gaps between beams are;
//   3. composite softening: a horizontal gaussian across the line (strongest on
//      "consumer"), the signal's limited bandwidth;
//   4. phosphor mask at the window's pixels: aperture grille (trinitron, pvm),
//      slot mask (consumer) or dot triads (shadow); crt_mask;
//   5. glow: bloom (a short soft spread of bright areas) and halation (the wide
//      glow of light scattered in the glass);
//   6. crt_strength mixes with the plain picture.
// Works in linear light; the swapchain is UNORM, so the output is encoded here.
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_col;
layout(set = 0, binding = 0) uniform sampler2D img;   // the game image (any size), linear filtering

layout(push_constant) uniform P {
    vec2 lines;      // x: frame lines at 1x (448), y: frame width at 1x (512)
    vec2 outsz;      // the game rectangle in window pixels
    float scan;      // gap darkness 0..1
    float mask;      // mask strength 0..1
    float halation;  // 0..1
    float bloom;     // 0..1
    float curvature; // 0..0.25
    float blur;      // composite softening, in 1x pixels (sigma)
    float strength;  // 0..1
    int masktype;    // 0 none, 1 aperture grille, 2 slot, 3 dots, 4 fine aperture (pvm)
    int interlace;   // 1: one beam per field line, averaging odd and even lines
} p;

vec3 Lin(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }

// The signal along a line at height y (uv): composite softening across x, and
// with interlace the line pair the beam covers.
vec3 Line(vec2 uv) {
    const float px = 1.0 / p.lines.y, py = 1.0 / p.lines.x;
    const float sig = max(p.blur, 0.05);
    vec3 acc = vec3(0.0);
    float ws = 0.0;
    for (int i = -3; i <= 3; ++i) {
        const float dx = float(i) * 0.5;
        const float g = exp(-dx * dx / (2.0 * sig * sig));
        vec2 t = vec2(uv.x + dx * px, uv.y);
        vec3 c = p.interlace != 0 ? 0.5 * (Lin(texture(img, t - vec2(0.0, 0.5 * py)).rgb) +
                                           Lin(texture(img, t + vec2(0.0, 0.5 * py)).rgb))
                                  : Lin(texture(img, t).rgb);
        acc += g * c;
        ws += g;
    }
    return acc / ws;
}

// A beam's light at distance d (in beam spacings) from its centre; brighter
// beams are taller.
vec3 Beam(float d, vec3 c) {
    const vec3 w = mix(vec3(0.28), vec3(0.50), sqrt(clamp(c, 0.0, 1.0)));
    const vec3 x = vec3(d) / w;
    return exp(-x * x);
}

vec3 Mask(vec2 fc) {
    if (p.masktype == 0 || p.mask <= 0.0) return vec3(1.0);
    const float lo = 1.0 - 0.6 * p.mask, hi = 1.0 + 0.35 * p.mask;
    int x = int(fc.x), y = int(fc.y);
    int k;
    if (p.masktype == 1) k = x % 3;                                    // aperture grille: RGB stripes
    else if (p.masktype == 4) k = (x / 1) % 3;                         // pvm: the same, finer pitch below
    else if (p.masktype == 2) {                                        // slot mask: stripes broken every 3 rows, offset
        k = (x + ((y / 3) & 1) * 3) % 6 / 2;
        if (y % 3 == 2) return vec3(lo);
    } else k = (x + (y & 1) * 2) % 3;                                  // dot triads, staggered rows
    vec3 m = vec3(lo);
    m[k] = hi;
    return m;
}

void main() {
    // 1. curvature
    vec2 uv = v_uv * 2.0 - 1.0;
    uv *= 1.0 + p.curvature * (uv.yx * uv.yx);
    uv /= 1.0 + p.curvature;   // keep the corners' scale near 1
    vec2 edge = smoothstep(vec2(1.0), vec2(1.0 - 4.0 / p.outsz), abs(uv));
    uv = uv * 0.5 + 0.5;
    if (any(greaterThan(abs(uv - 0.5), vec2(0.5)))) { o_col = vec4(0.0, 0.0, 0.0, 1.0); return; }

    // 2. beams at the field (or frame) line structure
    const float nl = p.interlace != 0 ? p.lines.x * 0.5 : p.lines.x;
    const float ly = uv.y * nl - 0.5;
    const float l0 = floor(ly), f = ly - l0;
    const vec3 c0 = Line(vec2(uv.x, (l0 + 0.5) / nl));
    const vec3 c1 = Line(vec2(uv.x, (l0 + 1.5) / nl));
    const vec3 flat_ = mix(c0, c1, f);
    vec3 beams = c0 * Beam(f, c0) + c1 * Beam(1.0 - f, c1);
    beams *= 1.0 + 0.6 * p.scan;   // a darker gap is a brighter beam
    vec3 col = mix(flat_, beams, p.scan);

    // 4. phosphor mask
    vec2 fc = gl_FragCoord.xy;
    if (p.masktype == 4) fc *= 1.5;   // pvm: finer pitch
    col *= Mask(fc);

    // 5. glow: bloom (radius ~1.5 px at 1x) and halation (~6 px)
    if (p.bloom > 0.0 || p.halation > 0.0) {
        const vec2 px = 1.0 / p.lines.yx;
        vec3 b = vec3(0.0), h = vec3(0.0);
        for (int i = 0; i < 8; ++i) {
            const float a = float(i) * 0.785398;
            const vec2 d = vec2(cos(a), sin(a));
            b += Lin(texture(img, uv + d * px * 1.5).rgb);
            h += Lin(texture(img, uv + d * px * 6.0).rgb);
        }
        b /= 8.0;
        h /= 8.0;
        col += p.bloom * 0.45 * b * b + p.halation * 0.35 * h;
        col /= 1.0 + 0.15 * (p.bloom + p.halation);
    }

    // 6. strength against the plain picture; edges of the curved screen fade out
    const vec3 plain = Lin(texture(img, v_uv).rgb);
    col = mix(plain, col, p.strength) * (p.curvature > 0.0 ? edge.x * edge.y : 1.0);
    o_col = vec4(pow(clamp(col, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
}
