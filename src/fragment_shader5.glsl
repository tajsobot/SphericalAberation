#version 330

out vec4 FragColor;

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

const float PI = 3.1415926535;

float y(float t, float x, float fi){
    float k = 1;
    float w = 2 * PI;
    float y0 = 1;
    return y0 * sin(k*x + w*t + fi);
}

float map(float x, float inMin, float inMax, float outMin, float outMax) {
    return outMin + (outMax - outMin) * (x - inMin) / (inMax - inMin);
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution;

    float scale = 10;

    float y_a = y(u_time, uv.x * scale, 0);

    vec3 c = vec3(map(y_a, -1, 1 , 0.01 , 1.0), 0.0, 0.0);

    FragColor = vec4(c, 1.0);
}
