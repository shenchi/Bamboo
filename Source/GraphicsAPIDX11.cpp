#include "GraphicsAPIDX11.h"

#include <Windows.h>
#include <d3d11_1.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

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

namespace bamboo::dx11
{
	struct VertexLayoutDX11
	{
		D3D11_INPUT_ELEMENT_DESC	elements[MaxVertexInputSlot];
		uint16_t					elementCount;
		uint16_t					stride;
	};

	struct GeometryBufferDX11
	{
		ID3D11Buffer*	buffer;
		size_t			size;
		UINT			bindFlags;
		bool			dynamic;

		inline void Reset(size_t size, UINT bindFlags, bool dynamic)
		{
			Release();
			this->size = size;
			this->bindFlags = bindFlags;
			this->dynamic = dynamic;
		}

		inline void Release()
		{
			if (nullptr != buffer)
			{
				buffer->Release();
				buffer = nullptr;
				size = 0;
				dynamic = false;
			}
		}

		inline void Update(ID3D11Device1* device, ID3D11DeviceContext1* context, size_t size, const void* data)
		{
			if (nullptr == buffer)
			{
				D3D11_BUFFER_DESC desc = {};
				desc.BindFlags = bindFlags;
				desc.ByteWidth = size;
				desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;

				D3D11_SUBRESOURCE_DATA data_desc = {};
				data_desc.pSysMem = data;

				if (FAILED(device->CreateBuffer(&desc, &data_desc, &(buffer))))
				{
					// error
					return;
				}
			}
			else if (dynamic)
			{
				D3D11_MAPPED_SUBRESOURCE res = {};
				context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
				memcpy(res.pData, data, min(size, this->size));
				context->Unmap(buffer, 0);
			}
			// else error
		}
	};

	struct RenderTargetDX11
	{
		union
		{
			ID3D11RenderTargetView*		renderTargetView;
			ID3D11DepthStencilView*		depthStencilView;
		};

		PixelFormat					format;
		uint32_t					width;
		uint32_t					height;
		bool						isDepth;
		bool						hasStencil;

		void Reset(PixelFormat format, uint32_t width, uint32_t height, bool isDepth, bool hasStencil)
		{
			Release();
			this->format = format;
			this->width = width;
			this->height = height;
			this->isDepth = isDepth;
			this->hasStencil = hasStencil;
		}

		void Release()
		{
			if (isDepth)
			{
				if (nullptr != depthStencilView)
				{
					depthStencilView->Release();
					depthStencilView = nullptr;
				}
			}
			else
			{
				if (nullptr != renderTargetView)
				{
					renderTargetView->Release();
					renderTargetView = nullptr;
				}
			}
		}
	};

	struct TextureDX11
	{
		ID3D11Texture2D*			texture;

		PixelFormat					format;
		uint32_t					width;
		uint32_t					height;

		void Reset(PixelFormat format, uint32_t width, uint32_t height)
		{
			Release();
			this->format = format;
			this->width = width;
			this->height = height;
		}

		void Release()
		{
			if (nullptr != texture)
			{
				texture->Release();
				texture = nullptr;
			}
		}
	};

	DXGI_FORMAT InputSlotTypeTable[][4] = 
	{
		// COMPONENT_FLOAT
		{ DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT}
	};

	size_t InputSlotSizeTable[] = 
	{
		4 // COMPONENT_FLOAT
	};

	LPSTR InputSemanticsTable[] =
	{
		"POSITION",
		"COLOR",
		"NORMAL",
		"TANGENT",
		"BINORMAL",
		"TEXCOORD0",
		"TEXCOORD1",
		"TEXCOORD2",
		"TEXCOORD3",
	};

	struct GraphicsAPIDX11 : public GraphicsAPI
	{
		int width;
		int height;

		HINSTANCE					hInstance;
		HWND						hWnd;

		IDXGISwapChain1*			swapChain;
		ID3D11Device1*				device;
		ID3D11DeviceContext1*		context;

		VertexLayoutDX11			vertexLayouts[MaxVertexLayoutCount];

