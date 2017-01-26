#include "GraphicsAPIDX11.h"

namespace
{
	wchar_t g_WindowClassName[] = L"BambooWindow";

	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}

		return 0;
	}
}

namespace bamboo
{

	int GraphicsAPIDX11::Init()
	{
		int result = CreateNativeWindow(L"Bamboo", 800, 600);

		if (0 != result)
		{
			return result;
		}

		result = CreateDevice();

		if (0 != result)
		{
			return result;
		}

		result = InitRenderTarget();

		if (0 != result)
		{
			return result;
		}

		{
			D3D11_VIEWPORT viewport{
				0.0f, 0.0f,
				static_cast<float>(width),
				static_cast<float>(height),
				0.0f, 1.0f };

			context->RSSetViewports(1, &viewport);
		}

		return 0;
	}

	int GraphicsAPIDX11::CreateNativeWindow(const wchar_t* title, int width, int height)
	{
		HINSTANCE hInstance = GetModuleHandle(nullptr);

		WNDCLASSEX cls{ 0 };
		cls.cbSize = sizeof(WNDCLASSEX);
		cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		cls.hInstance = hInstance;
		cls.lpfnWndProc = WndProc;
		cls.lpszClassName = g_WindowClassName;
		cls.style = CS_HREDRAW | CS_VREDRAW;

		if (!RegisterClassEx(&cls))
		{
			return -1;
		}

		RECT windowRect = { 0, 0, width, height };
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

		RECT screenRect;
		GetClientRect(GetDesktopWindow(), &screenRect);
		int windowX = screenRect.right / 2 - (windowRect.right - windowRect.left) / 2;
		int windowY = screenRect.bottom / 2 - (windowRect.bottom - windowRect.top) / 2;

		HWND hWnd = CreateWindow(
			g_WindowClassName,
			title,
			WS_OVERLAPPEDWINDOW,
			windowX, windowY,
			windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
			nullptr, nullptr, hInstance,
			nullptr/*reinterpret_cast<void*>(this)*/);

		if (!hWnd)
		{
			UnregisterClass(g_WindowClassName, hInstance);
			return -1;
		}

		this->hInstance = hInstance;
		this->hWnd = hWnd;

		ShowWindow(hWnd, SW_SHOW);

		return 0;
	}

	int GraphicsAPIDX11::CreateDevice()
	{
		HRESULT hr = S_OK;

		{
			D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

			ID3D11Device* device = nullptr;
			ID3D11DeviceContext* context = nullptr;

			if (S_OK != D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, 2, D3D11_SDK_VERSION, &device, nullptr, &context))
			{
				return -1;
			}

			if (S_OK != device->QueryInterface(__uuidof(ID3D11Device1), (void**)&(this->device)))
			{
				device->Release();
				return -1;
			}

			if (S_OK != context->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&(this->context)))
			{
				context->Release();
				device->Release();
				return -1;
			}

			device->Release();
			context->Release();
		}

		IDXGIFactory2* factory = nullptr;
		{
			IDXGIDevice* dxgiDevice = nullptr;
			if (S_OK == device->QueryInterface<IDXGIDevice>(&dxgiDevice))
			{
				IDXGIAdapter* adapter = nullptr;
				if (S_OK == dxgiDevice->GetAdapter(&adapter))
				{
					hr = adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory);
					adapter->Release();
				}
				dxgiDevice->Release();
			}
			if (S_OK != hr)
			{
				context->Release();
				device->Release();
				return -1;
			}
		}

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{ 0 };

		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		swapChainDesc.Flags = 0;// DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		if (S_OK != (hr = factory->CreateSwapChainForHwnd(device, hWnd, &swapChainDesc, nullptr, nullptr, &(swapChain))))
		{
			context->Release();
			device->Release();
			factory->Release();
			return -1;
		}

		factory->Release();
		return 0;
	}

	int GraphicsAPIDX11::InitRenderTarget()
	{
		HRESULT hr = S_OK;

		{
			ID3D11Texture2D* backbufferTex = nullptr;
			if (S_OK != (hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbufferTex)))
			{
				return -1;
			}
			if (S_OK != (hr = device->CreateRenderTargetView(backbufferTex, nullptr, &renderTarget)))
			{
				return -1;
			}
			backbufferTex->Release();


			ID3D11Texture2D* depthStencilTex = nullptr;
			D3D11_TEXTURE2D_DESC depthDesc{ 0 };
			depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depthDesc.Width = width;
			depthDesc.Height = height;
			depthDesc.ArraySize = 1;
			depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			depthDesc.MipLevels = 1;
			depthDesc.SampleDesc.Count = 1;
			depthDesc.SampleDesc.Quality = 0;
			if (S_OK != (hr = device->CreateTexture2D(&depthDesc, nullptr, &depthStencilTex)))
			{
				return -1;
			}

			if (S_OK != (hr = device->CreateDepthStencilView(depthStencilTex, nullptr, &depthStencil)))
			{
				return -1;
			}

			depthStencilTex->Release();

			context->OMSetRenderTargets(1, &renderTarget, depthStencil);
		}
		return 0;
	}

}