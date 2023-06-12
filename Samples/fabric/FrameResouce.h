#pragma once
#include "../../Common/D3DFrameHelper.h"
#include "../../Common/UploadBuffer.h"

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 world = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 texTransform = MathHelper::Identity4x4();
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 view = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 invView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 invProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 viewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 invViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 eyePosW = { 0.0f,0.0f,0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 renderTargetSize = { 0.0f,0.0f };
	DirectX::XMFLOAT2 invRenderTargetSize = { 0.0f,0.0f };
	float nearZ = 0.0f;
	float farZ = 0.0f;
	float totalTime = 0.0f;
	float deltaTime = 0.0f;
	DirectX::XMFLOAT4 ambientLight = { 0.0f,0.0f,0.0f,0.0f };
	Light lights[MaxLights];
};

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

struct FrameResource
{
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource operator=(const FrameResource& rhs) = delete;
	~FrameResource();
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc;
	std::unique_ptr<UploadBuffer<PassConstants>> passCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> objectCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> materialCB = nullptr;
	UINT64 fence = 0;
};