#version 330

out vec4 FragColor;

uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;

float PI = 3.1415926;

//math
float dot2( in vec2 v ) { return dot(v,v); }
float dot2( in vec3 v ) { return dot(v,v); }
float ndot( in vec2 a, in vec2 b ) { return a.x*b.x - a.y*b.y; }

struct Material {
    vec3 color;
    float reflectivity;
    float opticalDensity;
};

vec3 translate(vec3 pos, vec3 translation) {
    return pos - translation;
}

vec3 repeat(vec3 pos, vec3 spacing) {
    return mod(pos + 0.5 * spacing, spacing) - 0.5 * spacing;
}

vec3 rotate(vec3 pos, float angle, vec3 axis) {
    axis = normalize(axis);
    float cosA = cos(angle);
    float sinA = sin(angle);
    return mix(dot(pos, axis) * axis, pos, cosA)
    + cross(axis, pos) * sinA;
}


Material createMaterial(vec3 color, float reflectivity, float opticalDensity) {
    Material mat;
    mat.color = color;
    mat.reflectivity = reflectivity;
    mat.opticalDensity = opticalDensity;
    return mat;
}


float opSmoothUnion( float d1, float d2, float k )
{
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h);
}

//SDFs:
float SDFsphere(vec3 pos, float r){
    return(length(pos) - r);
}
float sdOctahedron( vec3 p, float s )
{
    p = abs(p);
    float m = p.x+p.y+p.z-s;
    vec3 q;
    if( 3.0*p.x < m ) q = p.xyz;
    else if( 3.0*p.y < m ) q = p.yzx;
    else if( 3.0*p.z < m ) q = p.zxy;
    else return m*0.57735027;

    float k = clamp(0.5*(q.z-q.y+s),0.0,s);
    return length(vec3(q.x,q.y-s+k,q.z-k));
}
float sdBox( vec3 p, vec3 b )
{
    vec3 q = abs(p) - b;
    return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float udQuad( vec3 p, vec3 a, vec3 b, vec3 c, vec3 d )
{
    vec3 ba = b - a; vec3 pa = p - a;
    vec3 cb = c - b; vec3 pb = p - b;
    vec3 dc = d - c; vec3 pc = p - c;
    vec3 ad = a - d; vec3 pd = p - d;
    vec3 nor = cross( ba, ad );

    return sqrt(
    (sign(dot(cross(ba,nor),pa)) +
    sign(dot(cross(cb,nor),pb)) +
    sign(dot(cross(dc,nor),pc)) +
    sign(dot(cross(ad,nor),pd))<3.0)
    ?
    min( min( min(
    dot2(ba*clamp(dot(ba,pa)/dot2(ba),0.0,1.0)-pa),
    dot2(cb*clamp(dot(cb,pb)/dot2(cb),0.0,1.0)-pb) ),
    dot2(dc*clamp(dot(dc,pc)/dot2(dc),0.0,1.0)-pc) ),
    dot2(ad*clamp(dot(ad,pd)/dot2(ad),0.0,1.0)-pd) )
    :
    dot(nor,pa)*dot(nor,pa)/dot2(nor) );
}
float sdTorus( vec3 p, vec2 t )
{
    vec2 q = vec2(length(p.xz)-t.x,p.y);
    return length(q)-t.y;
}

//final scene
vec2 scene(vec3 pos) {
    float sphere1 = SDFsphere(translate(pos, vec3(sin(u_time) * 3.0, -1.0 , cos(u_time) * 3.0)), 1.0);
    float octa = sdOctahedron(
        translate(
            rotate(pos, u_time* 10, vec3(0.0,1.0,0.0)), vec3(0.0, abs(tan(u_time* 4.0)) * 0.5,0.0)
        )
        ,abs(cos(u_time* 4.0)) * 2.0); //velikost
    vec3 p1 = vec3(-5, -2, -5);
    vec3 p2 = vec3(5, -2 , -5); // pri kameri
    vec3 p3 = vec3(5, -2, 5); //levo
    vec3 p4 = vec3(-5, -2 , 5);
    float quad = udQuad(pos, p1, p2, p3, p4);

    float torus = sdTorus( translate(pos, vec3(2.5,-1.0,2.5)), vec2(2.0,0.2));

    //materijali
    vec2 res = vec2(sphere1, 1.0);
    if (octa < res.x) res = vec2(octa, 2.0);
    if (quad < res.x) res = vec2(quad, 4.0);
    if (torus < res.x) res = vec2(torus, 3.0);
    return res;
}

// Function to retrieve material based on ID
Material getMaterial(float id) {
    if (id < 1.5) return createMaterial(vec3(0.8, 0.3, 0.2), 1., 1.0);
    if (id < 2.5) return createMaterial(vec3(0.2, 0.8, 0.3), 1., 1.0);
    if (id < 3.5) return createMaterial(vec3(0.6, 0.6, 0.0), 1., 1.0);
    return createMaterial(vec3(0.2, 0.3, 0.8), 0.4, 1.0);
}

//math funct
vec3 getNormal(vec3 p) {
    //chat
    vec2 e = vec2(0.001, 0.0);
    float d = scene(p).x;

    vec3 n = vec3(
    scene(p + vec3(e.x, e.y, e.y)).x - d,  // Partial derivative in X
    scene(p + vec3(e.y, e.x, e.y)).x - d,  // Partial derivative in Y
    scene(p + vec3(e.y, e.y, e.x)).x - d   // Partial derivative in Z
    );
    return normalize(n);
}

//RAYMARCHER:
#define MAX_STEPS 1000
#define MAX_DIST 1000.0
#define SURFACE_DIST 0.01
#define MAX_REFLECT 10

vec3 raymarch(vec3 ro, vec3 rd, int reflections) {
    float totalDist = 0.0;
    vec3 color = vec3(0.0);
    vec3 accumulatedColor = vec3(1.0);

    vec3 currentOrigin = ro;
    vec3 currentDirection = rd;

    for (int r = 0; r <= reflections; r++) {
        float dO = 0.0;
        bool hit = false;

        for (int i = 0; i < MAX_STEPS; i++) {
            vec3 pos = currentOrigin + currentDirection * dO;
            vec2 res = scene(pos); //id materijal dobis iz y
            float dS = res.x;
            float materialID = res.y;
            dO += dS;
            if (dS < SURFACE_DIST) {
                hit = true;
                totalDist += dO;

                Material mat = getMaterial(materialID);
                vec3 normal = getNormal(pos);
                currentDirection = reflect(currentDirection, normal);
                currentOrigin = pos + normal * SURFACE_DIST;
                accumulatedColor *= mat.color * mat.reflectivity;
                float depthFactor = 1.0 / (1.0 + float(r) * 0.5);
                color += accumulatedColor * depthFactor;
                break;
            }
            if (dO > MAX_DIST) break;
        }

        if (!hit) {
            vec3 backgroundColor = vec3(0.1, 0.2, 0.4);
            color += accumulatedColor * backgroundColor;
            break;
        }
    }

    return clamp(color, 0.0, 1.0);
}

vec3 reflect(vec3 incident, vec3 normal) {
    //chat
    return incident - 2.0 * dot(incident, normal) * normal;
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution;
    uv += -0.5;
    uv.x *= u_resolution.x / u_resolution.y; // aspect ratio
    vec2 mouse_norm = u_mouse/u_resolution;

    vec3 color = vec3(0.0);

    //camera logika
    float yaw = (mouse_norm.x - 0.5) * 2.0 * PI;
    float pitch = (mouse_norm.y - 0.5) * PI;
    vec3 ro = vec3(cos(u_time) * 5.0, 4.0, sin(u_time) * 5.0);
    vec3 forward = vec3(sin(yaw) * cos(pitch), sin(pitch), -cos(yaw) * cos(pitch));
    vec3 right = vec3(cos(yaw), 0.0, sin(yaw));

    vec3 up = cross(right, forward);
    vec3 rd = normalize(forward + uv.x * right + uv.y * up);

    color = raymarch(ro, rd, MAX_REFLECT);

    FragColor = vec4(color , 1.0);
}
