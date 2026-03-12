#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 0: feature map 1
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
layout(set = 0, binding = 2) uniform sampler2D Source;

void main()
{
    vec4 res = vec4(-0.1275466233491898,0.0152152702212334,0.0136403907090425,-0.0411372110247612);
    res += vec4(-0.0306390374898911,-0.0018572537228465,0.1089098379015923,0.0383838228881359) * dot(texture(Source, vTexCoord + vec2(-2,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0076138745062053,0.1845730692148209,-0.0557654984295368,-0.0044634994119406) * dot(texture(Source, vTexCoord + vec2(-2,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0167403426021338,0.2234024554491043,-0.0110784303396940,0.0148352775722742) * dot(texture(Source, vTexCoord + vec2(-2,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0113147469237447,0.0862987861037254,-0.0042494991794229,-0.0216839183121920) * dot(texture(Source, vTexCoord + vec2(-2,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0220282636582851,-0.0085032805800438,-0.0722964406013489,0.0409803055226803) * dot(texture(Source, vTexCoord + vec2(-2,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0202917959541082,-0.0520391575992107,-0.0383678749203682,-0.0390096753835678) * dot(texture(Source, vTexCoord + vec2(-1,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0908172652125359,0.0480716675519943,0.1606342345476151,0.1159007549285889) * dot(texture(Source, vTexCoord + vec2(-1,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1678475439548492,-0.2227642983198166,-0.2086166888475418,0.5652747750282288) * dot(texture(Source, vTexCoord + vec2(-1,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0531648397445679,-0.0526302382349968,0.1745014190673828,0.1720897555351257) * dot(texture(Source, vTexCoord + vec2(-1,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0093460213392973,-0.1545217335224152,0.1069174483418465,-0.0177406463772058) * dot(texture(Source, vTexCoord + vec2(-1,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0402536392211914,-0.1274782568216324,0.1862153559923172,-0.0568625666201115) * dot(texture(Source, vTexCoord + vec2(0,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.2047389000654221,-0.1615900397300720,0.0623472109436989,-0.0985199436545372) * dot(texture(Source, vTexCoord + vec2(0,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.4090168774127960,0.8198750019073486,0.1867096573114395,0.0432254225015640) * dot(texture(Source, vTexCoord).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1658263355493546,-0.5192563533782959,0.1509042829275131,-0.0895277187228203) * dot(texture(Source, vTexCoord + vec2(0,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0368501991033554,-0.0033033001236618,-0.3285988271236420,0.0077681872062385) * dot(texture(Source, vTexCoord + vec2(0,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0003060362359975,-0.0754906982183456,-0.0877383649349213,0.0704153776168823) * dot(texture(Source, vTexCoord + vec2(1,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0758231878280640,0.0121407564729452,-0.1004017293453217,-0.1118498966097832) * dot(texture(Source, vTexCoord + vec2(1,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.2045185565948486,-0.1409862339496613,-0.1454786360263824,-0.3740775883197784) * dot(texture(Source, vTexCoord + vec2(1,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0916302427649498,0.0238709598779678,-0.0904258713126183,-0.1816168874502182) * dot(texture(Source, vTexCoord + vec2(1,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0129315163940191,0.0335401147603989,-0.1257513910531998,-0.0377388261258602) * dot(texture(Source, vTexCoord + vec2(1,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0092872595414519,-0.1413606256246567,0.0067798076197505,0.0092190373688936) * dot(texture(Source, vTexCoord + vec2(2,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0067398985847831,0.0297972019761801,0.0972238779067993,0.0250104051083326) * dot(texture(Source, vTexCoord + vec2(2,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0497234761714935,0.1218791380524635,0.0955103561282158,-0.0306782722473145) * dot(texture(Source, vTexCoord + vec2(2,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0177344195544720,0.0362676344811916,0.0204550325870514,-0.0042170318774879) * dot(texture(Source, vTexCoord + vec2(2,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0031161343213171,-0.0028476081788540,-0.0993893370032310,0.0167021695524454) * dot(texture(Source, vTexCoord + vec2(2,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    FragColor = res;

}