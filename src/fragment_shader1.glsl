#version 330 core

out vec4 FragColor;

uniform vec2 u_resolution; // Screen resolution (width, height)
uniform vec2 u_mouse;      // Mouse position
uniform float u_time;      // Time (for animation)

// Signed distance function for a sphere
float sphereSDF(vec3 p, float r) {
    return length(sin(p)) - r;
}

// Signed distance function for a plane
float planeSDF(vec3 p, vec3 normal, float d) {
    return dot(p, normal) + d;
}

float sdLink( vec3 p, float le, float r1, float r2 )
{
    vec3 q = vec3( p.x, max(abs(p.y)-le,0.0), p.z );
    return length(vec2(length(q.xy)-r1,q.z)) - r2;
}

// Combine two SDFs with a union operator
float unionSDF(float d1, float d2) {
    return min(d1, d2);
}

float sceneSDF(vec3 p) {
//    float sphere = sdLink(p - vec3(0.0, 0.0 ,40.0 - sin(u_time) *200), abs(sin(u_time))*20.0, 10.0, 3.0);  // A sphere
    float sphere = sphereSDF(p - vec3(0.0, 0.0 ,0.0), abs(sin(u_time))*0.2);  // A sphere
    float plane = planeSDF(p, vec3(0.0, 0.0, 0.0), 1.0);      // A horizontal plane
    return sphere;
}

// Calculate the normal at a point using the gradient of the SDF
vec3 getNormal(vec3 p) {
    const float eps = 0.0001;
    return normalize(vec3(
    sceneSDF(p + vec3(eps, 0.0, 0.0)) - sceneSDF(p - vec3(eps, 0.0, 0.0)),
    sceneSDF(p + vec3(0.0, eps , 0.0)) - sceneSDF(p - vec3(0.0, eps, 0.0)),
    sceneSDF(p + vec3(0.0, 0.0, eps)) - sceneSDF(p - vec3(0.0, 0.0, eps))
    ));
}

// Ray-marching function
float rayMarch(vec3 ro, vec3 rd) {
    const float maxDist = 100.0;
    const float minDist = 0.01;
    const int maxSteps = 100;

    float dist = 0.0;
    for (int i = 0; i < maxSteps; i++) {
        vec3 p = ro + rd * dist;
        float d = sceneSDF(p);

        if (d < minDist) return dist; // Ray hit
        dist += d;
        if (dist > maxDist) break; // Exceeded max distance
    }
    return maxDist; // No hit
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution * 2.0 - 1.0;
    uv.x *= u_resolution.x / u_resolution.y; // Correct aspect ratio

    // Calculate camera orientation based on mouse position
    vec3 ro = vec3(sin(u_time*10) * 20 , sin(u_time*3) * 20 , cos(u_time*10) * 20); // Camera position
    float yaw = (u_mouse.x / u_resolution.x - 0.5) * -2.0 * 3.14159; // Horizontal rotation
    float pitch = (u_mouse.y / u_resolution.y - 0.5) * 3.14159;     // Vertical rotation

    vec3 forward = normalize(vec3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)));
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0 , 0.0)));
    vec3 up = cross(right, forward);

    vec3 rd = normalize(forward + right * uv.x + up * uv.y); // Ray direction

    // Ray march to find the intersection distance
    float dist = rayMarch(ro, rd);

    vec3 color = vec3(rd + vec3(0.6,0.6,0.6)); // Background color

    if (dist < 100.0) {
        vec3 hitPoint = ro + rd * dist;
        vec3 normal = getNormal(hitPoint);
        vec3 lightDir = normalize(vec3(-0.5 * sin(u_time * 5.0), 0.7, -0.5 * cos(u_time * 5.0)));
        float diff = max(dot(normal, lightDir), 0.3);
        color = vec3(0.4, 0.3, 0.9) * diff;
    }

    FragColor = vec4(color, 1.0);
}
