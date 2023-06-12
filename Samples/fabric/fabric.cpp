#include "fabric.h"

Fabric::Fabric(HINSTANCE hInstance) : D3DFrame(hInstance) {}
Fabric::~Fabric()
{
	if(m_d3dDevice!= nullptr)
		flushCommandQueue();
}

bool Fabric::initialize()
{
	if(!D3DFrame::initialize())
		return false;
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));
	m_CbvSrvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	loadTextures();
	buildRootSignature();
	buildDescriptorHeaps();
	buildShadersAndInputLayout();
	buildShape();
	buildMaterials();
	buildRenderItems();
	buildFrameResources();
	buildPSOs();

	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList * cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	flushCommandQueue();
	return true;
}

void Fabric::onResize()
{
	D3DFrame::onResize();
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*XM_PI, aspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&m_Proj, P);
}

void Fabric::update(const GameTimer& gt)
{
	updateCamera(gt);
	m_CurrFrameResourceIndex = (m_CurrFrameResourceIndex + 1) % gNumFrameResources;
	m_CurrFrameResource= m_FrameResources[m_CurrFrameResourceIndex].get();
	if (m_CurrFrameResource->fence != 0 && m_Fence->GetCompletedValue() < m_CurrFrameResource->fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(m_Fence->SetEventOnCompletion(m_CurrFrameResource->fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	updateObjectCBs(gt);
	updateMaterialCBs(gt);
	updateMainPassCB(gt);
}

void Fabric::onMouseDown(WPARAM btnState, int x, int y)
{
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
	SetCapture(m_hMainWnd);
}

void Fabric::onMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void Fabric::onMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - m_LastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - m_LastMousePos.y));
		m_Theta += dx;
		m_Phi += dy;
		m_Phi = MathHelper::clamp(m_Phi, 0.1f, XM_PI - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		float dx = 0.05f*static_cast<float>(x - m_LastMousePos.x);
		float dy = 0.05f*static_cast<float>(y - m_LastMousePos.y);
		m_Radius += dx - dy;
		m_Radius = MathHelper::clamp(m_Radius, 5.0f, 150.0f);
	}
	m_LastMousePos.x = x;
	m_LastMousePos.y  =y;
}

void Fabric::updateCamera(const GameTimer& gt)
{
	m_EyePos.x= m_Radius*sinf(m_Phi)*cosf(m_Theta);
	m_EyePos.z=m_Radius*sinf(m_Phi)*sinf(m_Theta);
	m_EyePos.y=m_Radius*cosf(m_Phi);

	XMVECTOR pos = XMVectorSet(m_EyePos.x, m_EyePos.y, m_EyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&m_View, view);
}

void Fabric::updateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = m_CurrFrameResource->objectCB.get();
	for (auto& e : m_AllRitems)
	{
		if (e->numFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->world);
			XMMATRIX texTrans = XMLoadFloat4x4(&e->texTransform);
			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.world, world);
			XMStoreFloat4x4(&objConstants.texTransform, texTrans);
			currObjectCB->copyData(e->objCBIndex, objConstants);
			e->numFramesDirty--;
		}

	}
}

void Fabric::updateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = m_CurrFrameResource->materialCB.get();
	for (auto& e : m_Materials)
	{
		Material* mat = e.second.get();
		if (mat->numFramesDirty > 0)
		{
			XMMATRIX matTrans = XMLoadFloat4x4(&mat->constants.matTransform);
			MaterialConstants matConstant;
			matConstant.diffuseAlbedo = mat->constants.diffuseAlbedo;
			matConstant.fresnelR0 = mat->constants.fresnelR0;
			matConstant.roughness = mat->constants.roughness;
			XMStoreFloat4x4(&matConstant.matTransform, matTrans);
			currMaterialCB->copyData(mat->matCBIndex, matConstant);
			mat->numFramesDirty--;
		}
	}
}