		GeometryBufferDX11			vertexBuffers[MaxVertexBufferCount];
		GeometryBufferDX11			indexBuffers[MaxIndexBufferCount];

		RenderTargetDX11			renderTargets[MaxRenderTargetCount];
		RenderTargetDX11			defaultColorBuffer;
		RenderTargetDX11			defaultDepthStencilBuffer;

		TextureDX11					textures[MaxTextureCount];

		int Init()
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

		int CreateNativeWindow(const wchar_t* title, int width, int height)
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

			DWORD winStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

			RECT windowRect = { 0, 0, width, height };
			AdjustWindowRect(&windowRect, winStyle, FALSE);

			RECT screenRect;
			GetClientRect(GetDesktopWindow(), &screenRect);
			int windowX = screenRect.right / 2 - (windowRect.right - windowRect.left) / 2;
			int windowY = screenRect.bottom / 2 - (windowRect.bottom - windowRect.top) / 2;

			HWND hWnd = CreateWindow(
				g_WindowClassName,
				title,
				winStyle,
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

		int CreateDevice()
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

			DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

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

		int InitRenderTarget()
		{
			HRESULT hr = S_OK;

			{
				// TODO
				defaultColorBuffer.Reset(PixelFormat::AUTO_PIXEL_FORMAT, width, height, false, false);
				ID3D11Texture2D* backbufferTex = nullptr;
				if (S_OK != (hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbufferTex)))
				{
					return -1;
				}
				if (S_OK != (hr = device->CreateRenderTargetView(backbufferTex, nullptr, &(defaultColorBuffer.renderTargetView))))
				{
					return -1;
				}
				backbufferTex->Release();

				defaultDepthStencilBuffer.Reset(PixelFormat::D24_UNORM_S8_UINT, width, height, true, true);
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

				if (S_OK != (hr = device->CreateDepthStencilView(depthStencilTex, nullptr, &(defaultDepthStencilBuffer.depthStencilView))))
				{
					return -1;
				}

				depthStencilTex->Release();

				context->OMSetRenderTargets(1, &(defaultColorBuffer.renderTargetView), defaultDepthStencilBuffer.depthStencilView);
			}

			return 0;
		}

		// interface implementation
#pragma region interface implementation

		VertexLayoutHandle CreateVertexLayout(VertexLayout layout) override
		{
			uint16_t handle = vlHandleAlloc.Alloc();

			if (invalid_handle != handle) return VertexLayoutHandle{ invalid_handle };

			VertexLayoutDX11& vl = vertexLayouts[handle];

			vl.elementCount = layout.SlotCount;

			UINT offset = 0;

			for (size_t i = 0; i < layout.SlotCount; ++i)
			{
				VertexInputSlot& slot = layout.Slots[i];
				D3D11_INPUT_ELEMENT_DESC& desc = vl.elements[i];

				size_t size = InputSlotSizeTable[slot.ComponentType] * (slot.ComponentCount + 1);

				desc.AlignedByteOffset = offset;
				desc.Format = InputSlotTypeTable[slot.ComponentType][slot.ComponentCount];
				desc.InputSlot = 0; // TODO
				desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
				desc.InstanceDataStepRate = 0;
				desc.SemanticIndex = 0;
				desc.SemanticName = InputSemanticsTable[slot.SemanticId];
				
				offset += size;
			}

			vl.stride = offset;

			return VertexLayoutHandle{ handle };
		}

		void DestroyVertexLayout(VertexLayoutHandle handle) override
		{
			if (vlHandleAlloc.InUse(handle.id))
				vlHandleAlloc.Free(handle.id);
		}

		VertexBufferHandle GraphicsAPIDX11::CreateVertexBuffer(size_t size, bool dynamic) override
		{
			uint16_t handle = vbHandleAlloc.Alloc();

			if (handle != invalid_handle)
			{
				GeometryBufferDX11& vb = vertexBuffers[handle];
				vb.Reset(size, D3D11_BIND_VERTEX_BUFFER, dynamic);
			}

			return VertexBufferHandle{ handle };
		}

