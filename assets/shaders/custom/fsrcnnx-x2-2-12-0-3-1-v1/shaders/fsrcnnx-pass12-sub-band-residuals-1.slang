#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 12: sub-band residuals 1
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
layout(set = 0, binding = 2) uniform sampler2D MODEL21_1;
layout(set = 0, binding = 3) uniform sampler2D MODEL22_1;
layout(set = 0, binding = 4) uniform sampler2D MODEL23_1;
layout(set = 0, binding = 5) uniform sampler2D FEATURE1;

void main()
{
    vec4 res = vec4(-0.0430866368114948,-0.1404373049736023,-0.0798836126923561,0.0419425442814827);
    res += mat4(0.0172950252890587,-0.6979768872261047,-0.0234361998736858,0.0830042138695717,0.0054362602531910,-0.0634286701679230,0.3570984601974487,-0.0049333171918988,0.2488382905721664,-0.4417920410633087,-0.8075768947601318,-0.1275192350149155,-0.1987060904502869,-0.3249325454235077,-0.0972941443324089,0.1155491769313812) * texture(MODEL21_1, vTexCoord);
    res += mat4(0.0635239183902740,-0.1098493635654449,0.0243946947157383,0.0308878831565380,0.0349893644452095,-0.1057806238532066,-0.2045549303293228,0.0268721394240856,-0.1032721325755119,0.0573472119867802,0.0137961618602276,-0.1465069204568863,0.0614347718656063,-0.2944710850715637,-0.3420190811157227,-0.0822718665003777) * texture(MODEL22_1, vTexCoord);
    res += mat4(0.0002673740382306,-0.1348736286163330,-0.0692818313837051,0.0410053580999374,-0.1539433300495148,-0.1825442463159561,0.2344257831573486,0.0158059317618608,0.1891757398843765,-1.4967734813690186,0.0293902289122343,0.0562157817184925,0.0876247137784958,0.0367805659770966,-0.0298327654600143,0.0789163559675217) * texture(MODEL23_1, vTexCoord);
    res += texture(FEATURE1, vTexCoord);
    res = max(res, vec4(0.0)) + vec4(0.9906274676322937,-0.0701753422617912,0.0098175881430507,-0.0492826364934444) * min(res, vec4(0.0));
    FragColor = res;

}