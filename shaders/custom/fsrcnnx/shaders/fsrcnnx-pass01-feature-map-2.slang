#version 450

// FSRCNNX_x2_8-0-4-1 - Ported from mpv GLSL to slang
// Pass 1: feature map 2
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
    vec4 res = vec4(0.0541447550058365,0.0088306749239564,-0.0112389577552676,-0.0127860950306058);
    res += vec4(0.0142660010606050,0.0137931071221828,0.0061188107356429,-0.0104134222492576) * dot(texture(Source, vTexCoord + vec2(-2,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0147292809560895,-0.0289912857115269,0.0266769435256720,0.0933856964111328) * dot(texture(Source, vTexCoord + vec2(-2,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.1734338253736496,0.1116316691040993,-0.1973157376050949,-0.0581855811178684) * dot(texture(Source, vTexCoord + vec2(-2,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0347507223486900,-0.0341566652059555,0.0061667622067034,0.0075258882716298) * dot(texture(Source, vTexCoord + vec2(-2,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0069884369149804,-0.0194250214844942,0.0080830128863454,-0.0036874092184007) * dot(texture(Source, vTexCoord + vec2(-2,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0233764201402664,0.0344744995236397,0.0162145942449570,0.0979529991745949) * dot(texture(Source, vTexCoord + vec2(-1,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1280796974897385,-0.1018339172005653,-0.0132977198809385,-0.0019474622095004) * dot(texture(Source, vTexCoord + vec2(-1,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.4286882579326630,0.1222677752375603,0.7046694159507751,0.0945475697517395) * dot(texture(Source, vTexCoord + vec2(-1,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1107441782951355,-0.0134433070197701,-0.0174900908023119,-0.1686445474624634) * dot(texture(Source, vTexCoord + vec2(-1,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0321478620171547,0.0065357843413949,0.0300805997103453,0.0420113280415535) * dot(texture(Source, vTexCoord + vec2(-1,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.1240341588854790,0.0950303301215172,-0.0129648456349969,-0.2681856453418732) * dot(texture(Source, vTexCoord + vec2(0,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.4846960902214050,0.0351924635469913,0.0223043337464333,-0.1273630708456039) * dot(texture(Source, vTexCoord + vec2(0,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-1.9379507303237915,-0.2444442063570023,0.0291962660849094,-0.3835578560829163) * dot(texture(Source, vTexCoord).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.6396278142929077,-0.0765938311815262,-0.0552659817039967,0.4393545985221863) * dot(texture(Source, vTexCoord + vec2(0,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.1969728022813797,-0.0607173256576061,0.0131113547831774,0.0542017817497253) * dot(texture(Source, vTexCoord + vec2(0,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0091696009039879,-0.0031533432193100,-0.0368777588009834,-0.0459998287260532) * dot(texture(Source, vTexCoord + vec2(1,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.1096992492675781,0.2597902715206146,0.0304869692772627,-0.0195200722664595) * dot(texture(Source, vTexCoord + vec2(1,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.2889648377895355,-0.4275591969490051,-0.7414156794548035,0.2695442438125610) * dot(texture(Source, vTexCoord + vec2(1,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0892018377780914,-0.0229137558490038,0.0244414471089840,-0.1926898956298828) * dot(texture(Source, vTexCoord + vec2(1,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0576358586549759,0.0027846973389387,-0.0036861505359411,-0.0253547113388777) * dot(texture(Source, vTexCoord + vec2(1,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0159624069929123,0.0319602824747562,0.0019470085389912,0.0089780492708087) * dot(texture(Source, vTexCoord + vec2(2,-2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0552792511880398,0.0543054342269897,0.0134062822908163,0.0545728243887424) * dot(texture(Source, vTexCoord + vec2(2,-1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.1170092225074768,0.1963327825069427,0.1503890156745911,0.1891828328371048) * dot(texture(Source, vTexCoord + vec2(2,0) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(-0.0084421783685684,0.1297017931938171,-0.0330600887537003,-0.0942063704133034) * dot(texture(Source, vTexCoord + vec2(2,1) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    res += vec4(0.0118440408259630,-0.0337875857949257,0.0055063469335437,0.0254479162395000) * dot(texture(Source, vTexCoord + vec2(2,2) * params.SourceSize.zw).rgb, vec3(0.299, 0.587, 0.114));
    FragColor = res;

}