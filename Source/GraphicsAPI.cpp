#include "GraphicsAPI.h"

#include "GraphicsAPIDX11.h"
#include "GraphicsAPIDX12.h"

#include <cstring>

namespace bamboo
{
	GraphicsAPI* InitGraphicsAPI(GraphicsAPIType type, void* windowHandle)
	{
		switch (type)
		{
		case Direct3D11:
			return bamboo::dx11::InitGraphicsAPIDX11(windowHandle);
			break;
		case Direct3D12:
			return bamboo::dx12::InitGraphicsAPIDX12(windowHandle);
			break;
		case GNM:
		default:
			break;
		}
		return nullptr;
	}

	void DrawCall::FillBindingData(uint32_t offset, const void * data, size_t size)
	{
		memcpy(ResourceBindingData + offset, data, size);
	}

	void DrawCall::ClearBindingData()
	{
		memset(ResourceBindingData, 0, sizeof(ResourceBindingData));
	}
}
