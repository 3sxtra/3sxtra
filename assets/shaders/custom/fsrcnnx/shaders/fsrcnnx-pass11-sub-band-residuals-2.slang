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
    vec4 res = vec4(-0.0425243787467480,-0.3715015351772308,-0.0256227850914001,-0.2774516046047211);
    res += mat4(0.0238118842244148,0.0295480657368898,-0.0066418983042240,0.1021223962306976,-0.0568209178745747,-0.4355100393295288,-0.2700522541999817,-0.2060186564922333,-0.0689613372087479,-0.1689691990613937,-0.0306748505681753,-0.2461252212524414,-0.0057375836186111,-0.1892303228378296,-0.0285871494561434,-0.5032613277435303) * texture(MODEL1_1, vTexCoord);
    res += mat4(0.5463213324546814,0.0972800329327583,0.0307560767978430,0.0678058937191963,-0.0356063023209572,-0.7013865113258362,0.1890443563461304,-0.1036657467484474,-0.1745826154947281,-0.2942218780517578,-0.0485423319041729,-0.2983124554157257,-0.0524431839585304,-0.3261034786701202,0.3217246532440186,0.1958018541336060) * texture(MODEL2_1, vTexCoord);
    res += texture(FEATURE2, vTexCoord);
    res = max(res, vec4(0.0)) + vec4(0.1391339898109436,0.0960328355431557,0.6235341429710388,0.1177272796630859) * min(res, vec4(0.0));
    FragColor = res;

}