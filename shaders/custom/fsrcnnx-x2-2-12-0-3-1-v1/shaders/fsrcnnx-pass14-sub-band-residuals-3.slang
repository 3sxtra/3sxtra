#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 14: sub-band residuals 3
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
    vec4 MODEL21_1Size;
    vec4 MODEL22_1Size;
    vec4 MODEL23_1Size;
    vec4 FEATURE3Size;
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
layout(set = 0, binding = 2) uniform sampler2D MODEL21_1;
layout(set = 0, binding = 3) uniform sampler2D MODEL22_1;
layout(set = 0, binding = 4) uniform sampler2D MODEL23_1;
layout(set = 0, binding = 5) uniform sampler2D FEATURE3;

void main()
{
    vec4 res = vec4(-0.0194628126919270,0.0384849794209003,-0.1370671689510345,-0.0289741884917021);
    res += mat4(-0.0247990153729916,0.0008049020543694,-0.8340780138969421,0.0104062678292394,-0.0006987675442360,-0.3214502036571503,0.0525814667344093,0.0221717450767756,0.0168134737759829,0.2826166450977325,-0.9196262359619141,0.3298948705196381,-0.0580113492906094,-0.0984214171767235,-0.2152282893657684,-0.2102040201425552) * texture(MODEL21_1, vTexCoord);
    res += mat4(-0.0175723284482956,0.0134424678981304,0.3411416113376617,-0.0278677064925432,0.0203363411128521,-0.0167787279933691,-0.8426642417907715,-0.0001701831643004,0.0017653023824096,0.0055606700479984,0.0235847812145948,0.0960469990968704,-0.0247149746865034,0.2151234447956085,-0.1565432697534561,0.1210755184292793) * texture(MODEL22_1, vTexCoord);
    res += mat4(-0.0715659484267235,0.0129982633516192,-0.3135857284069061,-0.0173337515443563,-0.0424073599278927,-0.0554904229938984,0.0182892531156540,-0.0560968182981014,-0.0578930862247944,0.0105051333084702,-0.6593804359436035,-0.0334414169192314,0.0005281314952299,-0.0099297771230340,-0.1703903675079346,-0.0677873715758324) * texture(MODEL23_1, vTexCoord);
    res += texture(FEATURE3, vTexCoord);
    res = max(res, vec4(0.0)) + vec4(0.6862429976463318,-0.3698577880859375,0.0466018654406071,0.9292247891426086) * min(res, vec4(0.0));
    FragColor = res;

}