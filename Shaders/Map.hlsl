
struct Material
{
	float4   DiffuseAlbedo;
    float3   FresnelR0;
    float    Roughness;
	float4x4 MatTransform;
};

Texture2D    gTexture[8] : register(t0);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
	float4x4 gTexTransform;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 cbPerObjectPad2;

	Material gMaterial[MAX_MATERIAL];
};

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
	float4 GeoOpacity0 : GEO_FIRST;
	float4 GeoOpacity1 : GEO_SECOND;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	float2 TexC1    : TEXCOORD1;
	float2 TexC2    : TEXCOORD2;
	float4 GeoOpacity0 : GEO_FIRST;
	float4 GeoOpacity1 : GEO_SECOND;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    vout.PosH = mul(posW, gViewProj);
	
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC1 = mul(texC, gMaterial[0].MatTransform).xy;
	texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC2 = mul(texC, gMaterial[1].MatTransform).xy;

	vout.GeoOpacity0 = vin.GeoOpacity0;
	vout.GeoOpacity1 = vin.GeoOpacity1;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 diffuseAlbedo = 
	gTexture[0].Sample(gsamAnisotropicWrap, pin.TexC2.xy) * pin.GeoOpacity0.x+
	gTexture[1].Sample(gsamAnisotropicWrap, pin.TexC1.xy) * pin.GeoOpacity0.y+
	gTexture[2].Sample(gsamAnisotropicWrap, pin.TexC1.xy) * pin.GeoOpacity0.z+
	gTexture[3].Sample(gsamAnisotropicWrap, pin.TexC1.xy) * pin.GeoOpacity0.w+
	gTexture[4].Sample(gsamAnisotropicWrap, pin.TexC1.xy) * pin.GeoOpacity1.x+
	gTexture[5].Sample(gsamAnisotropicWrap, pin.TexC1.xy) * pin.GeoOpacity1.y+
	gTexture[6].Sample(gsamAnisotropicWrap, pin.TexC1.xy) * pin.GeoOpacity1.z+
	gTexture[7].Sample(gsamAnisotropicWrap, pin.TexC1.xy) * pin.GeoOpacity1.w
	;
	
    return diffuseAlbedo;
}