void Fabric::updateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&m_View);
	XMMATRIX proj = XMLoadFloat4x4(&m_Proj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);
	XMStoreFloat4x4(&m_MainPassCB.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_MainPassCB.invView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&m_MainPassCB.proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_MainPassCB.invProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&m_MainPassCB.viewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&m_MainPassCB.invViewProj, XMMatrixTranspose(invViewProj));
	m_MainPassCB.eyePosW = m_EyePos;
	m_MainPassCB.renderTargetSize = XMFLOAT2((float)m_ClientWidth, (float)m_ClientHeight);
	m_MainPassCB.invRenderTargetSize = XMFLOAT2(1.0f / m_ClientWidth, 1.0f / m_ClientHeight);
	m_MainPassCB.nearZ = 1.0f;
	m_MainPassCB.farZ = 1000.0f;
	m_MainPassCB.totalTime = gt.totalTime();
	m_MainPassCB.deltaTime = gt.deltaTime();
	m_MainPassCB.ambientLight = { 0.25f,0.25f,0.35f,1.0f };
	m_MainPassCB.lights[0].direction = { 0.57735f,-0.57735f,0.57735f };
	m_MainPassCB.lights[0].strength = { 0.6f,0.6f,0.6f };
	m_MainPassCB.lights[1].direction = { -0.57735f,-0.57735f,0.57735f };
	m_MainPassCB.lights[1].strength = { 0.3f,0.3f,0.3f };
	m_MainPassCB.lights[2].direction = { 0.0f,-0.707f,-0.707f };
	m_MainPassCB.lights[2].strength = { 0.15f,0.15f,0.15f };

	auto currPassCB = m_CurrFrameResource->passCB.get();
	currPassCB->copyData(0, m_MainPassCB);
}

void Fabric::loadTextures()
{
	auto woodTex=std::make_unique<Texture>();
	woodTex->name = "woodTex";
	woodTex->fileName = L"../../Textures/WoodCrate02.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_d3dDevice.Get(),
		m_CommandList.Get(), woodTex->fileName.c_str(), woodTex->resource, woodTex->uploadHeap));
	m_Textures[woodTex->name] = std::move(woodTex);
}

void Fabric::buildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);
	auto staticSamplers = getStaticSamplers();
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
	if (errorBlob != nullptr)
		::OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
	ThrowIfFailed(hr);
	ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
}

void Fabric::buildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_SrvDescriptorHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDesc(m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	auto woodTex = m_Textures["woodTex"]->resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = woodTex->GetDesc().Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = woodTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	m_d3dDevice->CreateShaderResourceView(woodTex.Get(), &srvDesc, hDesc);
}

void Fabric::buildShadersAndInputLayout()
{
	m_vsByteCode = D3DUtil::compileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	m_psByteCode = D3DUtil::compileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");
	m_InputLayout =
	{
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32B32_FLOAT,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};
}

