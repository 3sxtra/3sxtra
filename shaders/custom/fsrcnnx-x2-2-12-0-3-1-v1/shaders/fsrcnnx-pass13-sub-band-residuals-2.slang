#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 13: sub-band residuals 2
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
layout(set = 0, binding = 2) uniform sampler2D MODEL21_1;
layout(set = 0, binding = 3) uniform sampler2D MODEL22_1;
layout(set = 0, binding = 4) uniform sampler2D MODEL23_1;
layout(set = 0, binding = 5) uniform sampler2D FEATURE2;

void main()
{
    vec4 res = vec4(-0.0368862673640251,0.0298584997653961,-0.0639628320932388,-0.0445928387343884);
    res += mat4(-0.0661047250032425,-0.0652442649006844,-0.1017902418971062,0.0001330167578999,0.2190812826156616,0.0142258070409298,-0.0227435342967510,0.0193809773772955,-0.4393957555294037,-0.0038158607203513,-0.0686217695474625,-0.1763090938329697,-0.1269524097442627,-0.0035015076864511,-0.2232210785150528,-0.0905678942799568) * texture(MODEL21_1, vTexCoord);
    res += mat4(0.0259871035814285,-0.0595894344151020,-0.0489647313952446,-0.0407732091844082,-0.0660964697599411,-0.0458098836243153,-0.0181962661445141,-0.0464059747755527,0.0276352092623711,0.1532502323389053,-0.0138516975566745,0.1421557664871216,0.0473646596074104,0.0783308222889900,-0.3215684592723846,-0.0588528923690319) * texture(MODEL22_1, vTexCoord);
    res += mat4(0.2470564693212509,-0.0207288749516010,-0.8651476502418518,-0.0095216082409024,0.1095837056636810,0.0650400519371033,-0.2963815033435822,0.0533689633011818,-0.0807202458381653,-0.2464819997549057,0.0338921993970871,-0.0981582850217819,-0.0214540921151638,-0.1281112730503082,0.0233648568391800,0.1054920256137848) * texture(MODEL23_1, vTexCoord);
    res += texture(FEATURE2, vTexCoord);
    res = max(res, vec4(0.0)) + vec4(0.0667014941573143,0.9698160290718079,0.1120984405279160,-0.1779469251632690) * min(res, vec4(0.0));
    FragColor = res;

}