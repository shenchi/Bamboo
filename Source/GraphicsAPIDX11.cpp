#include "GraphicsAPIDX11.h"

#include <Windows.h>
#include <d3d11_1.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace bamboo
{
	namespace dx11
	{

		DXGI_FORMAT InputSlotTypeTable[][4] =
		{
			// TYPE_FLOAT
			{ DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT },
			// TYPE_INT8
			{ DXGI_FORMAT_R8_SINT, DXGI_FORMAT_R8G8_SINT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_SINT },
			// TYPE_UINT8
			{ DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UINT },
			// TYPE_INT16
			{ DXGI_FORMAT_R16_SINT, DXGI_FORMAT_R16G16_SINT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R16G16B16A16_SINT },
			// TYPE_UINT16
			{ DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R16G16B16A16_UINT },
			// TYPE_INT32
			{ DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32B32_SINT, DXGI_FORMAT_R32G32B32A32_SINT },
			// TYPE_UINT32
			{ DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32A32_UINT },
		};

		size_t InputSlotSizeTable[] =
		{
			4, // TYPE_FLOAT
			1, // TYPE_INT8
			1, // TYPE_UINT8
			2, // TYPE_INT16
			2, // TYPE_UINT16
			4, // TYPE_INT32
			4, // TYPE_UINT32
		};

		DXGI_FORMAT IndexTypeTable[] = 
		{
			DXGI_FORMAT_UNKNOWN, // TYPE_FLOAT
			DXGI_FORMAT_UNKNOWN, // TYPE_INT8
			DXGI_FORMAT_UNKNOWN, // TYPE_UINT8
			DXGI_FORMAT_UNKNOWN, // TYPE_INT16
			DXGI_FORMAT_R16_UINT, // TYPE_UINT16
			DXGI_FORMAT_UNKNOWN, // TYPE_INT32
			DXGI_FORMAT_R32_UINT, // TYPE_UINT32
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

		D3D11_PRIMITIVE_TOPOLOGY PrimitiveTypeTable[] =
		{
			D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
			D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		};


		struct VertexLayoutDX11
		{
			D3D11_INPUT_ELEMENT_DESC	elements[MaxVertexInputElement];
			uint16_t					elementCount;
		};

		struct BufferDX11
		{
			ID3D11Buffer*	buffer;
			UINT			size;
			UINT			bindFlags;
			bool			dynamic;

			void Reset(UINT size, UINT bindFlags, bool dynamic)
			{
				Release();
				this->size = size;
				this->bindFlags = bindFlags;
				this->dynamic = dynamic;
			}

			void Release()
			{
				if (nullptr != buffer)
				{
					buffer->Release();
					buffer = nullptr;
					size = 0;
					dynamic = false;
				}
			}


			void Update(ID3D11Device1* device, ID3D11DeviceContext1* context, UINT size, const void* data)
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

		struct GeometryBufferDX11 : public BufferDX11
		{
			UINT			stride;
		};

		struct IndexBufferDX11 : public BufferDX11
		{
			DXGI_FORMAT		type;
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

		struct VertexShaderDX11
		{
			ID3D11VertexShader*		shader;
			void*					byteCode;
			SIZE_T					length;

			void Release()
			{
				if (nullptr != shader)
				{
					shader->Release();
					shader = nullptr;

					delete[] reinterpret_cast<uint8_t*>(byteCode);
					byteCode = nullptr;
				}
			}
		};

		struct PixelShaderDX11
		{
			ID3D11PixelShader*		shader;

			void Release()
			{
				if (nullptr != shader)
				{
					shader->Release();
					shader = nullptr;
				}
			}
		};


		struct GraphicsAPIDX11 : public GraphicsAPI
		{
			int width;
			int height;

			HWND						hWnd;

			IDXGISwapChain1*			swapChain;
			ID3D11Device1*				device;
			ID3D11DeviceContext1*		context;

			VertexLayoutDX11			vertexLayouts[MaxVertexLayoutCount];

			GeometryBufferDX11			vertexBuffers[MaxVertexBufferCount];
			IndexBufferDX11				indexBuffers[MaxIndexBufferCount];

			BufferDX11					constantBuffers[MaxConstantBufferCount];
			RenderTargetDX11			renderTargets[MaxRenderTargetCount];

			TextureDX11					textures[MaxTextureCount];

			VertexShaderDX11			vertexShaders[MaxVertexShaderCount];
			PixelShaderDX11				pixelShaders[MaxPixelShaderCount];

			RenderTargetHandle			defaultColorBuffer;
			RenderTargetHandle			defaultDepthStencilBuffer;

			int Init(void* windowHandle)
			{
				hWnd = reinterpret_cast<HWND>(windowHandle);

				int result = 0;

				if (0 != (result = CreateDevice()))
					return result;

				if (0 != (result = InitRenderTargets()))
					return result;

				InitPipelineStates();

				return 0;
			}

			int CreateDevice()
			{
				UINT creationFlags = 0;

#if _DEBUG
				creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

				HRESULT hr = S_OK;

				{
					D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

					ID3D11Device* device = nullptr;
					ID3D11DeviceContext* context = nullptr;

					if (S_OK != D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags, featureLevels, 2, D3D11_SDK_VERSION, &device, nullptr, &context))
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


				RECT rect = {};
				GetClientRect(hWnd, &rect);
				width = rect.right - rect.left;
				height = rect.bottom - rect.top;

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

			int InitRenderTargets()
			{
				HRESULT hr = S_OK;

				{
					defaultColorBuffer.id = rtHandleAlloc.Alloc();
					if (0 != defaultColorBuffer.id)
						return -1;

					RenderTargetDX11& rt = renderTargets[defaultColorBuffer.id];

					rt.Reset(PixelFormat::FORMAT_AUTO, width, height, false, false);

					ID3D11Texture2D* backbufferTex = nullptr;
					if (S_OK != (hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbufferTex)))
					{
						return -1;
					}
					if (S_OK != (hr = device->CreateRenderTargetView(backbufferTex, nullptr, &(rt.renderTargetView))))
					{
						return -1;
					}
					backbufferTex->Release();
				}

				{
					defaultDepthStencilBuffer.id = rtHandleAlloc.Alloc();
					if (1 != defaultDepthStencilBuffer.id)
						return -1;

					RenderTargetDX11& ds = renderTargets[defaultDepthStencilBuffer.id];

					ds.Reset(PixelFormat::FORMAT_D24_UNORM_S8_UINT, width, height, true, true);
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

					if (S_OK != (hr = device->CreateDepthStencilView(depthStencilTex, nullptr, &(ds.depthStencilView))))
					{
						return -1;
					}

					depthStencilTex->Release();
				}

				return 0;
			}

			void InitPipelineStates()
			{
				// TODO
				{
					D3D11_VIEWPORT viewport{
						0.0f, 0.0f,
						static_cast<float>(width),
						static_cast<float>(height),
						0.0f, 1.0f };

					context->RSSetViewports(1, &viewport);
				}
			}

			void SetPipelineStates(const PipelineState& state)
			{
				// Input Assembly
				context->IASetPrimitiveTopology(PrimitiveTypeTable[state.PrimitiveType]);

				if (state.VertexBufferCount > 0)
				{
					ID3D11Buffer*	vb[MaxVertexBufferBindingSlot];
					UINT			strides[MaxVertexBufferBindingSlot];
					UINT			offsets[MaxVertexBufferBindingSlot];

					for (size_t i = 0; i < state.VertexBufferCount; ++i)
					{
						uint16_t handle = state.VertexBuffers[i].id;
#if _DEBUG
						if (!vbHandleAlloc.InUse(handle))
							return;
#endif

						vb[i] = vertexBuffers[handle].buffer;
						strides[i] = vertexBuffers[handle].stride;
						offsets[i] = 0;
					}

					context->IASetVertexBuffers(0, state.VertexBufferCount, vb, strides, offsets);
				}

				if (state.HasIndexBuffer)
				{
					uint16_t handle = state.IndexBuffer.id;
#if _DEBUG
					if (!ibHandleAlloc.InUse(handle))
						return;
#endif
					context->IASetIndexBuffer(indexBuffers[handle].buffer, indexBuffers[handle].type, 0);
				}

				// Vertex Shader & Input Layout
				{
					uint16_t handle = state.VertexShader.id;
#if _DEBUG
					if (!vsHandleAlloc.InUse(handle))
						return;
#endif
					VertexShaderDX11& vs = vertexShaders[handle];
					context->VSSetShader(vs.shader, nullptr, 0);

					{
						uint16_t handle = state.VertexLayout.id;
#if _DEBUG
						if (!vlHandleAlloc.InUse(handle))
							return;
#endif

						ID3D11InputLayout* layout = nullptr;

						if (FAILED(device->CreateInputLayout(
							vertexLayouts[handle].elements,
							vertexLayouts[handle].elementCount,
							vs.byteCode,
							vs.length,
							&layout
						)))
						{
							return;
						}

						context->IASetInputLayout(layout);

						layout->Release();
					}
				}

				// Rasterizer
				{
					D3D11_VIEWPORT vp =
					{
						state.Viewport.X,
						state.Viewport.Y,
						state.Viewport.Width,
						state.Viewport.Height,
						state.Viewport.ZMin,
						state.Viewport.ZMax
					};

					context->RSSetViewports(1, &vp);
				}

				// Pixel Shader
				{
					uint16_t handle = state.PixelShader.id;
#if _DEBUG
					if (!psHandleAlloc.InUse(handle))
						return;
#endif
					PixelShaderDX11& ps = pixelShaders[handle];
					context->PSSetShader(ps.shader, nullptr, 0);
				}

				// Render Target
				if (state.RenderTargetCount > 0 || state.HasDepthStencil)
				{
					ID3D11RenderTargetView* rtvs[MaxRenderTargetBindingSlot];
					ID3D11DepthStencilView* dsv = nullptr;

					for (size_t i = 0; i < state.RenderTargetCount; ++i)
					{
						uint16_t handle = state.RenderTargets[i].id;
#if _DEBUG
						if (!rtHandleAlloc.InUse(handle))
							return;
#endif
						auto rt = renderTargets[handle];
						if (rt.isDepth)
							return;

						rtvs[i] = rt.renderTargetView;
					}

					if (state.HasDepthStencil)
					{
						uint16_t handle = state.DepthStencil.id;
#if _DEBUG
						if (!rtHandleAlloc.InUse(handle))
							return;
#endif
						auto rt = renderTargets[handle];
						if (!rt.isDepth)
							return;

						dsv = rt.depthStencilView;
					}

					context->OMSetRenderTargets(state.RenderTargetCount, rtvs, dsv);
				}
				else
				{
					ID3D11RenderTargetView* rtv = renderTargets[defaultColorBuffer.id].renderTargetView;
					ID3D11DepthStencilView* dsv = renderTargets[defaultDepthStencilBuffer.id].depthStencilView;
					context->OMSetRenderTargets(1, &rtv, dsv);
				}

				internalState = state;
			}

			// interface implementation
#pragma region interface implementation

			VertexLayoutHandle CreateVertexLayout(VertexLayout layout) override
			{
				uint16_t handle = vlHandleAlloc.Alloc();

				if (invalid_handle == handle) return VertexLayoutHandle{ invalid_handle };

				VertexLayoutDX11& vl = vertexLayouts[handle];

				vl.elementCount = layout.ElementCount;

				UINT offset = 0;
				UINT lastSlot = 0;

				for (size_t i = 0; i < layout.ElementCount; ++i)
				{
					VertexInputElement& elem = layout.Elements[i];
					D3D11_INPUT_ELEMENT_DESC& desc = vl.elements[i];

					size_t size = InputSlotSizeTable[elem.ComponentType] * (elem.ComponentCount + 1);

					if (elem.BindingSlot != lastSlot)
						offset = 0;

					desc.AlignedByteOffset = offset;
					desc.Format = InputSlotTypeTable[elem.ComponentType][elem.ComponentCount];
					desc.InputSlot = elem.BindingSlot;
					desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
					desc.InstanceDataStepRate = 0;
					desc.SemanticIndex = 0;
					desc.SemanticName = InputSemanticsTable[elem.SemanticId];

					offset += static_cast<UINT>(size);
					lastSlot = elem.BindingSlot;
				}

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
					vb.Reset(static_cast<UINT>(size), D3D11_BIND_VERTEX_BUFFER, dynamic);
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

			void UpdateVertexBuffer(VertexBufferHandle handle, size_t size, const void* data, size_t stride) override
			{
				if (!vbHandleAlloc.InUse(handle.id)) return;
				GeometryBufferDX11& vb = vertexBuffers[handle.id];
				vb.Update(device, context, static_cast<UINT>(size), data);
				vb.stride = static_cast<UINT>(stride);
			}

			IndexBufferHandle GraphicsAPIDX11::CreateIndexBuffer(size_t size, bool dynamic) override
			{
				uint16_t handle = ibHandleAlloc.Alloc();

				if (handle != invalid_handle)
				{
					IndexBufferDX11& ib = indexBuffers[handle];
					ib.Reset(static_cast<UINT>(size), D3D11_BIND_INDEX_BUFFER, dynamic);
				}

				return IndexBufferHandle{ handle };
			}

			void DestroyIndexBuffer(IndexBufferHandle handle) override
			{
				if (!ibHandleAlloc.InUse(handle.id)) return;
				IndexBufferDX11& ib = indexBuffers[handle.id];
				ib.Release();
				ibHandleAlloc.Free(handle.id);
			}

			void UpdateIndexBuffer(IndexBufferHandle handle, size_t size, const void* data, DataType type) override
			{
				if (!ibHandleAlloc.InUse(handle.id) || (type != TYPE_UINT16 && type != TYPE_UINT32)) return;
				IndexBufferDX11& ib = indexBuffers[handle.id];
				ib.Update(device, context, static_cast<UINT>(size), data);
				ib.type = IndexTypeTable[type];
			}

			ConstantBufferHandle CreateConstantBuffer(size_t size) override
			{
				uint16_t handle = cbHandleAlloc.Alloc();

				if (handle != invalid_handle)
				{
					BufferDX11& cb = constantBuffers[handle];
					cb.Reset(static_cast<UINT>(size), D3D11_BIND_CONSTANT_BUFFER, true);
				}

				return ConstantBufferHandle{ handle };
			}

			void DestroyConstantBuffer(ConstantBufferHandle handle) override
			{
				if (!cbHandleAlloc.InUse(handle.id)) return;
				BufferDX11& cb = constantBuffers[handle.id];
				cb.Release();
				cbHandleAlloc.Free(handle.id);
			}

			void UpdateConstantBuffer(ConstantBufferHandle handle, size_t size, const void* data) override
			{
				if (!cbHandleAlloc.InUse(handle.id)) return;
				BufferDX11& cb = constantBuffers[handle.id];
				cb.Update(device, context, static_cast<UINT>(size), data);
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
						return RenderTargetHandle{ invalid_handle };
					}

					if (isDepth)
					{
						if (FAILED(device->CreateDepthStencilView(tex, nullptr, &(rt.depthStencilView))))
						{
							rtHandleAlloc.Free(handle);
							tex->Release();
							return RenderTargetHandle{ invalid_handle };
						}
					}
					else
					{
						if (FAILED(device->CreateRenderTargetView(tex, nullptr, &(rt.renderTargetView))))
						{
							rtHandleAlloc.Free(handle);
							tex->Release();
							return RenderTargetHandle{ invalid_handle };
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

			void Clear(RenderTargetHandle handle, float color[4]) override
			{
				if (!rtHandleAlloc.InUse(handle.id)) return;
				RenderTargetDX11& rt = renderTargets[handle.id];
				if (rt.isDepth) return;
				context->ClearRenderTargetView(rt.renderTargetView, color);
			}

			void ClearDepth(RenderTargetHandle handle, float depth) override
			{
				if (!rtHandleAlloc.InUse(handle.id)) 
					handle = defaultColorBuffer; // TODO another way to create swap chain buffer
				RenderTargetDX11& rt = renderTargets[handle.id];
				if (!rt.isDepth) return;
				context->ClearDepthStencilView(rt.depthStencilView, D3D11_CLEAR_DEPTH, depth, 0);
			}

			void ClearDepthStencil(RenderTargetHandle handle, float depth, uint8_t stencil) override
			{
				if (!rtHandleAlloc.InUse(handle.id))
					handle = defaultDepthStencilBuffer; // TODO another way to create swap chain buffer
				RenderTargetDX11& rt = renderTargets[handle.id];
				if (!rt.isDepth || !rt.hasStencil) return;
				context->ClearDepthStencilView(rt.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
			}

			VertexShaderHandle CreateVertexShader(const void* bytecode, size_t size) override
			{
				uint16_t handle = vsHandleAlloc.Alloc();

				if (invalid_handle == handle)
					return VertexShaderHandle{ invalid_handle };

				ID3D11VertexShader* shader = nullptr;
				if (FAILED(device->CreateVertexShader(bytecode, size, nullptr, &shader)))
				{
					vsHandleAlloc.Free(handle);
					return VertexShaderHandle{ invalid_handle };
				}

				VertexShaderDX11& vs = vertexShaders[handle];
				vs.shader = shader;
				vs.byteCode = reinterpret_cast<void*>(new uint8_t[size]); // TODO another way to keep this
				memcpy(vs.byteCode, bytecode, size);
				vs.length = size;

				// TODO Reflect

				return VertexShaderHandle{ handle };
			}

			void DestroyVertexShader(VertexShaderHandle handle) override
			{
				if (!vsHandleAlloc.InUse(handle.id)) return;
				VertexShaderDX11& vs = vertexShaders[handle.id];
				vs.Release();
				vsHandleAlloc.Free(handle.id);
			}

			PixelShaderHandle CreatePixelShader(const void* bytecode, size_t size) override
			{
				uint16_t handle = psHandleAlloc.Alloc();

				if (invalid_handle == handle)
					return PixelShaderHandle{ invalid_handle };

				ID3D11PixelShader* shader = nullptr;
				if (FAILED(device->CreatePixelShader(bytecode, size, nullptr, &shader)))
				{
					psHandleAlloc.Free(handle);
					return PixelShaderHandle{ invalid_handle };
				}

				PixelShaderDX11& ps = pixelShaders[handle];
				ps.shader = shader;

				// TODO reflect

				return PixelShaderHandle{ handle };
			}

			void DestroyPixelShader(PixelShaderHandle handle) override
			{
				if (!psHandleAlloc.InUse(handle.id)) return;
				PixelShaderDX11& ps = pixelShaders[handle.id];
				ps.Release();
				psHandleAlloc.Free(handle.id);
			}

			void Draw(const PipelineState& state, uint32_t vertexCount) override
			{
				SetPipelineStates(state);
				context->Draw(vertexCount, 0);
			}

			void DrawIndex(const PipelineState& state, uint32_t indexCount) override
			{
				SetPipelineStates(state);
				context->DrawIndexed(indexCount, 0, 0);
			}

			void Present() override
			{
				swapChain->Present(0, 0);
			}

			void Shutdown() override
			{
#define CLEAR_ARRAY(arr, count, alloc) \
				for (uint16_t handle = 0; handle < count; ++handle) \
					if (alloc.InUse(handle)) arr[handle].Release();

				CLEAR_ARRAY(vertexBuffers, MaxVertexBufferCount, vbHandleAlloc);
				CLEAR_ARRAY(indexBuffers, MaxIndexBufferCount, ibHandleAlloc);
				CLEAR_ARRAY(constantBuffers, MaxConstantBufferCount, cbHandleAlloc);
				CLEAR_ARRAY(renderTargets, MaxRenderTargetCount, rtHandleAlloc);
				CLEAR_ARRAY(textures, MaxTextureCount, texHandleAlloc);
				CLEAR_ARRAY(vertexShaders, MaxVertexShaderCount, vsHandleAlloc);
				CLEAR_ARRAY(pixelShaders, MaxPixelShaderCount, psHandleAlloc);

#undef CLEAR_ARRAY
				
				swapChain->Release();
				context->Release();
				device->Release();
			}

#pragma endregion
			// interface end
		};


		GraphicsAPI * InitGraphicsAPIDX11(void* windowHandle)
		{
			GraphicsAPIDX11* api = new GraphicsAPIDX11();

			if (0 != api->Init(windowHandle))
			{
				delete api;
				return nullptr;
			}

			return api;
		}

	}

}