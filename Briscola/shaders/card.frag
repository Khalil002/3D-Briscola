#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 vUV;
layout(location = 1) flat in int vCardIndex;

layout(set = 0, binding = 1) uniform sampler2D uAtlas;
layout(set = 0, binding = 2) uniform sampler2D uBack;

layout(location = 0) out vec4 outColor;

vec2 getAtlasUV(vec2 uv, int cardIndex, int cols, int rows) {
    int col = cardIndex % cols;
    int row = cardIndex / cols;
    vec2 scale = vec2(1.0 / float(cols), 1.0 / float(rows));
    vec2 offset = vec2(float(col), float(row)) * scale;
    return uv * scale + offset;
}

void main() {
    if (gl_FrontFacing) {
        // Example: 10 columns × 4 rows = 40 cards
        vec2 atlasUV = getAtlasUV(vUV, vCardIndex, 10, 4);
        outColor = texture(uAtlas, atlasUV);
    } else {
        // Back face → back texture
        outColor = texture(uBack, vUV);
    }
}
