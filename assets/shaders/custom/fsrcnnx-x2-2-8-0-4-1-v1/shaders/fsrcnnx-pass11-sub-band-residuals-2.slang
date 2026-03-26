#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 11: sub-band residuals 2
// Original: Copyright (C) 2017-2021 igv (LGPL v3.0+)

layout(push_constant) uniform Push
{
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
} params;

layout(std140, set = 0, binding = 0) uniform UBO
{
    mat4 MVP;
    vec4 MODEL1_1Size;
    vec4 MODEL2_1Size;
    vec4 FEATURE2Size;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D MODEL1_1;
layout(set = 0, binding = 3) uniform sampler2D MODEL2_1;
layout(set = 0, binding = 4) uniform sampler2D FEATURE2;

void main()
{
    vec4 res = vec4(-0.0352841801941395,-0.0507849305868149,-0.0478690788149834,0.1702791452407837);
    res += mat4(-0.1236276403069496,-0.5335519313812256,-0.0695063471794128,-0.0114915864542127,-0.0540533885359764,-0.0888038724660873,-0.2306438386440277,-0.0026235629338771,-0.0007292276131921,-0.0173562914133072,0.0112944012507796,0.0202705077826977,0.0907734259963036,-0.0249151028692722,0.0209017992019653,-0.1129910200834274) * texture(MODEL1_1, vTexCoord);
    res += mat4(-0.1070277318358421,-0.0071275900118053,-0.0116497930139303,0.0744326859712601,0.0291691869497299,0.0112725952640176,-0.0504363738000393,0.1115738004446030,0.0312122646719217,-0.0645003840327263,0.0849588736891747,0.1091819256544113,-0.6924550533294678,-0.0424557439982891,0.0578757189214230,-0.0180510915815830) * texture(MODEL2_1, vTexCoord);
    res += texture(FEATURE2, vTexCoord);
    res = max(res, vec4(0.0)) + vec4(0.1004023551940918,0.4499900341033936,1.3149757385253906,0.1613716036081314) * min(res, vec4(0.0));
    FragColor = res;

}