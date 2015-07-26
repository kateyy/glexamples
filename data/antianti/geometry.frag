#version 140

uniform vec3 camera;

flat in int g_vertexID;
in vec3 g_worldPos;
in vec3 g_N;
in vec3 g_N_face;
in vec3 g_L;
in vec3 g_E;
in vec2 g_T;

in vec4 g_S;

out vec4 fragColor;
out vec4 fragNormal;

uniform uint time;
uniform bool shadowsEnabled;
uniform bool linearizedShadowMap;
uniform vec2 lightZRange;

uniform float hasDiff;
uniform float hasNorm;
uniform float hasSpec;
uniform float hasEmis;

uniform sampler2D diff;
uniform sampler2D norm;
uniform sampler2D spec;
uniform sampler2D emis;

uniform sampler2D smap;

const float ambientFactor = 0.6;
const float diffuseFactor = 1.00;
const float specularFactor = 4.0;
const float emissionFactor = 0.33;


bool transparency_discard(int vertexID);
float calculateAlpha(uint mask);

// depth_util.frag
float linearize(float depth, vec2 zRange);

float lambert(vec3 n, vec3 l)
{
    return max(0.0, dot(l, n));
}

// taken from http://www.thetenthplanet.de/archives/1180
mat3 cotangent_frame( vec3 N, vec3 p, vec2 uv )
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx( p );
    vec3 dp2 = dFdy( p );
    vec2 duv1 = dFdx( uv );
    vec2 duv2 = dFdy( uv );
 
    // solve the linear system
    vec3 dp2perp = cross( dp2, N );
    vec3 dp1perp = cross( N, dp1 );
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
    return mat3( T * invmax, B * invmax, N );
}


void main()
{
    if (transparency_discard(g_vertexID))
        discard;

    vec2 uv = g_T;
    uv.y = uv.y;

    vec3 l = normalize(g_L);
    vec3 n_world = normalize(g_N);

    //vec3 normSample;
    //vec3 test;
    //if (hasNorm) {
    //    normSample = texture(norm, uv).xyz;
    //    test = vec3(1.0, 0.0, 0.0);
    //}
    //else {
    //    normSample = vec3(0.5, 0.5, 1.0);
    //    test = vec3(0.0, 1.0, 0.0);
    //}


	vec3 normSample;
	if (hasNorm > 0.5)
	{
		normSample = mix(vec3(0.5, 0.5, 1.0), texture(norm, uv).xyz, hasNorm);
    }
    else
    {
	    float moo = texture(norm, uv).r;
	    vec2 moo_tex = vec2(textureSize(norm, 0));
	    float moo_xx = texture(norm, uv + vec2(2.0/ moo_tex.x, 0.0)).r;
	    float moo_yy = texture(norm, uv + vec2(0.0, 2.0 / moo_tex.y)).r;
	    float moo_x =  moo_xx - moo;
	    float moo_y =  moo_yy - moo;
	    normSample = normalize(vec3(moo_x, moo_y, 1.0))  * 0.5 + 0.5;
    }

    //normSample = vec3(0.5, 0.500000, 1.0);
    vec3 n_tangent = normalize(normSample * 2.0 - 1.0);

    mat3 tbn = cotangent_frame(n_world, g_worldPos, uv);
    
    // ambient
    
    vec3 ambient = ambientFactor * vec3(0.434, 0.512, 0.668);

    // diffuse

    float l_face = lambert(l, n_world);
    float l_frag = lambert(l * tbn, n_tangent);

    float l_both = mix(l_face, l_frag, 1.0) ; // fakie fakie...
    
    vec3 treePlaneColor = vec3(50, 76, 63) / 255.0; // Megacity

    vec3 diffTexColor = texture(diff, uv).rgb;
    
    if ((hasDiff > 0.5) && (distance(diffTexColor, treePlaneColor) < 0.001))
        discard;
    
    vec3 diffSample = mix(vec3(g_N * 0.5 + 0.5), diffTexColor, hasDiff);
    vec3 diffuse = diffuseFactor * diffSample * l_both;

    // specular 

    vec3 c = normalize(g_E - camera);
    vec3 r_tangent = reflect(c * tbn, n_tangent);
    vec3 l_tangent = l * tbn;

    float ldotn = max(dot(l_tangent, n_tangent), 0.0); 
    float rdotn = max(dot(r_tangent, l_tangent), 0.0);

    float shininess = 2.0;
    vec3 specular = float(hasSpec) * specularFactor * texture(spec, uv).xyz * clamp(pow(rdotn, shininess), 0.0, 1.0);

    // emission 
    vec3 emission = float(hasEmis) * emissionFactor * texture(emis, uv).xyz;
    

    float shadow = 1.0;

    if (shadowsEnabled) {
        vec4 scoord = g_S / g_S.w;
            if (linearizedShadowMap)
                scoord.z = linearize(scoord.z, lightZRange);
        scoord.z -= 0.0001;
        //scoord.y = 1.0 - scoord.y;

        float sdist = texture(smap, scoord.xy).r;
        
        shadow = step(0.0, sign(g_S.w)) * step(scoord.z, sdist);
    }

    vec3 color = ambient * diffSample + (shadow) * (diffuse + diffuse * specular) + emission;

    fragColor = vec4(  vec3(color) , 1.0);
    fragNormal = vec4(g_N_face, 1.0);

    if (hasNorm < 0.5 && hasDiff < 0.5)
		fragColor = fragNormal * 0.5 + 0.5;
}
