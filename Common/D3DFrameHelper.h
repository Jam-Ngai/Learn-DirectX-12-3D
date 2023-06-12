#pragma once 

#include <windows.h>

#include <initguid.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>

#include <string>
#include <wrl.h>
#include <vector>
#include <array>
#include <algorithm>
#include <memory>
#include <unordered_map>

inline std::wstring ANSIToWide(const std::string& str)
{
	WCHAR buffer[512];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
	return std::wstring(buffer);
}

class DxException
{
public:
DxException() = default;
	DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);
	std::wstring ToString()const;
	HRESULT ErrorCode = S_OK;
	std::wstring FunctionName;
	std::wstring Filename;
	int LineNumber = -1;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                                               \
{                                                                                      \
	HRESULT hr__ = (x);                                                                \
	if(FAILED(hr__)) { throw DxException(hr__, L#x, ANSIToWide(__FILE__), __LINE__); } \
}                                                                                      
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif

class MathHelper
{
public:
	static DirectX::XMFLOAT4X4 Identity4x4()
	{
		static DirectX::XMFLOAT4X4 I(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
		return I;
	}

	template<typename T>
	static T clamp(const T& x, const T& low, const T& high)
	{
		return x < low ? low : (x > high ? high : x);
	}
};

class D3DUtil
{
public:
	static bool isKeyDown(int vkeyCode);
	static std::string toString(HRESULT hr);
	static UINT calcConstantBufferByteSize(UINT byteSize)
	{
		return (byteSize + 255) & ~255;
	}
	static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);
	static Microsoft::WRL::ComPtr<ID3D12Resource> createDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
	static Microsoft::WRL::ComPtr<ID3DBlob> compileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);
};

struct SubMeshGeo
{
	UINT indexCount = 0;
	UINT vertexCount = 0;
	UINT startIndexLocation = 0;
	INT baseVertexLocation = 0;
	DirectX::BoundingBox bounds;
};
struct MeshGeo
{
	std::string name;
	Microsoft::WRL::ComPtr<ID3DBlob> vertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> indexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferUploader = nullptr;

	UINT vertexByteStride = 0;
	UINT vertexBufferByteSize = 0;
	DXGI_FORMAT indexFormat = DXGI_FORMAT_R16_UINT;
	UINT indexBufferByteSize = 0;

	std::unordered_map<std::string, SubMeshGeo> drawArgs;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView()const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = vertexByteStride;
		vbv.SizeInBytes = vertexBufferByteSize;
		return vbv;
	}
	D3D12_INDEX_BUFFER_VIEW indexBufferView()const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = indexFormat;
		ibv.SizeInBytes = indexBufferByteSize;
		return ibv;
	}
	void disposeUploaders()
	{
		vertexBufferUploader = nullptr;
		indexBufferUploader = nullptr;
	}
};

struct Light
{
	DirectX::XMFLOAT3 strength = { 0.5f, 0.5f, 0.5f };
	float falloffStart = 1.0f;
	DirectX::XMFLOAT3 direction = { 0.0f, -1.0f, 0.0f };
	float falloffEnd = 10.0f;
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	float spotPower = 64.0f;
};

#define MaxLights 16

struct MaterialConstants
{
	DirectX::XMFLOAT4 diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 fresnelR0 = { 0.01f, 0.01f, 0.01f };
	float roughness = 0.25f;
	DirectX::XMFLOAT4X4 matTransform = MathHelper::Identity4x4();
};

extern const int gNumFrameResources;

struct Material
{
	std::string name;
	int matCBIndex = -1;
	int diffuseSrvHeapIndex = -1;
	int normalSrvHeapIndex = -1;
	int numFramesDirty = gNumFrameResources;
	MaterialConstants constants;
};

struct Texture
{
	std::string name;
	std::wstring fileName;
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap = nullptr;
};