		void DestroyVertexBuffer(VertexBufferHandle handle) override
		{
			if (!vbHandleAlloc.InUse(handle.id)) return;
			GeometryBufferDX11& vb = vertexBuffers[handle.id];
			vb.Release();
			vbHandleAlloc.Free(handle.id);
		}

		void UpdateVertexBuffer(VertexBufferHandle handle, size_t size, const void* data) override
		{
			if (!vbHandleAlloc.InUse(handle.id)) return;
			GeometryBufferDX11& vb = vertexBuffers[handle.id];
			vb.Update(device, context, size, data);
		}

		IndexBufferHandle GraphicsAPIDX11::CreateIndexBuffer(size_t size, bool dynamic) override
		{
			uint16_t handle = ibHandleAlloc.Alloc();

			if (handle != invalid_handle)
			{
				GeometryBufferDX11& ib = indexBuffers[handle];
				ib.Reset(size, D3D11_BIND_INDEX_BUFFER, dynamic);
			}

			return IndexBufferHandle{ handle };
		}

		void DestroyIndexBuffer(IndexBufferHandle handle) override
		{
			if (!ibHandleAlloc.InUse(handle.id)) return;
			GeometryBufferDX11& ib = indexBuffers[handle.id];
			ib.Release();
			ibHandleAlloc.Free(handle.id);
		}

		void UpdateIndexBuffer(IndexBufferHandle handle, size_t size, const void* data) override
		{
			if (!ibHandleAlloc.InUse(handle.id)) return;
			GeometryBufferDX11& ib = indexBuffers[handle.id];
			ib.Update(device, context, size, data);
		}

		RenderTargetHandle CreateRenderTarget(PixelFormat format, uint32_t width, uint32_t height, bool isDepth, bool hasStencil) override
		{
			uint16_t handle = rtHandleAlloc.Alloc();

			if (handle != invalid_handle)
			{
				RenderTargetDX11& rt = renderTargets[handle];
				rt.Reset(format, width, height, isDepth, hasStencil);

				ID3D11Texture2D* tex = nullptr;

				D3D11_TEXTURE2D_DESC desc{ 0 };
				desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				desc.Width = width;
				desc.Height = height;
				desc.ArraySize = 1;
				desc.BindFlags = isDepth ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;
				desc.MipLevels = 1;
				desc.SampleDesc.Count = 1;
				desc.SampleDesc.Quality = 0;

				if (FAILED(device->CreateTexture2D(&desc, nullptr, &tex)))
				{
					rtHandleAlloc.Free(handle);
					return;
				}

				if (isDepth)
				{
					if (FAILED(device->CreateDepthStencilView(tex, nullptr, &(rt.depthStencilView))))
					{
						rtHandleAlloc.Free(handle);
						tex->Release();
						return;
					}
				}
				else
				{
					if (FAILED(device->CreateRenderTargetView(tex, nullptr, &(rt.renderTargetView))))
					{
						rtHandleAlloc.Free(handle);
						tex->Release();
						return;
					}
				}

				tex->Release();
			}

			return RenderTargetHandle{ handle };
		}

		void DestroyRenderTarget(RenderTargetHandle handle) override
		{
			if (!rtHandleAlloc.InUse(handle.id)) return;
			RenderTargetDX11& rt = renderTargets[handle.id];
			rt.Release();
			rtHandleAlloc.Free(handle.id);
		}

		VertexShaderHandle CreateVertexShader(const void* bytecode, size_t size) override
		{
			uint32_t handle = vsHandleAlloc.Alloc();

			if (invalid_handle == handle)
				return VertexShaderHandle{ invalid_handle };

			ID3D11VertexShader* shader = nullptr;
			if (FAILED(device->CreateVertexShader(bytecode, size, nullptr, &shader)))
			{
				vsHandleAlloc.Free(handle);
				return VertexShaderHandle{ invalid_handle };
			}

			// TODO Reflect

			return VertexShaderHandle{ handle };
		}

#pragma endregion
		// interface end
	};


}