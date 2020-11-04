#ifdef GL_ES
precision mediump float;
#endif


uniform mat3      worldToOutput;

uniform sampler2D inputImage[2];
uniform mat3      outputToInput[2];

uniform bool bhue;
uniform bool bsat;
uniform bool blum;
uniform float balpha;


// Blending calculations from:
// https://www.khronos.org/registry/OpenGL/extensions/KHR/KHR_blend_equation_advanced.txt

float minv3(vec3 c)
{
  return min(min(c.r, c.g), c.b);
}

float maxv3(vec3 c)
{
  return max(max(c.r, c.g), c.b);
}

float lumv3(vec3 c)
{
  return dot(c, vec3(0.30, 0.59, 0.11));
}

float satv3(vec3 c)
{
  return maxv3(c) - minv3(c);
}

// If any color components are outside [0,1], adjust the color to get the components in range.
vec3 ClipColor(vec3 color)
{
  float lum = lumv3(color);
  float mincol = minv3(color);
  float maxcol = maxv3(color);
  if (mincol < 0.0) {
    color = lum + ((color-lum) * lum) / (lum-mincol);
  }
  if (maxcol > 1.0) {
    color = lum + ((color-lum) * (1.0 -lum)) / (maxcol-lum);
  }
  return color;
}

// Take the base RGB color <cbase> and override its luminosity
// with that of the RGB color <clum>.
vec3 SetLum(vec3 cbase, vec3 clum)
{
  float lbase = lumv3(cbase);
  float llum = lumv3(clum);
  float ldiff = llum - lbase;
  vec3 color = cbase + vec3(ldiff);
  return ClipColor(color);
}

// Take the base RGB color <cbase> and override its saturation with
// that of the RGB color <csat>.  The override the luminosity of the
// result with that of the RGB color <clum>.
vec3 SetLumSat(vec3 cbase, vec3 csat, vec3 clum)
{
  float minbase = minv3(cbase);
  float sbase = satv3(cbase);
  float ssat = satv3(csat);
  vec3 color;
  if (sbase > 0.0) {
    // Equivalent (modulo rounding errors) to setting the
    // smallest (R,G,B) component to 0, the largest to <ssat>,
    // and interpolating the "middle" component based on its
    // original value relative to the smallest/largest.
    color = (cbase - minbase) * ssat / sbase;
  } else {
    color = vec3(0.0);
  }
  return SetLum(color, clum);
}


void main( void )
{
  // Read sources
  vec2 s_texPos = (outputToInput[0] * vec3(gl_FragCoord.xy, 1.0)).xy;
  vec2 d_texPos = (outputToInput[1] * vec3(gl_FragCoord.xy, 1.0)).xy;
  vec4 s_frag = texture2D(inputImage[0], s_texPos);
  vec4 d_frag = texture2D(inputImage[1], d_texPos);

  // De-premultiplication
  vec3 s_pix = vec3(0.0);
  if (s_frag.a > 0.0) s_pix = s_frag.rgb / s_frag.a;
  vec3 d_pix = vec3(0.0);
  if (d_frag.a > 0.0) d_pix = d_frag.rgb / d_frag.a;

  // Figure out output alpha
  float s_alpha = s_frag.a * balpha;
  float d_alpha = d_frag.a;
  gl_FragColor.a = s_alpha + d_alpha * (1.0 - s_alpha);
  if (gl_FragColor.a <= 0.0) discard;

  // Perform blending
  if (s_alpha > 0.0 && d_alpha > 0.0) {
    vec3 o_pix = SetLumSat(bhue ? s_pix : d_pix, bsat ? s_pix : d_pix, blum ? s_pix : d_pix);
    gl_FragColor.rgb = mix(d_pix, o_pix, balpha);
  } else if (s_alpha > 0.0) {
    gl_FragColor.rgb = s_pix;
  } else {
    gl_FragColor.rgb = d_pix;
  }

  // Premultiplication
  gl_FragColor.rgb *= gl_FragColor.a;
}
