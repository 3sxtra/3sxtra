#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 10: sub-band residuals 1
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
    vec4 FEATURE1Size;
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
layout(set = 0, binding = 4) uniform sampler2D FEATURE1;

void main()
{
    vec4 res = vec4(-0.0653526633977890,0.0372839011251926,-0.0943703427910805,0.1114943772554398);
    res += mat4(0.0000380158235203,0.1380850970745087,0.0643891990184784,-0.0892903134226799,-0.0394837260246277,0.1216435879468918,-0.0500898882746696,0.0047975354827940,0.0006672664312646,0.0189263354986906,-0.0493743866682053,-0.2970330715179443,-0.0195789597928524,0.0437738560140133,0.0448027476668358,-0.1282175779342651) * texture(MODEL1_1, vTexCoord);
    res += mat4(-0.0669333934783936,-0.0305195692926645,-0.0460394471883774,0.0894932225346565,-0.0424035377800465,0.0613921992480755,-0.0783639550209045,0.1126127243041992,-0.0984074771404266,-0.0225051697343588,-0.0687031149864197,0.2151065766811371,-0.0165280047804117,-0.0774783864617348,0.0444195419549942,-0.0306710023432970) * texture(MODEL2_1, vTexCoord);
    res += texture(FEATURE1, vTexCoord);
    res = max(res, vec4(0.0)) + vec4(0.9877840876579285,0.0405866466462612,2.2439649105072021,0.3284466266632080) * min(res, vec4(0.0));
    FragColor = res;

}