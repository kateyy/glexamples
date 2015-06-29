#version 140

uniform vec3 camera;

in vec3 v_worldPos;
in vec3 v_N;
in vec3 v_L;
in vec3 v_E;
in vec2 v_T;

in vec4 v_S;

out vec4 fragColor;
out vec4 fragNormal;

uniform uint time;
uniform bool shadowsEnabled;
uniform bool linearizedShadowMap;
uniform vec2 lightZRange;

uniform sampler2D diff;
uniform sampler2D norm;
uniform sampler2D spec;
uniform sampler2D emis;

uniform sampler2D smap;

const float ambientFactor = 0.08;
const float diffuseFactor = 1.4;
const float specularFactor = 4.0;
const float emissionFactor = 0.33;


bool transparency_discard();
float calculateAlpha(uint mask);

// depth_util.frag
float linearize(float depth, vec2 zRange);

float lambert(vec3 n, vec3 l)
{
    return max(0.0, dot(l, n));
}

void main()
{
    if (transparency_discard())
        discard;

    vec2 uv = v_T;
    uv.y = uv.y;

    vec3 l = normalize(v_L);
    vec3 n_world = normalize(v_N);
    vec3 n_tangent = normalize(texture(norm, uv).xyz * 2.0 - 1.0);

     // get edge vectors of the pixel triangle
    vec3 dp1  = dFdx(v_worldPos);
    vec3 dp2  = dFdy(v_worldPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp1perp = cross( n_world, dp1 );
    vec3 dp2perp = cross( dp2, n_world );

    vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt( max( dot(t,t), dot(b,b) ) );
        
    mat3 tbn = mat3(t * invmax, b * invmax, n_world);
    
    // ambient
    
    vec3 ambient = ambientFactor * vec3(0.384, 0.512, 0.768);

    // diffuse

    float l_face = lambert(l, n_world);
    float l_frag = lambert(l * tbn, n_tangent);

    float l_both = mix(l_face, l_frag, 1.0); // fakie fakie...

    vec3 diffuse = diffuseFactor * texture(diff, uv).xyz * l_both;

    // specular 

    vec3 c = normalize(v_E - camera);
    vec3 r_tangent = reflect(c * tbn, n_tangent);
    vec3 l_tangent = l * tbn;

    float ldotn = max(dot(l_tangent, n_tangent), 0.0); 
    float rdotn = max(dot(r_tangent, l_tangent), 0.0);

    float shininess = 2.0;
    vec3 specular = specularFactor * texture(spec, uv).xyz * clamp(pow(rdotn, shininess), 0.0, 1.0);

    // emission 
    vec3 emission = emissionFactor * texture(emis, uv).xyz;
    

    float shadow = 1.0;

    if (shadowsEnabled) {
        vec4 scoord = v_S / v_S.w;
            if (linearizedShadowMap)
                scoord.z = linearize(scoord.z, lightZRange);
        scoord.z -= 0.0001;
        //scoord.y = 1.0 - scoord.y;

        float sdist = texture(smap, scoord.xy).r;
        
        shadow = step(0.0, sign(v_S.w)) * step(scoord.z, sdist);
    }

    vec3 color = shadow * (ambient + diffuse + diffuse * specular) + emission;

    fragColor = vec4(  vec3(color) , 1.0);
    fragNormal = vec4(v_N, 1.0);
}
