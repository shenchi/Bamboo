#include "GraphicsAPI.h"

#include "GraphicsAPIDX11.h"

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
		case GNM:
		default:
			break;
		}
		return nullptr;
	}
}
