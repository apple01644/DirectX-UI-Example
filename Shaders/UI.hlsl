#include "Struct.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
	float4 gColor;
};

struct VertexIn
{
	float2 PosL    : POSITION;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
	float2 _2D_POS = vin.PosL;
	
	vout.Color = gColor;
	
	_2D_POS = mul(float4(vin.PosL, 0.f, 1.f), gWorld).xy;
	//vout.Color.x += gWorld[0].w / 32;
	_2D_POS.x += gWorld[0].w;
	_2D_POS.y += gWorld[1].w;
	
	vout.PosH = float4(_2D_POS / gRenderTargetSize * float2(2,-2) + float2(-1, 1), 0.f, 1.0f);
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}


