#include "Hello.h"

Hello::Hello(HINSTANCE hInstance)
	:D3DFrame(hInstance)
{
	m_MainWndCaption = L"Hello";
}

Hello::~Hello()
{
}

bool Hello::initialize()
{
	if (!D3DFrame::initialize())
		return false;
	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));
	buildDescriptorHeaps();
	buildConstantBuffers();
	buildRootSignature();
	buildShadersAndInputLayout();
	buildBox();
	buildPSO();
	// Execute the initialization commands.
	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	// Wait until initialization is complete.
	flushCommandQueue();
	return true;
}

void Hello::onResize()
{
	D3DFrame::onResize();
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * XM_PI, aspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&m_Proj, P);
}

void Hello::update(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	float x = m_Radius * sinf(m_Phi) * cosf(m_Theta);
	float z = m_Radius * sinf(m_Phi) * sinf(m_Theta);
	float y = m_Radius * cosf(m_Phi);
	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&m_View, view);

	XMMATRIX world = XMLoadFloat4x4(&m_World);
	XMMATRIX proj = XMLoadFloat4x4(&m_Proj);
	XMMATRIX worldViewProj = world * view * proj;
	obejectConstant curObjConstants;

	XMStoreFloat4x4(&curObjConstants.wolrdViewProj, XMMatrixTranspose(worldViewProj));
	m_ObjectCB->copyData(0, curObjConstants);
}

void Hello::onMouseDown(WPARAM btnState, int x, int y)
{
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
	SetCapture(m_hMainWnd);
}

void Hello::onMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void Hello::onMouseMove(WPARAM btnState, int x, int y)
{
	if (btnState & MK_LBUTTON)
	{
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - m_LastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - m_LastMousePos.y));
		m_Theta += dx;
		m_Phi += dy;
		m_Phi = MathHelper::clamp(m_Phi, 0.1f, XM_PI - 0.1f);
	}
	else if (btnState & MK_RBUTTON)
	{
		float dx = 0.05f*static_cast<float>(x - m_LastMousePos.x);
		float dy = 0.05f*static_cast<float>(y - m_LastMousePos.y);
		m_Radius += dx - dy;
		m_Radius = MathHelper::clamp(m_Radius, 5.0f, 150.0f);
	}
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
}

void Hello::buildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_CbvHeap)));
}

void Hello::buildConstantBuffers()
{
	m_ObjectCB=std::make_unique<UploadBuffer<obejectConstant>>(m_d3dDevice.Get(), 1, true);
	UINT objCBByteSize = D3DUtil::calcConstantBufferByteSize(sizeof(obejectConstant));
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_ObjectCB->resource()->GetGPUVirtualAddress();
	int Cbindex = 0;
	cbAddress += Cbindex * objCBByteSize;
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = objCBByteSize;
	m_d3dDevice->CreateConstantBufferView(&cbvDesc, m_CbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Hello::buildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, 
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, 
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
	if (errorBlob)
	{
		::OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
	}
	ThrowIfFailed(hr);
	ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), 
				serializedRootSig->GetBufferSize(), IID_PPV_ARGS(m_RootSignature.GetAddressOf())));
}

void Hello::buildShadersAndInputLayout()
{
	HRESULT hr = S_OK;
	m_vsByteCode = D3DUtil::compileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	m_psByteCode = D3DUtil::compileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");
	m_InputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, 
					D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, 
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void Hello::buildBox()
{
	std::vector<vertex> vertices=
	{
		vertex({XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White)}),
		vertex({XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black)}),
		vertex({XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red)}),
		vertex({XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green)}),
		vertex({XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue)}),
		vertex({XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow)}),
		vertex({XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan)}),
		vertex({XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta)})
	};
	std::vector<std::uint16_t> indices=
	{
		0, 1, 2, 0, 2, 3,//front
		4, 6, 5, 4, 7, 6,//back
		4, 5, 1, 4, 1, 0,//left
		3, 2, 6, 3, 6, 7,//right
		1, 5, 6, 1, 6, 2,//top
		4, 0, 3, 4, 3, 7//bottom
	};
	const UINT vbByteSize = static_cast<UINT>(vertices.size()) * sizeof(vertex);
	const UINT ibByteSize = static_cast<UINT>(indices.size()) * sizeof(std::uint16_t);
	m_BoxGeo = std::make_unique<MeshGeo>();
	m_BoxGeo->name = "boxGeo";
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &m_BoxGeo->vertexBufferCPU));
	CopyMemory(m_BoxGeo->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &m_BoxGeo->indexBufferCPU));
	CopyMemory(m_BoxGeo->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
	m_BoxGeo->vertexBufferGPU = D3DUtil::createDefaultBuffer(m_d3dDevice.Get(),
		m_CommandList.Get(), vertices.data(), vbByteSize, m_BoxGeo->vertexBufferUploader);
	m_BoxGeo->indexBufferGPU = D3DUtil::createDefaultBuffer(m_d3dDevice.Get(),
		m_CommandList.Get(), indices.data(), ibByteSize, m_BoxGeo->indexBufferUploader);
	m_BoxGeo->vertexByteStride = sizeof(vertex);
	m_BoxGeo->vertexBufferByteSize = vbByteSize;
	m_BoxGeo->indexBufferByteSize = ibByteSize;

	SubMeshGeo box;
	box.indexCount = static_cast<UINT>(indices.size());
	box.startIndexLocation = 0;
	box.baseVertexLocation = 0;
	m_BoxGeo->drawArgs["box"] = box;
}

void Hello::buildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = {m_InputLayout.data(), static_cast<UINT>(m_InputLayout.size())};
	psoDesc.pRootSignature = m_RootSignature.Get();
	psoDesc.VS = 
	{
		reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()),
		m_vsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()),
		m_psByteCode->GetBufferSize()
	};
	CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
	rsDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	rsDesc.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState = rsDesc;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = 0xffffffff;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = m_BackBufferFormat;
	psoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));
}

void Hello::draw(const GameTimer &gt)
{
	ThrowIfFailed(m_DirectCmdListAlloc->Reset());
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), m_PSO.Get()));
	m_CommandList->RSSetViewports(1,&m_ScreenViewport);
	m_CommandList->RSSetScissorRects(1, &m_ScissorRect);
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	m_CommandList->ClearRenderTargetView(currentBackBufferView(), Colors::Black, 0, nullptr);
	m_CommandList->ClearDepthStencilView(depthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);
	m_CommandList->OMSetRenderTargets(1, &currentBackBufferView(), true, &depthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_CbvHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
	m_CommandList->SetGraphicsRootDescriptorTable(0, m_CbvHeap->GetGPUDescriptorHandleForHeapStart());
	m_CommandList->IASetVertexBuffers(0,1 ,&m_BoxGeo->vertexBufferView());
	m_CommandList->IASetIndexBuffer(&m_BoxGeo->indexBufferView());
	m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_CommandList->DrawIndexedInstanced(m_BoxGeo->drawArgs["box"].indexCount, 1, 0, 0, 0);
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList* cmdLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	m_SwapChain->Present(0, 0);
	m_CurrentBackBuffer = (m_CurrentBackBuffer + 1) % SwapChainBufferCount;
	flushCommandQueue();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	Hello theApp(hInstance);
	if (!theApp.initialize())
		return 0;
	return theApp.run();
}