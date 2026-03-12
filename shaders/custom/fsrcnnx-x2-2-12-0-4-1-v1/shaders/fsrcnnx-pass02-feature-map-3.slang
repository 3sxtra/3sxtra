#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 2: feature map 3
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
    vec4 res = vec4(0.0496259294450283,-0.0054626469500363,-0.0028826687484980,0.0245390739291906);
    res += vec4(-0.0139327803626657,-0.0107311122119427,0.0326866768300533,-0.0065727680921555) * dot(texture(Source, vTexCoord + vec2(-2,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0301438216120005,-0.0145515305921435,-0.0295667350292206,-0.0350141823291779) * dot(texture(Source, vTexCoord + vec2(-2,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0204928778111935,-0.0264455992728472,0.0119906561449170,-0.1034157052636147) * dot(texture(Source, vTexCoord + vec2(-2,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0119021432474256,0.0116225434467196,-0.0179430544376373,0.0227058473974466) * dot(texture(Source, vTexCoord + vec2(-2,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0069086048752069,0.0078157996758819,-0.0070084435865283,-0.0364718511700630) * dot(texture(Source, vTexCoord + vec2(-2,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0001247427571798,-0.0441221892833710,-0.0182254314422607,0.0015759583329782) * dot(texture(Source, vTexCoord + vec2(-1,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1810556650161743,0.0189144238829613,0.0432335957884789,-0.1630787402391434) * dot(texture(Source, vTexCoord + vec2(-1,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.2772021889686584,0.2336208075284958,-0.1638352423906326,0.3997396528720856) * dot(texture(Source, vTexCoord + vec2(-1,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0458508320152760,0.1730852425098419,-0.2024881392717361,0.0483242683112621) * dot(texture(Source, vTexCoord + vec2(-1,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0064747110009193,0.0196865722537041,0.0205670315772295,-0.0470002964138985) * dot(texture(Source, vTexCoord + vec2(-1,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0564243644475937,-0.0729262828826904,0.0793752297759056,0.2132518440485001) * dot(texture(Source, vTexCoord + vec2(0,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.3100465238094330,-0.2129869610071182,-0.0521022193133831,-0.3506218791007996) * dot(texture(Source, vTexCoord + vec2(0,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0277066696435213,0.0659971609711647,1.0559650659561157,-0.2683604359626770) * dot(texture(Source, vTexCoord).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.3337151408195496,0.3627733886241913,0.2408450841903687,0.2992235720157623) * dot(texture(Source, vTexCoord + vec2(0,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0166841316968203,0.0358852148056030,-0.0965528264641762,-0.2262587547302246) * dot(texture(Source, vTexCoord + vec2(0,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0088939471170306,-0.0092873983085155,-0.0069504007697105,0.0257117114961147) * dot(texture(Source, vTexCoord + vec2(1,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0258208494633436,-0.1687349677085876,-0.2330803722143173,0.0683359876275063) * dot(texture(Source, vTexCoord + vec2(1,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.2878983914852142,-0.2449259310960770,-0.5108366012573242,-0.1503494828939438) * dot(texture(Source, vTexCoord + vec2(1,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.1368679404258728,0.0229105670005083,-0.0342753566801548,0.1567915380001068) * dot(texture(Source, vTexCoord + vec2(1,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0255247931927443,-0.0020390425343066,-0.0613429136574268,0.0467975549399853) * dot(texture(Source, vTexCoord + vec2(1,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0083413794636726,-0.0186238456517458,-0.0146364243701100,0.0376012884080410) * dot(texture(Source, vTexCoord + vec2(2,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0002742194337770,-0.0078036263585091,0.0059567480348051,-0.0561616383492947) * dot(texture(Source, vTexCoord + vec2(2,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0109222335740924,-0.0682856813073158,0.0823259800672531,0.0285383369773626) * dot(texture(Source, vTexCoord + vec2(2,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0142541555687785,-0.0427635759115219,-0.1110669821500778,0.0462795905768871) * dot(texture(Source, vTexCoord + vec2(2,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0107144722715020,-0.0190379973500967,-0.0077217523939908,-0.0088313883170485) * dot(texture(Source, vTexCoord + vec2(2,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    FragColor = res;

}