void Fabric::buildShape()
{
	std::vector<Vertex> vertices;
	float halfWidth = 0.5f, halfHeight = 0.5f, halfDepth = 0.5f;
	vertices =
	{
		//front
		{XMFLOAT3(-halfWidth,-halfHeight,-halfDepth),XMFLOAT3(0.0f,0.0f,1.0f),XMFLOAT2(0.0f,1.0f)},
		{XMFLOAT3(-halfWidth,+halfHeight,-halfDepth),XMFLOAT3(0.0f,0.0f,1.0f),XMFLOAT2(0.0f,0.0f)},
		{XMFLOAT3(+halfWidth,+halfHeight,-halfDepth),XMFLOAT3(0.0f,0.0f,1.0f),XMFLOAT2(1.0f,0.0f)},
		{XMFLOAT3(+halfWidth,-halfHeight,-halfDepth),XMFLOAT3(0.0f,0.0f,1.0f),XMFLOAT2(1.0f,1.0f)},
		//back
		{XMFLOAT3(-halfWidth,-halfHeight,+halfDepth),XMFLOAT3(0.0f,0.0f,-1.0f),XMFLOAT2(1.0f,1.0f)},
		{XMFLOAT3(+halfWidth,-halfHeight,+halfDepth),XMFLOAT3(0.0f,0.0f,-1.0f),XMFLOAT2(0.0f,1.0f)},
		{XMFLOAT3(+halfWidth,+halfHeight,+halfDepth),XMFLOAT3(0.0f,0.0f,-1.0f),XMFLOAT2(0.0f,0.0f)},
		{XMFLOAT3(-halfWidth,+halfHeight,+halfDepth),XMFLOAT3(0.0f,0.0f,-1.0f),XMFLOAT2(1.0f,0.0f)},
		//left
		{XMFLOAT3(-halfWidth,-halfHeight,+halfDepth),XMFLOAT3(-1.0f,0.0f,0.0f),XMFLOAT2(0.0f,1.0f)},
		{XMFLOAT3(-halfWidth,+halfHeight,+halfDepth),XMFLOAT3(-1.0f,0.0f,0.0f),XMFLOAT2(0.0f,0.0f)},
		{XMFLOAT3(-halfWidth,+halfHeight,-halfDepth),XMFLOAT3(-1.0f,0.0f,0.0f),XMFLOAT2(1.0f,0.0f)},
		{XMFLOAT3(-halfWidth,-halfHeight,-halfDepth),XMFLOAT3(-1.0f,0.0f,0.0f),XMFLOAT2(1.0f,1.0f)},
		//right
		{XMFLOAT3(+halfWidth,-halfHeight,-halfDepth),XMFLOAT3(1.0f,0.0f,0.0f),XMFLOAT2(0.0f,1.0f)},
		{XMFLOAT3(+halfWidth,+halfHeight,-halfDepth),XMFLOAT3(1.0f,0.0f,0.0f),XMFLOAT2(0.0f,0.0f)},
		{XMFLOAT3(+halfWidth,+halfHeight,+halfDepth),XMFLOAT3(1.0f,0.0f,0.0f),XMFLOAT2(1.0f,0.0f)},
		{XMFLOAT3(+halfWidth,-halfHeight,+halfDepth),XMFLOAT3(1.0f,0.0f,0.0f),XMFLOAT2(1.0f,1.0f)},
		//top
		{XMFLOAT3(-halfWidth,+halfHeight,-halfDepth),XMFLOAT3(0.0f,1.0f,0.0f),XMFLOAT2(0.0f,1.0f)},
		{XMFLOAT3(-halfWidth,+halfHeight,+halfDepth),XMFLOAT3(0.0f,1.0f,0.0f),XMFLOAT2(0.0f,0.0f)},
		{XMFLOAT3(+halfWidth,+halfHeight,+halfDepth),XMFLOAT3(0.0f,1.0f,0.0f),XMFLOAT2(1.0f,0.0f)},
		{XMFLOAT3(+halfWidth,+halfHeight,-halfDepth),XMFLOAT3(0.0f,1.0f,0.0f),XMFLOAT2(1.0f,1.0f)},
		//buttom
		{XMFLOAT3(-halfWidth,-halfHeight,-halfDepth),XMFLOAT3(0.0f,-1.0f,0.0f),XMFLOAT2(1.0f,1.0f)},
		{XMFLOAT3(+halfWidth,-halfHeight,-halfDepth),XMFLOAT3(0.0f,-1.0f,0.0f),XMFLOAT2(0.0f,1.0f)},
		{XMFLOAT3(+halfWidth,-halfHeight,+halfDepth),XMFLOAT3(0.0f,-1.0f,0.0f),XMFLOAT2(0.0f,0.0f)},
		{XMFLOAT3(-halfWidth,-halfHeight,+halfDepth),XMFLOAT3(0.0f,-1.0f,0.0f),XMFLOAT2(1.0f,0.0f)},
	};
	std::vector<std::uint16_t> indices =
	{
		//front
		0,1,2,
		0,2,3,
		//back
		4,5,6,
		4,6,7,
		//left
		8,9,10,
		8,10,11,
		//right
		12,13,14,
		12,14,15,
		//top
		16,17,18,
		16,18,19,
		//buttom
		20,21,22,
		20,22,23,
	};
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);
	auto geo=std::make_unique<MeshGeo>();
	geo->name="boxGeo";
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->vertexBufferCPU));
	CopyMemory(geo->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize,&geo->indexBufferCPU));
	CopyMemory(geo->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
	geo->vertexBufferGPU = D3DUtil::createDefaultBuffer(m_d3dDevice.Get(), m_CommandList.Get(),
		vertices.data(), vbByteSize, geo->vertexBufferUploader);
	geo->indexBufferGPU = D3DUtil::createDefaultBuffer(m_d3dDevice.Get(), m_CommandList.Get(),
		indices.data(), ibByteSize, geo->indexBufferUploader);
	geo->indexBufferByteSize = ibByteSize;
	geo->vertexBufferByteSize = vbByteSize;
	geo->vertexByteStride = sizeof(Vertex);
	SubMeshGeo box;
	box.baseVertexLocation = 0;
	box.indexCount = (UINT)indices.size();
	box.startIndexLocation = 0;
	box.vertexCount = (UINT)vertices.size();
	geo->drawArgs["box"] = box;
	m_Geo[geo->name] = std::move(geo);
}

