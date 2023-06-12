#define MaxLights 16
struct Light
{
	float3 Strength;
	float FalloffStart;
	float3 Direction;
	float FalloffEnd;
	float3 Position;
	float SpotPower;
};

struct Material
{
    float4 DiffuseAlbedo;
	float3 FresnelR0;
    float Shininess;
};

float CalcAttenuation(float d,float fallofStart,float fallofEnd)
{
	return saturate((fallofEnd - d) / (fallofEnd - fallofStart));
}

float3 SchlickFresnel(float3 R0,float3 normal,float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));
    float f0 = 1.0f - cosIncidentAngle;
    float3 refectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);
    return refectPercent;
}

float3 BlinnPhone(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);
    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);
    float3 specAlbedo=fresnelFactor*roughnessFactor;
    specAlbedo = specAlbedo / (1.0f + specAlbedo);
    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;
    float ndotl = max(dot(normal, lightVec), 0.0f);
    float3 lightStrength = ndotl * L.Strength;
    return BlinnPhone(lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputePointLight(Light L,Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    if(d>L.FalloffEnd)
        return 0.0f;
    lightVec /= d;
    float ndotl = max(dot(normal, lightVec),0.0f);
    float3 lightStrength = L.Strength * ndotl;
    float att = saturate((L.FalloffEnd - d) / (L.FalloffEnd - L.FalloffStart));
    lightStrength *= att;
    return BlinnPhone(lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputeSpotLight(Light L,Material mat, float3 pos, float3 normal,float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    if(d>L.FalloffEnd)
        return float3(0,0,0);
    lightVec /= d;
    float ndotl = max(dot(normal, lightVec), 0.0f);
    float3 lightStrength = L.Strength * ndotl;
    float att = saturate((L.FalloffEnd - d) / (L.FalloffEnd - L.FalloffStart));
    lightStrength *= att;
    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
    lightStrength *= spotFactor;
    return BlinnPhone(lightStrength, lightVec, normal, toEye, mat);
}

float4 ComputeLighting(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, 
                        float3 toEye, float3 shadowFactor)
{
    float3 result = 0.0f;
    int i = 0;
#if (NUM_DIR_LIGHTS>0)
    for(i=0;i<NUM_DIR_LIGHTS;++i)
    {
        result +=ComputeDirectionalLight(gLights[i],mat,normal,toEye);
    }
#endif

#if (NUM_POINT_LIGHTS>0)
    for(i=NUM_DIR_LIGHTS;i<NUM_DIR_LIGHTS+NUM_POINTS_LIGHTS;++i)
    {
        result +=ComputeDirectionalLight(gLights[i],mat,normal,toEye);
    }
#endif
    
#if (NUM_SPOT_LIGHTS>0)
    for(i=NUM_DIR_LIGHTS+NUM_POINT_LIGHTS;i<NUM_DIR_LIGHTS+NUM_POINTS_LIGHTS+NUM_SPOT_LIGHTS;++i)
    {
        result +=ComputeDirectionalLight(gLights[i],mat,normal,toEye);
    }
#endif
    return float4(result, 0.0f);
}