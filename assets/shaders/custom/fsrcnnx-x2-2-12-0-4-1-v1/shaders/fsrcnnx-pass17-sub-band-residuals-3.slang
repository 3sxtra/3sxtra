#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 17: sub-band residuals 3
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
layout(set = 0, binding = 2) uniform sampler2D MODEL1_1;
layout(set = 0, binding = 3) uniform sampler2D MODEL2_1;
layout(set = 0, binding = 4) uniform sampler2D MODEL3_1;
layout(set = 0, binding = 5) uniform sampler2D FEATURE3;

void main()
{
    vec4 res = vec4(-0.0522450618445873,0.0038620673585683,-0.0241306982934475,-0.0224116425961256);
    res += mat4(0.0394919365644455,-0.0035245516337454,0.0583688244223595,0.0325629450380802,-0.0824199542403221,0.1055598929524422,-0.0728916153311729,0.2064977437257767,-0.0092217922210693,0.1804540604352951,-0.2109112590551376,-0.1102742031216621,-0.0696008577942848,-0.0033466434106231,-0.0824781656265259,-0.1026663407683372) * texture(MODEL1_1, vTexCoord);
    res += mat4(0.0202881656587124,-0.0003871728549711,0.1865404397249222,-0.0316385738551617,-0.0457314960658550,-0.0539189502596855,0.0959941297769547,-0.0894097536802292,-0.0408547744154930,0.0249312538653612,0.0552539527416229,-0.0957197025418282,0.0393722020089626,0.0185600817203522,-0.0412260778248310,0.0377961359918118) * texture(MODEL2_1, vTexCoord);
    res += mat4(-0.0082626855000854,0.0049936780706048,-0.1425205022096634,-0.0797016248106956,0.0087543306872249,-0.0804564580321312,-0.0047904299572110,0.1292443275451660,-0.0162828546017408,0.0242217537015676,-0.0656456723809242,-0.0898852497339249,0.0299642048776150,0.0028474368155003,-0.0498579666018486,0.0811447501182556) * texture(MODEL3_1, vTexCoord);
    res += texture(FEATURE3, vTexCoord);
    res = max(res, vec4(0.0)) + vec4(0.7778466939926147,1.0444278717041016,-0.1113424450159073,0.3541233241558075) * min(res, vec4(0.0));
    FragColor = res;

}