void Fabric::buildMaterials()
{
	auto wood = std::make_unique<Material>();
	wood->name = "wood";
	wood->matCBIndex = 0;
	wood->diffuseSrvHeapIndex = 0;
	wood->constants.diffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wood->constants.fresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	wood->constants.roughness = 0.2f;
	m_Materials["wood"] = std::move(wood);
}

void Fabric::buildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->objCBIndex = 0;
	boxRitem->geo=m_Geo["boxGeo"].get();
	boxRitem->mat = m_Materials["wood"].get();
	boxRitem->indexCount = boxRitem->geo->drawArgs["box"].indexCount;
	boxRitem->startIndexLocation = boxRitem->geo->drawArgs["box"].startIndexLocation;
	boxRitem->baseVertexLocation = boxRitem->geo->drawArgs["box"].baseVertexLocation;
	m_RitemLayer.push_back(boxRitem.get());
	m_AllRitems.push_back(std::move(boxRitem));
}

void Fabric::buildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		m_FrameResources.push_back(std::make_unique<FrameResource>(m_d3dDevice.Get(),
			1, static_cast<UINT>(m_AllRitems.size()), static_cast<UINT>(m_Materials.size())));
	}
}

void Fabric::buildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { m_InputLayout.data(),(UINT)m_InputLayout.size() };
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
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = m_BackBufferFormat;
	psoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = m_DepthStencilFormat;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_PSO.GetAddressOf())));
}

void Fabric::draw(const GameTimer& gt)
{
	auto cmdListAlloc = m_CurrFrameResource->cmdListAlloc;
	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(m_CommandList->Reset(cmdListAlloc.Get(),m_PSO.Get()));
	m_CommandList->RSSetViewports(1, &m_ScreenViewport);
	m_CommandList->RSSetScissorRects(1, &m_ScissorRect);
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		currentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET));
	m_CommandList->ClearRenderTargetView(currentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	m_CommandList->ClearDepthStencilView(depthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	m_CommandList->OMSetRenderTargets(1, &currentBackBufferView(), true, &depthStencilView());
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescriptorHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
	auto passCB = m_CurrFrameResource->passCB->resource();
	m_CommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());
	drawRenderItems(m_CommandList.Get(), m_RitemLayer);
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		currentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT));
	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList* cmdLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	ThrowIfFailed(m_SwapChain->Present(0, 0));
	m_CurrentBackBuffer = (m_CurrentBackBuffer + 1) % SwapChainBufferCount;
	m_CurrFrameResource->fence = ++m_CurrentFence;
	m_CommandQueue->Signal(m_Fence.Get(), m_CurrentFence);
}

void Fabric::drawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = D3DUtil::calcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = D3DUtil::calcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = m_CurrFrameResource->objectCB->resource();
	auto matCB = m_CurrFrameResource->materialCB->resource();
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];
		cmdList->IASetVertexBuffers(0, 1, &ri->geo->vertexBufferView());
		cmdList->IASetIndexBuffer(&ri->geo->indexBufferView());
		cmdList->IASetPrimitiveTopology(ri->primitiveType);
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->mat->diffuseSrvHeapIndex, m_CbvSrvUavDescriptorSize);
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->objCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->mat->matCBIndex * matCBByteSize;
		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);
		cmdList->DrawIndexedInstanced(ri->indexCount, 1, ri->startIndexLocation, ri->baseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Fabric::getStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP // addressW
	);
	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP // addressW
	);
	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW
	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try
	{
		Fabric theApp(hInstance);
		if(!theApp.initialize())
			return 0;
		return theApp.run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}