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
    vec4 res = vec4(0.0113607570528984,-0.0105006471276283,0.0100348824635148,0.0133596109226346);
    res += vec4(0.0177628137171268,-0.0126087218523026,-0.0088967438787222,-0.0102580124512315) * dot(texture(Source, vTexCoord + vec2(-2,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0044593545608222,-0.0045495838858187,0.0613456554710865,0.0068385642953217) * dot(texture(Source, vTexCoord + vec2(-2,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0064060362055898,-0.0060563725419343,0.0554100014269352,0.1782049536705017) * dot(texture(Source, vTexCoord + vec2(-2,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0874216482043266,-0.0451561510562897,-0.0846164301037788,0.0418666154146194) * dot(texture(Source, vTexCoord + vec2(-2,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0244254004210234,0.0391888916492462,-0.0180687569081783,-0.0297124981880188) * dot(texture(Source, vTexCoord + vec2(-2,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0210701860487461,0.0488988496363163,-0.0111486641690135,-0.0492630824446678) * dot(texture(Source, vTexCoord + vec2(-1,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1461265832185745,-0.0284737683832645,-0.2063746452331543,-0.0656265169382095) * dot(texture(Source, vTexCoord + vec2(-1,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.3071222901344299,-0.0855146944522858,-0.6605976223945618,-0.2800565361976624) * dot(texture(Source, vTexCoord + vec2(-1,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1905650347471237,0.0799539610743523,0.0096526024863124,0.2373301088809967) * dot(texture(Source, vTexCoord + vec2(-1,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.1073149070143700,-0.0012930578086525,-0.0227871034294367,0.0002808148274198) * dot(texture(Source, vTexCoord + vec2(-1,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1985427886247635,-0.0960889533162117,0.0174822285771370,-0.0618476271629333) * dot(texture(Source, vTexCoord + vec2(0,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.2175396382808685,0.4818271100521088,0.1772113442420959,0.2636826634407043) * dot(texture(Source, vTexCoord + vec2(0,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0855126529932022,-0.5490865707397461,0.6680964827537537,0.0311413761228323) * dot(texture(Source, vTexCoord).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0168744921684265,0.0070880167186260,0.2848839163780212,-0.3187344372272491) * dot(texture(Source, vTexCoord + vec2(0,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.1514324843883514,0.0652331337332726,0.0860924497246742,0.0357779972255230) * dot(texture(Source, vTexCoord + vec2(0,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0359616316854954,-0.0774780660867691,-0.0153719680383801,0.0500944517552853) * dot(texture(Source, vTexCoord + vec2(1,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0331903584301472,0.1020270735025406,0.0873355492949486,-0.2461836785078049) * dot(texture(Source, vTexCoord + vec2(1,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1907140761613846,0.0038514174520969,-0.3299100995063782,0.2149865925312042) * dot(texture(Source, vTexCoord + vec2(1,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.2617568373680115,-0.0192136336117983,-0.1389757245779037,0.2062063962221146) * dot(texture(Source, vTexCoord + vec2(1,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1678439080715179,0.0449530817568302,-0.0334185212850571,-0.0437469072639942) * dot(texture(Source, vTexCoord + vec2(1,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0045408112928271,0.0038486721459776,0.0365526936948299,-0.0501669980585575) * dot(texture(Source, vTexCoord + vec2(2,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0892640426754951,0.0072101326659322,-0.0588505677878857,-0.0125531302765012) * dot(texture(Source, vTexCoord + vec2(2,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0323155820369720,-0.0273864008486271,0.0338236242532730,-0.1749554723501205) * dot(texture(Source, vTexCoord + vec2(2,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0590584389865398,0.0629845783114433,0.0424392484128475,0.0419366583228111) * dot(texture(Source, vTexCoord + vec2(2,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0597587972879410,0.0112949702888727,-0.0402389094233513,-0.0181667283177376) * dot(texture(Source, vTexCoord + vec2(2,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    FragColor = res;

}