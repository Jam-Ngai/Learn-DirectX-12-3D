#pragma once

#include "D3DFrameHelper.h"

template<typename T>
class UploadBuffer
{
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) :
		m_IsConstantuUffer(isConstantBuffer)
	{
		m_ElementByteSize = sizeof(T);
		if (isConstantBuffer)
		{
			m_ElementByteSize=D3DUtil::calcConstantBufferByteSize(sizeof(T));
		}
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(m_ElementByteSize*elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_UploadBuffer.GetAddressOf())
		));
		ThrowIfFailed(m_UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData)));
	}

	~UploadBuffer()
	{
		if (m_UploadBuffer != nullptr)
			m_UploadBuffer->Unmap(0, nullptr);
		m_MappedData = nullptr;
	}
	ID3D12Resource* resource()const
	{
		return m_UploadBuffer.Get();
	}
	void copyData(int elementIndex, const T& data)
	{
		memcpy(&m_MappedData[elementIndex*m_ElementByteSize], &data, sizeof(T));
	}
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_UploadBuffer;
	BYTE *m_MappedData = nullptr;
	UINT m_ElementByteSize = 0;
	bool m_IsConstantuUffer = false;
};