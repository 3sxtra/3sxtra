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
    vec4 res = vec4(0.0245648548007011,-0.4467784762382507,0.0197526942938566,-0.0110000418499112);
    res += mat4(0.0302665308117867,-0.9262221455574036,-0.1161134764552116,-0.0506900474429131,0.2716045379638672,-0.0485871583223343,0.0044713355600834,-0.4274623394012451,0.0749531090259552,-0.3700785338878632,0.0350039415061474,-0.0540786534547806,-0.0607390031218529,-0.8019900321960449,0.0923245251178741,0.1258827745914459) * texture(MODEL1_1, vTexCoord);
    res += mat4(-0.0649135261774063,0.0815236791968346,0.0067334296181798,0.1277425885200500,-0.0051357815973461,-0.1485908329486847,0.0074226572178304,0.0050623500719666,0.0588018335402012,-0.0692552924156189,0.1288725286722183,-0.0989386290311813,0.0427936837077141,0.0967708528041840,-0.0455632135272026,-0.0711275041103363) * texture(MODEL2_1, vTexCoord);
    res += texture(FEATURE1, vTexCoord);
    res = max(res, vec4(0.0)) + vec4(0.9927186965942383,0.0570580027997494,1.3226752281188965,1.0069466829299927) * min(res, vec4(0.0));
    FragColor = res;

}