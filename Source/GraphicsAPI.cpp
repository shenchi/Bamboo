#include "GraphicsAPI.h"

#include "GraphicsAPIDX11.h"
#include "GraphicsAPIDX12.h"

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
}
