#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 16: sub-band residuals 2
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
    vec4 MODEL3_1Size;
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
layout(set = 0, binding = 4) uniform sampler2D MODEL3_1;
layout(set = 0, binding = 5) uniform sampler2D FEATURE2;

void main()
{
    vec4 res = vec4(-0.0172216109931469,0.0247540399432182,-0.0398628637194633,-0.0118948081508279);
    res += mat4(0.1906390339136124,0.0233912263065577,0.0174888335168362,-0.0901736915111542,0.1464671790599823,-0.0395766124129295,-0.0540083833038807,0.1566239297389984,-0.0517465062439442,-0.1743529438972473,-0.1013471409678459,0.1945663541555405,-0.0934700742363930,0.0630740523338318,0.0786507576704025,-0.0517205558717251) * texture(MODEL1_1, vTexCoord);
    res += mat4(-0.0706686750054359,0.0863402709364891,-0.0550284273922443,0.0135095743462443,0.1930631697177887,0.1408959478139877,-0.0748752728104591,-0.0043719923123717,0.0552369430661201,-0.0458237752318382,0.0187368579208851,-0.0559655837714672,-0.0162091571837664,-0.0056480150669813,-0.0753760263323784,0.1213253363966942) * texture(MODEL2_1, vTexCoord);
    res += mat4(-0.0118277128785849,-0.1656214594841003,-0.0029224010650069,0.0218585040420294,-0.0977983251214027,0.0101703274995089,0.0548275336623192,-0.0974680110812187,-0.0638912022113800,-0.0352693535387516,-0.0021437604445964,0.0435218587517738,-0.0016886305529624,0.0001100749504985,0.1489241719245911,-0.1303878575563431) * texture(MODEL3_1, vTexCoord);
    res += texture(FEATURE2, vTexCoord);
    res = max(res, vec4(0.0)) + vec4(0.7571665048599243,-0.5676655173301697,1.0025273561477661,1.1510212421417236) * min(res, vec4(0.0));
    FragColor = res;

}