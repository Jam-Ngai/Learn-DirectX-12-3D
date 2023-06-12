#pragma once
#include "../../Common/D3DFrame.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/DDSTextureLoader.h"
#include "FrameResouce.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

struct RenderItem
{
	RenderItem() = default;
	XMFLOAT4X4 world = MathHelper::Identity4x4();
	XMFLOAT4X4 texTransform = MathHelper::Identity4x4();
	int numFramesDirty = gNumFrameResources;
	UINT objCBIndex = -1;
	Material* mat = nullptr;
	MeshGeo* geo = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT indexCount = 0;
	UINT startIndexLocation = 0;
	int baseVertexLocation = 0;
};

class Fabric : public D3DFrame
{
public:
	Fabric(HINSTANCE hInstance);
	~Fabric();
	Fabric(const Fabric& rhs) = delete;
	Fabric& operator=(const Fabric& rhs) = delete;
	virtual bool initialize() override;

private:
	virtual void onResize() override;
	virtual void update(const GameTimer& gt) override;
	virtual void draw(const GameTimer& gt) override;

	virtual void onMouseDown(WPARAM btnState, int x, int y) override;
	virtual void onMouseUp(WPARAM btnState, int x, int y) override;
	virtual void onMouseMove(WPARAM btnState, int x, int y) override;

	void updateCamera(const GameTimer& gt);
	void updateObjectCBs(const GameTimer& gt);
	void updateMaterialCBs(const GameTimer& gt);
	void updateMainPassCB(const GameTimer& gt);

	void loadTextures();
	void buildRootSignature();
	void buildDescriptorHeaps();
	void buildShadersAndInputLayout();
	void buildShape();
	void buildPSOs();
	void buildFrameResources();
	void buildMaterials();
	void buildRenderItems();
	void drawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> getStaticSamplers();

private:
	std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
	FrameResource* m_CurrFrameResource = nullptr;
	int m_CurrFrameResourceIndex = 0;
	UINT m_CbvSrvDescriptorSize = 0;
	ComPtr<ID3D12RootSignature> m_RootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_SrvDescriptorHeap = nullptr;
	std::unordered_map<std::string, std::unique_ptr<MeshGeo>> m_Geo;
	std::unordered_map<std::string, std::unique_ptr<Material>> m_Materials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;
	ComPtr<ID3DBlob> m_vsByteCode = nullptr;
	ComPtr<ID3DBlob> m_psByteCode = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;
	ComPtr<ID3D12PipelineState> m_PSO = nullptr;
	std::vector<std::unique_ptr<RenderItem>> m_AllRitems;
	std::vector<RenderItem*> m_RitemLayer;
	PassConstants m_MainPassCB;
	XMFLOAT3 m_EyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 m_View = MathHelper::Identity4x4();
	XMFLOAT4X4 m_Proj = MathHelper::Identity4x4();
	float m_Theta = 1.5f * XM_PI;
	float m_Phi = XM_PIDIV4;
	float m_Radius = 2.5f;
	POINT m_LastMousePos;
};