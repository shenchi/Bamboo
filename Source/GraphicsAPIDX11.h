#pragma once

#include "GraphicsAPI.h"

#include <Windows.h>
#include <d3d11_1.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace bamboo::dx11
{
	/*class VertexBuffer
	{
	public:
		int Create();
	};*/

	class GraphicsAPIDX11 : public GraphicsAPI
	{
	public:
		virtual int Init();

	private:
		int CreateNativeWindow(const wchar_t* title, int width, int height);

		int CreateDevice();

		int InitRenderTarget();

		ID3D11Buffer* CreateVertexBuffer(UINT size, const void* data);

		ID3D11Buffer* CreateIndexBuffer(UINT size, const void* data);

	private:

		// window handles
		HWND					hWnd;
		HINSTANCE				hInstance;

		// window / buffer size
		int						width;
		int						height;

	private:
		ID3D11Device1*			device;
		ID3D11DeviceContext1*	context;

		IDXGISwapChain1*		swapChain;

		ID3D11RenderTargetView*	renderTarget;
		ID3D11DepthStencilView*	depthStencil;

	};
}
