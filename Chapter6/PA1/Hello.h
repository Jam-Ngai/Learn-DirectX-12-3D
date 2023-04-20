#pragma once

#include "../../Common/D3DFrame.h"
#include "../../Common/UploadBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct vertex
{
	XMFLOAT3 pos;
	XMFLOAT4 color;
};

struct obejectConstant
{
	XMFLOAT4X4 wolrdViewProj = MathHelper::Identity4x4();
};

class Hello : public D3DFrame
{
public:
	Hello(HINSTANCE hInstance);
	~Hello();

	virtual bool initialize()override;

private:
	virtual void onResize()override;
	virtual void update(const GameTimer& gt)override;
	virtual void draw(const GameTimer& gt)override;
	virtual void onMouseDown(WPARAM btnState, int x, int y)override;
	virtual void onMouseUp(WPARAM btnState, int x, int y)override;
	virtual void onMouseMove(WPARAM btnState, int x, int y)override;

	void buildDescriptorHeaps();
	void buildConstantBuffers();
	void buildRootSignature();
	void buildShadersAndInputLayout();
	void buildBox();
	void buildPSO();

	ComPtr<ID3D12RootSignature> m_RootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_CbvHeap = nullptr;

	std::unique_ptr<MeshGeo> m_BoxGeo = nullptr;
	std::unique_ptr <UploadBuffer<obejectConstant>> m_ObjectCB = nullptr;
	
	ComPtr<ID3DBlob> m_vsByteCode = nullptr;
	ComPtr<ID3DBlob> m_psByteCode  =nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;
	ComPtr<ID3D12PipelineState> m_PSO = nullptr;
	XMFLOAT4X4 m_World = MathHelper::Identity4x4();
	XMFLOAT4X4 m_View = MathHelper::Identity4x4();
	XMFLOAT4X4 m_Proj = MathHelper::Identity4x4();

	float m_Theta = 1.5 * XM_PI;
	float m_Phi = XM_PIDIV4;
	float m_Radius = 5.0f;

	POINT m_LastMousePos;
};