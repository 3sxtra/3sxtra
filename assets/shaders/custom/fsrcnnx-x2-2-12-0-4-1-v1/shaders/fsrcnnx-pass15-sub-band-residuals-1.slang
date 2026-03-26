#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 15: sub-band residuals 1
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
layout(set = 0, binding = 4) uniform sampler2D MODEL3_1;
layout(set = 0, binding = 5) uniform sampler2D FEATURE1;

void main()
{
    vec4 res = vec4(-0.0643149688839912,-0.0576749593019485,-0.0408056937158108,-0.0185023397207260);
    res += mat4(0.1274719536304474,-0.1554331928491592,-0.1829464286565781,0.0781351029872894,-0.1543711423873901,0.0412564687430859,0.0998993292450905,-0.1202722117304802,0.0508668571710587,-0.5214651823043823,-0.0157972667366266,-0.1427432596683502,-0.0166169404983521,-0.0423728749155998,-0.1367035508155823,0.1895777732133865) * texture(MODEL1_1, vTexCoord);
    res += mat4(0.2312063276767731,0.1709280014038086,-0.1061000302433968,0.1818547695875168,-0.3313679993152618,0.4145560562610626,-0.1002107039093971,-0.1037807837128639,-0.1157908439636230,-0.0632109045982361,-0.1699832975864410,0.0091027254238725,0.0032862401567400,0.0246057100594044,0.2558397352695465,0.0909002795815468) * texture(MODEL2_1, vTexCoord);
    res += mat4(-0.2564980983734131,-0.1062827780842781,-0.0996602103114128,0.1038301885128021,0.0216037910431623,-0.0081060416996479,0.0169479716569185,-0.0179405491799116,-0.0052979793399572,-0.0154040623456240,0.0087776901200414,-0.0216414909809828,0.1552186608314514,-0.0341454669833183,-0.0034302233252674,0.0569461323320866) * texture(MODEL3_1, vTexCoord);
    res += texture(FEATURE1, vTexCoord);
    res = max(res, vec4(0.0)) + vec4(0.0093261189758778,0.0033607596997172,-0.0225459411740303,0.0883640125393867) * min(res, vec4(0.0));
    FragColor = res;

}