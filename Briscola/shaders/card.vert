#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;

// Instance attributes
layout(location = 3) in int inCardIndex;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec2 vUV;
layout(location = 1) flat out int vCardIndex;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    vUV = inUV;
    vCardIndex = inCardIndex;
}
