#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D tex;

void main()
{
    vec2 uv = vec2(v_uv.x, 1.0 - v_uv.y);
    vec4 c = texture(tex, uv);
    fragColor = vec4(c.rgb * c.a, c.a);
}
