#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec4 FgColor;

uniform sampler2D Source;
uniform sampler2D NativeDepthTex;
uniform vec2 Resolution;

void main()
{
    vec2 screen_uv = gl_FragCoord.xy / Resolution;
    float native_depth = texture(NativeDepthTex, screen_uv).r;

    // ⚡ Bolt: Z is sorted back-to-front. 0.0 is BACK plane, 1.0 is FRONT plane.
    // If the HD fragment's Z is LESS than the Native Depth, it means it is BEHIND it!
    if (gl_FragCoord.z < native_depth - 0.0001) {
        discard;
    }

    FragColor = texture(Source, TexCoord) * FgColor;
    if (FragColor.a < 0.01) discard;
}
