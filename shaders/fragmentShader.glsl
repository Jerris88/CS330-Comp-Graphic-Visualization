#version 330 core
out vec4 fragmentColor;

in vec3 fragmentPosition;
in vec3 fragmentVertexNormal;
in vec2 fragmentTextureCoordinate;

struct Material {
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
}; 

struct DirectionalLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    bool bActive;
};

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    bool bActive;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;       
    bool bActive;
};

#define TOTAL_POINT_LIGHTS 5

uniform bool bUseTexture = false;
uniform bool bUseLighting = false;
uniform vec4 objectColor = vec4(1.0);
uniform vec3 viewPosition;
uniform DirectionalLight directionalLight;
uniform PointLight pointLights[TOTAL_POINT_LIGHTS];
uniform SpotLight spotLight;
uniform Material material;
uniform sampler2D objectTexture;
uniform vec2 UVscale = vec2(1.0, 1.0);

void CalcDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir, out vec3 ad, out vec3 sp);
void CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, out vec3 ad, out vec3 sp);
void CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, out vec3 ad, out vec3 sp);

void main()
{
    vec4 texSample = bUseTexture
        ? texture(objectTexture, fragmentTextureCoordinate * UVscale)
        : vec4(objectColor.rgb, objectColor.a);

    vec3 baseColor = bUseTexture ? texSample.rgb : objectColor.rgb;
    float baseAlpha = bUseTexture ? texSample.a : objectColor.a;

    if (!bUseLighting) {
        fragmentColor = vec4(baseColor * objectColor.rgb, baseAlpha * objectColor.a);
        return;
    }

    vec3 norm = normalize(fragmentVertexNormal);
    vec3 viewDir = normalize(viewPosition - fragmentPosition);

    vec3 adSum = vec3(0.0);
    vec3 spSum = vec3(0.0);

    if (directionalLight.bActive) {
        vec3 ad, sp; CalcDirectionalLight(directionalLight, norm, viewDir, ad, sp);
        adSum += ad; spSum += sp;
    }

    for (int i = 0; i < TOTAL_POINT_LIGHTS; ++i) {
        if (pointLights[i].bActive) {
            vec3 ad, sp; CalcPointLight(pointLights[i], norm, fragmentPosition, viewDir, ad, sp);
            adSum += ad; spSum += sp;
        }
    }

    if (spotLight.bActive) {
        vec3 ad, sp; CalcSpotLight(spotLight, norm, fragmentPosition, viewDir, ad, sp);
        adSum += ad; spSum += sp;
    }

    vec3 litRGB = adSum * baseColor + spSum;
    float outA = baseAlpha * objectColor.a;

    fragmentColor = vec4(litRGB, outA);
}

void CalcDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir, out vec3 ad, out vec3 sp)
{
    vec3 L = normalize(-light.direction);
    float diff = max(dot(normal, L), 0.0);

    vec3 reflectDir = reflect(-L, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    vec3 ambient = light.ambient;
    vec3 diffuse = light.diffuse * diff * material.diffuseColor;
    vec3 specular = light.specular * spec * material.specularColor;

    ad = ambient + diffuse;
    sp = specular;
}

void CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, out vec3 ad, out vec3 sp)
{
    vec3 L = normalize(light.position - fragPos);
    float diff = max(dot(normal, L), 0.0);

    vec3 reflectDir = reflect(-L, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

    vec3 ambient = light.ambient * attenuation;
    vec3 diffuse = light.diffuse * diff * material.diffuseColor * attenuation;
    vec3 specular = light.specular * spec * material.specularColor * attenuation;

    ad = ambient + diffuse;
    sp = specular;
}

void CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, out vec3 ad, out vec3 sp)
{
    vec3 L = normalize(light.position - fragPos);
    float diff = max(dot(normal, L), 0.0);

    vec3 reflectDir = reflect(-L, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance) + 1e-6);

    float theta = dot(L, normalize(-light.direction));
    float epsilon = max(light.cutOff - light.outerCutOff, 1e-5);
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 ambient = light.ambient;
    vec3 diffuse = light.diffuse * diff * material.diffuseColor;
    vec3 specular = light.specular * spec * material.specularColor;

    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    ad = ambient + diffuse;
    sp = specular;
}