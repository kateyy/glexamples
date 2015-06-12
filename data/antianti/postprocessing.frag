#version 140

uniform sampler2D DepthTexture; 
uniform sampler2D ColorTexture;
uniform sampler2D NormalTexture;

uniform mat3 normalMatrix;
//uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 projectionInverseMatrix;

//uniform mat4 viewProjectionMatrix;

uniform float nearZ;
uniform float farZ;

uniform vec2 screenSize;


uniform bool useSSAO;

uniform int frame;
uniform sampler2D lastFrame;

const int Source_Final = 0;
const int Source_Color = 1;
const int Source_Normals = 3;
const int Source_Geometry = 4;
const int Source_Depth = 5;

int Source = Source_Final;

out vec4 outColor;

in vec2 v_uv;

vec3 ssao(in vec2 uv, vec3 ssaoColor);

void main()
{
    vec4 color;
    
    switch (Source)
    {
        case Source_Final:
            color = texture(ColorTexture, v_uv);
            if (useSSAO)
            {
                color.rgb *= ssao(v_uv, vec3(0.0));
            }
            break;
        case Source_Color:
            color = texture(ColorTexture, v_uv);
            break;
        case Source_Normals:
            vec3 normal = texture(NormalTexture, v_uv).xyz;
            
            color = vec4((normal+vec3(1.0))/2.0, 1.0);
            
            break;
        case Source_Depth:
            float depth = texture(DepthTexture, v_uv).r;

            float d = (nearZ*farZ/4.0) / (farZ-depth*(farZ-nearZ));

            color = vec4(d);
            break;
    }
    
    vec4 last = texture(lastFrame, v_uv);
    color = mix(last, color, 1.0/float(frame));
    
    color.a = 1.0;
    
    outColor = color;
}
