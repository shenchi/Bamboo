#include "GraphicsAPIDX11.h"

#include <Windows.h>
#include <d3d11_1.h>

#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define RELEASE(x) if (nullptr != (x)) { (x)->Release(); (x) = nullptr; }
#define C(x, ret) if ((x) != S_OK) { return (ret); }
#define CHECKED(x) if ((x) != S_OK) { return false; }

namespace bamboo
{
	namespace dx11
	{

		constexpr size_t MaxShaderResourceBindingSlot = 128;

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
			"TEXCOORD",
			"TEXCOORD",
			"TEXCOORD",
			"TEXCOORD",
		};

		UINT InputSemanticsIndex[] =
		{
			0,
			0,
			0,
			0,
			0,
			0,
			1,
			2,
			3,
		};

		D3D11_PRIMITIVE_TOPOLOGY PrimitiveTypeTable[] =
		{
			D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
			D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		};

		DXGI_FORMAT PixelFormatTable[] =
		{
			DXGI_FORMAT_UNKNOWN, // AUTO
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R8G8B8A8_SNORM,
			DXGI_FORMAT_R16G16B16A16_UNORM,
			DXGI_FORMAT_R16G16B16A16_SNORM,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			DXGI_FORMAT_R16_SINT,
			DXGI_FORMAT_R32_SINT,
			DXGI_FORMAT_R16_UINT,
			DXGI_FORMAT_R32_UINT,
			DXGI_FORMAT_D24_UNORM_S8_UINT,
		};

		PixelFormat PixelFormatFromDXGI(DXGI_FORMAT format)
		{
			for (unsigned i = PixelFormat::FORMAT_AUTO; i < PixelFormat::NUM_PIXEL_FORMAT; ++i)
			{
				if (PixelFormatTable[i] == format)
				{
					return static_cast<PixelFormat>(i);
				}
			}
			return FORMAT_AUTO;
		}

		struct BindingLayoutDX11
		{
			uint32_t					entryCount;
			BindingLayout				layout;
			uint32_t					offsets[MaxBindingLayoutEntry];
			ID3D11Buffer*				cbs[MaxBindingLayoutEntry];

			bool Reset(ID3D11Device* device, const BindingLayout& layout)
			{
				Release();

				uint32_t offset = 0, i = 0;

				for (; i < MaxBindingLayoutEntry; ++i)
				{
					auto& entry = layout.table[i];
					if (entry.Type == BINDING_SLOT_TYPE_NONE)
					{
						entryCount = i;
						break;
					}

					offsets[i] = offset;
					uint32_t count = (
						entry.Type == BINDING_SLOT_TYPE_TABLE ?
						0 : (entry.Count == 0 ? 1 : entry.Count));
					uint32_t size = 4 * count;
					offset += size;

					if (entry.Type == BINDING_SLOT_TYPE_CONSTANT)
					{
						D3D11_BUFFER_DESC desc = {};
						desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
						desc.ByteWidth = ((size + 15u) & (~15u));
						desc.Usage = D3D11_USAGE_DYNAMIC;
						desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
						if (FAILED(device->CreateBuffer(&desc, nullptr, &cbs[i])))
						{
							Release();
							return false;
						}
					}
				}

				if (i == MaxBindingLayoutEntry)
				{
					entryCount = MaxBindingLayoutEntry;
				}

				this->layout = layout;

				return true;
			}

			void Release()
			{
				for (int i = 0; i < MaxBindingLayoutEntry; i++)
				{
					if (nullptr != cbs[i])
					{
						cbs[i]->Release();
						cbs[i] = nullptr;
					}
				}
				entryCount = 0;
				layout = {};
			}
		};

		struct BufferDX11
		{
			ID3D11Buffer*				buffer;
			ID3D11ShaderResourceView*	srv;
			UINT						size;
			UINT						stride;
			UINT						bindFlags;
			bool						dynamic;

			void Reset(UINT size, UINT bindFlags, bool dynamic)
			{
				Release();
				if (bindFlags & BINDING_CONSTANT_BUFFER)
				{
					size = ((size + 15u) & (~15u));
				}
				this->size = size;
				this->bindFlags = bindFlags;
				this->dynamic = dynamic;
				stride = 0;
			}

			void Release()
			{
				RELEASE(buffer);
				RELEASE(srv);
				size = 0;
				bindFlags = 0;
				dynamic = false;
				stride = 0;
			}

			bool Update(ID3D11Device1* device, ID3D11DeviceContext1* context, UINT size, const void* data, UINT stride, PixelFormat format)
			{
				if (nullptr == buffer)
				{
					this->stride = stride;

					D3D11_BUFFER_DESC desc = {};
					desc.BindFlags = bindFlags;
					desc.ByteWidth = size;
					desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
					desc.StructureByteStride = stride;
					if ((bindFlags & BINDING_SHADER_RESOURCE) && stride > sizeof(float) * 4)
						desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
					if (dynamic) desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

					D3D11_SUBRESOURCE_DATA data_desc = {};
					data_desc.pSysMem = data;

					CHECKED(device->CreateBuffer(&desc, &data_desc, &(buffer)));

					if ((bindFlags & BINDING_SHADER_RESOURCE) != 0)
					{
						D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
						srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
						srvDesc.Buffer.NumElements = this->size / stride;
						srvDesc.Format = PixelFormatTable[format];
						if (FAILED(device->CreateShaderResourceView(buffer, &srvDesc, &srv)))
						{
							RELEASE(buffer);
							return false;
						}

					}
				}
				else if (dynamic)
				{
					D3D11_MAPPED_SUBRESOURCE res = {};
					context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
					memcpy(res.pData, data, min(size, this->size));
					context->Unmap(buffer, 0);
				}
				else
				{
					context->UpdateSubresource(buffer, 0, nullptr, data, 0, 0);
				}

				return true;
			}
		};

		struct TextureDX11
		{
			ID3D11Resource*				texture;
			ID3D11ShaderResourceView*	srv;
			ID3D11RenderTargetView*		rtv;
			ID3D11DepthStencilView*		dsv;

			UINT						bindFlags;

			TextureType					type;
			PixelFormat					format;
			uint32_t					width;
			uint32_t					height;
			uint32_t					depth;
			uint32_t					arraySize;
			uint32_t					mipLevels;

			bool						dynamic;

			void Reset(TextureType type, PixelFormat format, uint32_t bindFlags, uint32_t width, uint32_t height = 1, uint32_t depth = 1, uint32_t arraySize = 1, uint32_t mipLevels = 1, bool dynamic = false)
			{
				Release();
				this->type = type;
				this->format = format;
				this->bindFlags = bindFlags;
				this->width = width;
				this->height = height;
				this->depth = depth;
				this->arraySize = arraySize;
				this->mipLevels = mipLevels;
				this->dynamic = dynamic;
			}

			bool Update(ID3D11Device1* device, ID3D11DeviceContext1* context, UINT pitch, const void* data)
			{
				if (nullptr == texture)
				{
					if (type == TEXTURE_1D)
					{
						D3D11_TEXTURE1D_DESC desc = {};

						desc.Width = width;
						desc.MipLevels = mipLevels;
						desc.ArraySize = arraySize;
						desc.Format = PixelFormatTable[format];
						desc.BindFlags = bindFlags;
						desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
						if (dynamic) desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

						D3D11_SUBRESOURCE_DATA data_desc = {};
						data_desc.pSysMem = data;
						data_desc.SysMemPitch = pitch;

						ID3D11Texture1D* tex1d = nullptr;
						CHECKED(device->CreateTexture1D(&desc, &data_desc, &(tex1d)));
						texture = tex1d;
					}
					else if (type == TEXTURE_2D || type == TEXTURE_CUBE)
					{
						D3D11_TEXTURE2D_DESC desc = {};
						desc.Width = width;
						desc.Height = height;
						desc.MipLevels = mipLevels; // TODO
						desc.ArraySize = arraySize;
						desc.Format = PixelFormatTable[format];
						desc.SampleDesc.Count = 1;
						desc.BindFlags = bindFlags;
						desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
						if (dynamic) desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
						if (type == TEXTURE_CUBE)
							desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

						D3D11_SUBRESOURCE_DATA data_desc = {};
						data_desc.pSysMem = data;
						data_desc.SysMemPitch = pitch;

						ID3D11Texture2D* tex2d = nullptr;
						CHECKED(device->CreateTexture2D(&desc, &data_desc, &(tex2d)));
						texture = tex2d;
					}
					else if (type == TEXTURE_3D)
					{
						D3D11_TEXTURE3D_DESC desc = {};
						desc.Width = width;
						desc.Height = height;
						desc.Depth = depth;
						desc.MipLevels = mipLevels; // TODO
						desc.Format = PixelFormatTable[format];
						desc.BindFlags = bindFlags;
						desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
						if (dynamic) desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
						if (type == TEXTURE_CUBE)
							desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

						D3D11_SUBRESOURCE_DATA data_desc = {};
						data_desc.pSysMem = data;
						data_desc.SysMemPitch = pitch;

						ID3D11Texture3D* tex3d = nullptr;
						CHECKED(device->CreateTexture3D(&desc, &data_desc, &(tex3d)));
						texture = tex3d;
					}

					if (bindFlags & BINDING_SHADER_RESOURCE)
					{
						if (FAILED(device->CreateShaderResourceView(texture, nullptr, &srv)))
						{
							RELEASE(texture);
							return false;
						}
					}
					if (bindFlags & BINDING_RENDER_TARGET)
					{
						// TODO mipmaps
						if (FAILED(device->CreateRenderTargetView(texture, nullptr, &rtv)))
						{
							RELEASE(texture);
							return false;
						}
					}
					if (bindFlags & BINDING_DEPTH_STENCIL)
					{
						if (FAILED(device->CreateDepthStencilView(texture, nullptr, &dsv)))
						{
							RELEASE(texture);
							return false;
						}
					}

				}
				else if (dynamic)
				{
					// TODO for mipmaps and texture array
					D3D11_MAPPED_SUBRESOURCE res = {};
					context->Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
					const uint8_t* pSrc = reinterpret_cast<const uint8_t*>(data);
					uint8_t* pDst = reinterpret_cast<uint8_t*>(res.pData);

					for (uint32_t i = 0; i < height; ++i)
					{
						memcpy(pDst, pSrc, pitch * height);

						pSrc += pitch * height;
						pDst += res.RowPitch;
					}

					context->Unmap(texture, 0);
				}
				else
				{
					// TODO for mipmaps and texture array
					context->UpdateSubresource(texture, 0, nullptr, data, pitch, 0);
				}

				return true;
			}

			void Release()
			{
				RELEASE(texture);
				RELEASE(srv);
				RELEASE(rtv);
				RELEASE(dsv);
			}
		};

		struct SamplerDX11
		{
			ID3D11SamplerState*			sampler;

			void Reset(ID3D11Device* device)
			{
				// TODO
				D3D11_SAMPLER_DESC desc = {};
				desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
				desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
				desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
				desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
				desc.MaxLOD = D3D11_FLOAT32_MAX;

				if (FAILED(device->CreateSamplerState(&desc, &sampler)))
				{
					// error
				}
			}

			void Release()
			{
				RELEASE(sampler);
			}
		};

		struct VertexShaderDX11
		{
			ID3D11VertexShader*		shader;
			void*					byteCode;
			SIZE_T					length;

			void Release()
			{
				RELEASE(shader);
				if (nullptr != byteCode)
				{
					delete[] reinterpret_cast<uint8_t*>(byteCode);
					byteCode = nullptr;
					length = 0;
				}
			}
		};

		struct PixelShaderDX11
		{
			ID3D11PixelShader*		shader;

			void Release()
			{
				RELEASE(shader);
			}
		};

		struct PipelineStateDX11
		{
			ID3D11InputLayout*			layout;
			ID3D11RasterizerState*		rsState;
			ID3D11DepthStencilState*	dsState;

			BindingLayoutHandle			bindingLayout;

			VertexShaderHandle			vs;
			PixelShaderHandle			ps;

			D3D11_PRIMITIVE_TOPOLOGY	topology;

			bool Reset(ID3D11Device* device, const PipelineState& state, VertexShaderDX11* _vs)
			{
				Release();

				if (nullptr != _vs)
				{
					D3D11_INPUT_ELEMENT_DESC elements[MaxVertexInputElement];
					uint16_t elementCount = state.VertexLayout.ElementCount;

					UINT offset = 0;
					UINT lastSlot = 0;

					for (size_t i = 0; i < elementCount; ++i)
					{
						const VertexInputElement& elem = state.VertexLayout.Elements[i];
						D3D11_INPUT_ELEMENT_DESC& desc = elements[i];

						size_t size = InputSlotSizeTable[elem.ComponentType] * (elem.ComponentCount + 1);

						if (elem.BindingSlot != lastSlot)
							offset = 0;

						desc.AlignedByteOffset = offset;
						desc.Format = InputSlotTypeTable[elem.ComponentType][elem.ComponentCount];
						desc.InputSlot = elem.BindingSlot;
						desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
						desc.InstanceDataStepRate = 0;
						desc.SemanticIndex = InputSemanticsIndex[elem.SemanticId];
						desc.SemanticName = InputSemanticsTable[elem.SemanticId];

						offset += static_cast<UINT>(size);
						lastSlot = elem.BindingSlot;
					}

					if (FAILED(device->CreateInputLayout(elements, elementCount, _vs->byteCode, _vs->length, &layout)))
					{
						return false;
					}
				}

				{
					D3D11_RASTERIZER_DESC rsDesc = {};
					rsDesc.FillMode = D3D11_FILL_SOLID;
					rsDesc.CullMode = static_cast<D3D11_CULL_MODE>(state.CullMode + 1);
					rsDesc.DepthClipEnable = TRUE;
					if (FAILED(device->CreateRasterizerState(&rsDesc, &rsState)))
					{
						return false;
					}

					D3D11_DEPTH_STENCIL_DESC dsDesc = {};
					dsDesc.DepthEnable = state.DepthEnable;
					dsDesc.DepthWriteMask = static_cast<D3D11_DEPTH_WRITE_MASK>(state.DepthWrite);
					dsDesc.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(state.DepthFunc + 1);
					if (FAILED(device->CreateDepthStencilState(&dsDesc, &dsState)))
					{
						return false;
					}
				}

				bindingLayout = state.BindingLayout;
				vs = state.VertexShader;
				ps = state.PixelShader;
				topology = PrimitiveTypeTable[state.PrimitiveType];

				return true;
			}

			void Release()
			{
				RELEASE(layout);
				RELEASE(rsState);
				RELEASE(dsState);
			}
		};


		struct GraphicsAPIDX11 : public GraphicsAPI
		{
			int							width;
			int							height;

			HWND						hWnd;

			IDXGISwapChain1*			swapChain;
			ID3D11Device1*				device;
			ID3D11DeviceContext1*		context;

			BindingLayoutDX11			bindingLayouts[MaxBindingLayoutCount];

			PipelineStateDX11			pipelineStates[MaxPipelineStateCount];

			BufferDX11					buffers[MaxBufferCount];

			TextureDX11					textures[MaxTextureCount];

			SamplerDX11					samplers[MaxSamplerCount];

			VertexShaderDX11			vertexShaders[MaxVertexShaderCount];
			PixelShaderDX11				pixelShaders[MaxPixelShaderCount];

			TextureHandle				defaultColorBuffer;
			TextureHandle				defaultDepthStencilBuffer;

			PipelineStateHandle			currentPipelineState;

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
					defaultColorBuffer.id = texHandleAlloc.Alloc();
					if (0 != defaultColorBuffer.id)
						return -1;

					ID3D11Texture2D* backbufferTex = nullptr;
					if (S_OK != (hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbufferTex)))
					{
						return -1;
					}

					D3D11_TEXTURE2D_DESC desc = {};
					backbufferTex->GetDesc(&desc);

					TextureDX11& rt = textures[defaultColorBuffer.id];

					rt.Reset(TextureType::TEXTURE_2D, PixelFormatFromDXGI(desc.Format), BINDING_RENDER_TARGET, width, height);

					if (S_OK != (hr = device->CreateRenderTargetView(backbufferTex, nullptr, &(rt.rtv))))
					{
						return -1;
					}
					rt.texture = backbufferTex;
				}

				{
					defaultDepthStencilBuffer.id = texHandleAlloc.Alloc();
					if (1 != defaultDepthStencilBuffer.id)
						return -1;

					TextureDX11& ds = textures[defaultDepthStencilBuffer.id];

					ds.Reset(TextureType::TEXTURE_2D, PixelFormat::FORMAT_D24_UNORM_S8_UINT, width, height);
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

					if (S_OK != (hr = device->CreateDepthStencilView(depthStencilTex, nullptr, &(ds.dsv))))
					{
						return -1;
					}
					ds.texture = depthStencilTex;
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

				currentPipelineState.id = invalid_handle;
			}

			void SetPipelineState(const PipelineStateDX11& state)
			{

				// Vertex Shader & Input Layout
				{
					uint16_t handle = state.vs.id;
#if _DEBUG
					if (!vsHandleAlloc.InUse(handle))
						return;
#endif
					VertexShaderDX11& vs = vertexShaders[handle];
					context->VSSetShader(vs.shader, nullptr, 0);
				}

				// Pixel Shader
				{
					uint16_t handle = state.ps.id;
#if _DEBUG
					if (!psHandleAlloc.InUse(handle))
						return;
#endif
					PixelShaderDX11& ps = pixelShaders[handle];
					context->PSSetShader(ps.shader, nullptr, 0);
				}

				context->IASetPrimitiveTopology(state.topology);

				context->IASetInputLayout(state.layout);

				context->RSSetState(state.rsState);
				context->OMSetDepthStencilState(state.dsState, 0 /* TODO !!!!! */);

				// TODO internalState = state;
			}

			bool BindResources(const DrawCall& drawcall)
			{
				// TODO
				// prevent resources bind to be read and written simultaneously


				// Input Assembly
				if (drawcall.VertexBufferCount > 0)
				{
					ID3D11Buffer*	vb[MaxVertexBufferBindingSlot];
					UINT			strides[MaxVertexBufferBindingSlot];
					UINT			offsets[MaxVertexBufferBindingSlot];

					for (size_t i = 0; i < drawcall.VertexBufferCount; ++i)
					{
						uint16_t handle = drawcall.VertexBuffers[i].id;
#if _DEBUG
						if (!bufHandleAlloc.InUse(handle))
							return false;
#endif
						auto& buf = buffers[handle];
						if ((buf.bindFlags & BINDING_VERTEX_BUFFER) == 0)
							return false;

						vb[i] = buf.buffer;
						strides[i] = buf.stride;
						offsets[i] = 0;
					}

					context->IASetVertexBuffers(0, drawcall.VertexBufferCount, vb, strides, offsets);
				}

				if (drawcall.HasIndexBuffer)
				{
					uint16_t handle = drawcall.IndexBuffer.id;
#if _DEBUG
					if (!bufHandleAlloc.InUse(handle))
						return false;
#endif
					auto& buf = buffers[handle];
					if ((buf.bindFlags & BINDING_INDEX_BUFFER) == 0)
						return false;

					context->IASetIndexBuffer(buf.buffer, (buf.stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT), 0);
				}
				/////

				// Rasterizer
				{
					D3D11_VIEWPORT vp =
					{
						drawcall.Viewport.X,
						drawcall.Viewport.Y,
						drawcall.Viewport.Width,
						drawcall.Viewport.Height,
						drawcall.Viewport.ZMin,
						drawcall.Viewport.ZMax
					};

					context->RSSetViewports(1, &vp);
				}

				{
					ID3D11Buffer* vsCBs[MaxConstantBufferBindingSlot] = {};
					ID3D11Buffer* psCBs[MaxConstantBufferBindingSlot] = {};
					UINT vsCBCount = 0, psCBCount = 0;

					ID3D11ShaderResourceView* vsSRVs[MaxShaderResourceBindingSlot] = {};
					ID3D11ShaderResourceView* psSRVs[MaxShaderResourceBindingSlot] = {};
					UINT vsSRVCount = 0, psSRVCount = 0;

					ID3D11SamplerState* vsSamps[MaxSamplerBindingSlot] = {};
					ID3D11SamplerState* psSamps[MaxSamplerBindingSlot] = {};
					UINT vsSampCount = 0, psSampCount = 0;

					if (!psoHandleAlloc.InUse(currentPipelineState.id))
					{
						return false;
					}
					uint16_t handle = pipelineStates[currentPipelineState.id].bindingLayout.id;
					if (!blHandleAlloc.InUse(handle))
					{
						return false;
					}

					BindingLayoutDX11& layout = bindingLayouts[handle];
					const uint8_t* pData = reinterpret_cast<const uint8_t*>(drawcall.ResourceBindingData);

					for (size_t i = 0; i < layout.entryCount; i++)
					{
						auto& entry = layout.layout.table[i];
						switch (entry.Type)
						{
						case BINDING_SLOT_TYPE_CONSTANT:
							if (SHADER_VISIBILITY_ALL == entry.ShaderVisibility ||
								SHADER_VISIBILITY_VERTEX == entry.ShaderVisibility)
							{
								vsCBs[entry.Register] = layout.cbs[i];
								if (entry.Register + 1u > vsCBCount)
									vsCBCount = entry.Register + 1u;
							}
							if (SHADER_VISIBILITY_ALL == entry.ShaderVisibility ||
								SHADER_VISIBILITY_PIXEL == entry.ShaderVisibility)
							{
								psCBs[entry.Register] = layout.cbs[i];
								if (entry.Register + 1u > psCBCount)
									psCBCount = entry.Register + 1u;
							}
							{
								uint32_t size = entry.Count * 4;
								uint32_t offset = layout.offsets[i];
								D3D11_MAPPED_SUBRESOURCE subRes = {};
								if (FAILED(context->Map(layout.cbs[i], 0, D3D11_MAP_WRITE_DISCARD, 0, &subRes)))
									return false;
								memcpy(subRes.pData, pData + offset, size);
								context->Unmap(layout.cbs[i], 0);
							}
							break;
						case BINDING_SLOT_TYPE_CBV:
							for (uint32_t j = 0; j < entry.Count; j++)
							{
								uint32_t r = entry.Register + j;
								uint32_t offset = layout.offsets[i] + 4u * j;
								uint16_t handle = static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>((pData + offset)));

								ID3D11Buffer* buffer = nullptr;

								if (invalid_handle != handle)
								{
									if (!bufHandleAlloc.InUse(handle))
										return false;
									BufferDX11& buf = buffers[handle];
									buffer = buf.buffer;
								}
								
								if (SHADER_VISIBILITY_ALL == entry.ShaderVisibility ||
									SHADER_VISIBILITY_VERTEX == entry.ShaderVisibility)
								{
									vsCBs[r] = buffer;
									if (r + 1u > vsCBCount)
										vsCBCount = r + 1u;
								}
								if (SHADER_VISIBILITY_ALL == entry.ShaderVisibility ||
									SHADER_VISIBILITY_PIXEL == entry.ShaderVisibility)
								{
									psCBs[r] = buffer;
									if (r + 1u > psCBCount)
										psCBCount = r + 1u;
								}
							}
							break;
						case BINDING_SLOT_TYPE_SRV:
							for (uint32_t j = 0; j < entry.Count; j++)
							{
								uint32_t r = entry.Register + j;
								uint32_t offset = layout.offsets[i] + 4u * j;
								uint32_t data = *reinterpret_cast<const uint32_t*>((pData + offset));
								bool isBuffer = (data & 0x80000000u) != 0u;
								uint16_t handle = static_cast<uint16_t>(data & 0xffff);

								ID3D11ShaderResourceView* srv = nullptr;

								if (invalid_handle != handle)
								{
									if (isBuffer)
									{
										if (!bufHandleAlloc.InUse(handle))
											return false;
										BufferDX11& buf = buffers[handle];
										if (nullptr == buf.srv)
											return false;
										srv = buf.srv;
									}
									else
									{
										if (!texHandleAlloc.InUse(handle))
											return false;
										TextureDX11& tex = textures[handle];
										srv = tex.srv;
									}
								}

								if (SHADER_VISIBILITY_ALL == entry.ShaderVisibility ||
									SHADER_VISIBILITY_VERTEX == entry.ShaderVisibility)
								{
									vsSRVs[r] = srv;
									if (r + 1 > vsSRVCount)
										vsSRVCount = r + 1;
								}
								if (SHADER_VISIBILITY_ALL == entry.ShaderVisibility ||
									SHADER_VISIBILITY_PIXEL == entry.ShaderVisibility)
								{
									psSRVs[r] = srv;
									if (r + 1 > psSRVCount)
										psSRVCount = r + 1;
								}
							}
							break;
						case BINDING_SLOT_TYPE_SAMPLER:
							for (uint32_t j = 0; j < entry.Count; j++)
							{
								uint32_t r = entry.Register + j;
								uint32_t offset = layout.offsets[i] + 4u * j;
								uint16_t handle = static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>((pData + offset)));

								ID3D11SamplerState* samp = nullptr;

								if (invalid_handle != handle)
								{
									if (!sampHandleAlloc.InUse(handle))
										return false;
									samp = samplers[handle].sampler;
								}

								if (SHADER_VISIBILITY_ALL == entry.ShaderVisibility ||
									SHADER_VISIBILITY_VERTEX == entry.ShaderVisibility)
								{
									vsSamps[r] = samp;
									if (r + 1u > vsSampCount)
										vsSampCount = r + 1u;
								}
								if (SHADER_VISIBILITY_ALL == entry.ShaderVisibility ||
									SHADER_VISIBILITY_PIXEL == entry.ShaderVisibility)
								{
									psSamps[r] = samp;
									if (r + 1u > psSampCount)
										psSampCount = r + 1u;
								}
							}
							break;
						default:
							break;
						}
					}

					context->VSSetConstantBuffers(0, vsCBCount, vsCBs);
					context->PSSetConstantBuffers(0, psCBCount, psCBs);

					context->VSSetShaderResources(0, vsSRVCount, vsSRVs);
					context->PSSetShaderResources(0, psSRVCount, psSRVs);

					context->VSSetSamplers(0, vsSampCount, vsSamps);
					context->PSSetSamplers(0, psSampCount, psSamps);
				}

				// Render Target
				if (drawcall.RenderTargetCount > 0 || drawcall.HasDepthStencil)
				{
					ID3D11RenderTargetView* rtvs[MaxRenderTargetBindingSlot];
					ID3D11DepthStencilView* dsv = nullptr;

					for (size_t i = 0; i < drawcall.RenderTargetCount; ++i)
					{
						uint16_t handle = drawcall.RenderTargets[i].id;
#if _DEBUG
						if (!texHandleAlloc.InUse(handle))
							return false;
#endif
						auto& tex = textures[handle];
						if (nullptr == tex.rtv)
							return false;

						rtvs[i] = tex.rtv;
					}

					if (drawcall.HasDepthStencil)
					{
						uint16_t handle = drawcall.DepthStencil.id;
#if _DEBUG
						if (!texHandleAlloc.InUse(handle))
							return false;
#endif
						auto& tex = textures[handle];
						if (nullptr == tex.dsv)
							return false;

						dsv = tex.dsv;
					}

					context->OMSetRenderTargets(drawcall.RenderTargetCount, rtvs, dsv);
				}
				else
				{
					ID3D11RenderTargetView* rtv = textures[defaultColorBuffer.id].rtv;
					ID3D11DepthStencilView* dsv = textures[defaultDepthStencilBuffer.id].dsv;
					context->OMSetRenderTargets(1, &rtv, dsv);
				}
				//

				return true;
			}

			// interface implementation
#pragma region interface implementation

			BindingLayoutHandle CreateBindingLayout(const BindingLayout& layout) override
			{
				uint16_t handle = blHandleAlloc.Alloc();

				if (invalid_handle == handle)
					return BindingLayoutHandle{ invalid_handle };

				BindingLayoutDX11& bl = bindingLayouts[handle];
				bl.Reset(device, layout);

				return BindingLayoutHandle{ handle };
			}

			void DestroyBindingLayout(BindingLayoutHandle handle) override
			{
				if (!blHandleAlloc.InUse(handle.id)) return;
				BindingLayoutDX11& bl = bindingLayouts[handle.id];
				bl.Release();
				blHandleAlloc.Free(handle.id);
			}

			PipelineStateHandle CreatePipelineState(const PipelineState& state) override
			{
				uint16_t handle = psoHandleAlloc.Alloc();

				if (invalid_handle == handle) return PipelineStateHandle{ invalid_handle };

				PipelineStateDX11& pso = pipelineStates[handle];

				VertexShaderDX11* vs = nullptr;

				{
					uint16_t handle = state.VertexShader.id;
					if (vsHandleAlloc.InUse(handle))
					{
						vs = &vertexShaders[handle];
					}
				}

				if (!pso.Reset(device, state, vs))
				{
					pso.Release();
					return PipelineStateHandle{ invalid_handle };
				}

				return PipelineStateHandle{ handle };
			}

			void DestroyPipelineState(PipelineStateHandle handle) override
			{
				if (!psoHandleAlloc.InUse(handle.id)) return;
				PipelineStateDX11& pso = pipelineStates[handle.id];
				pso.Release();
				psoHandleAlloc.Free(handle.id);
			}

			BufferHandle GraphicsAPIDX11::CreateBuffer(size_t size, uint32_t bindingFlags, bool dynamic) override
			{
				uint16_t handle = bufHandleAlloc.Alloc();

				if (handle != invalid_handle)
				{
					BufferDX11& vb = buffers[handle];
					vb.Reset(static_cast<UINT>(size), bindingFlags, dynamic);
				}

				return BufferHandle{ handle };
			}

			void DestroyBuffer(BufferHandle handle) override
			{
				if (!bufHandleAlloc.InUse(handle.id)) return;
				BufferDX11& buf = buffers[handle.id];
				buf.Release();
				bufHandleAlloc.Free(handle.id);
			}

			void UpdateBuffer(BufferHandle handle, size_t size, const void* data, size_t stride, PixelFormat format) override
			{
				if (!bufHandleAlloc.InUse(handle.id)) return;
				BufferDX11& vb = buffers[handle.id];
				vb.Update(device, context, static_cast<UINT>(size), data, static_cast<UINT>(stride), format);
			}

			TextureHandle CreateTexture(TextureType type, PixelFormat format, uint32_t bindFlags, uint32_t width, uint32_t height, uint32_t depth, uint32_t arraySize, uint32_t mipLevels, bool dynamic) override
			{
				uint16_t handle = texHandleAlloc.Alloc();

				if (handle != invalid_handle)
				{
					TextureDX11& tex = textures[handle];
					tex.Reset(type, format, bindFlags, width, height, depth, arraySize, mipLevels, dynamic);
				}

				return TextureHandle{ handle };
			}

			TextureHandle CreateTexture(const wchar_t* filename) override
			{
				uint16_t handle = texHandleAlloc.Alloc();

				if (handle != invalid_handle)
				{
					TextureDX11& tex = textures[handle];

					ID3D11Resource* res;
					ID3D11ShaderResourceView* srv;

					size_t fnLen = wcslen(filename);
					if (filename[fnLen - 4] == L'.' &&
						filename[fnLen - 3] == L'd' &&
						filename[fnLen - 2] == L'd' &&
						filename[fnLen - 1] == L's')
					{
						if (FAILED(DirectX::CreateDDSTextureFromFile(device, filename, &res, &srv)))
						{
							texHandleAlloc.Free(handle);
							return TextureHandle{ invalid_handle };
						}
					}
					else if (FAILED(DirectX::CreateWICTextureFromFile(device, filename, &res, &srv)))
					{
						texHandleAlloc.Free(handle);
						return TextureHandle{ invalid_handle };
					}


					{
						D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
						srv->GetDesc(&srvDesc);

						PixelFormat format = PixelFormatFromDXGI(srvDesc.Format);
						if (format == FORMAT_AUTO)
						{
							RELEASE(res);
							RELEASE(srv);
							texHandleAlloc.Free(handle);
							return TextureHandle{ invalid_handle };
						}

						switch (srvDesc.ViewDimension)
						{
						case D3D11_SRV_DIMENSION_TEXTURE1D:
						case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
						{
							D3D11_TEXTURE1D_DESC texDesc = {};
							ID3D11Texture1D* tex1d = nullptr;
							if (FAILED(res->QueryInterface(&tex1d)))
							{
								RELEASE(res);
								RELEASE(srv);
								texHandleAlloc.Free(handle);
								return TextureHandle{ invalid_handle };
							}
							tex1d->GetDesc(&texDesc);
							tex1d->Release();
							tex.Reset(
								TEXTURE_1D,
								format,
								BINDING_SHADER_RESOURCE,
								texDesc.Width,
								1,
								1,
								texDesc.ArraySize,
								texDesc.MipLevels
							);
						}
						break;
						case D3D11_SRV_DIMENSION_TEXTURE2D:
						case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
						case D3D11_SRV_DIMENSION_TEXTURE2DMS:
						case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
						case D3D11_SRV_DIMENSION_TEXTURECUBE:
						case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
						{
							D3D11_TEXTURE2D_DESC texDesc = {};
							ID3D11Texture2D* tex2d = nullptr;
							if (FAILED(res->QueryInterface(&tex2d)))
							{
								RELEASE(res);
								RELEASE(srv);
								texHandleAlloc.Free(handle);
								return TextureHandle{ invalid_handle };
							}
							tex2d->GetDesc(&texDesc);
							tex2d->Release();
							tex.Reset(
								((srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE) ||
								(srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE) ? TEXTURE_CUBE : TEXTURE_2D),
								format,
								BINDING_SHADER_RESOURCE,
								texDesc.Width,
								texDesc.Height,
								1,
								texDesc.ArraySize,
								texDesc.MipLevels
							);
						}
						break;
						case D3D11_SRV_DIMENSION_TEXTURE3D:
						{
							D3D11_TEXTURE3D_DESC texDesc = {};
							ID3D11Texture3D* tex3d = nullptr;
							if (FAILED(res->QueryInterface(&tex3d)))
							{
								RELEASE(res);
								RELEASE(srv);
								texHandleAlloc.Free(handle);
								return TextureHandle{ invalid_handle };
							}
							tex3d->GetDesc(&texDesc);
							tex3d->Release();
							tex.Reset(
								TEXTURE_3D,
								format,
								BINDING_SHADER_RESOURCE,
								texDesc.Width,
								texDesc.Height,
								texDesc.Depth,
								1,
								texDesc.MipLevels
							);
						}
						break;
						default:
							RELEASE(res);
							RELEASE(srv);
							texHandleAlloc.Free(handle);
							return TextureHandle{ invalid_handle };
						}

						tex.texture = res;
						tex.srv = srv;
					}
				}

				return TextureHandle{ handle };
			}

			void DestroyTexture(TextureHandle handle) override
			{
				if (!texHandleAlloc.InUse(handle.id)) return;
				TextureDX11& tex = textures[handle.id];
				tex.Release();
				texHandleAlloc.Free(handle.id);
			}

			void UpdateTexture(TextureHandle handle, size_t pitch, const void* data) override
			{
				if (!texHandleAlloc.InUse(handle.id)) return;
				TextureDX11& tex = textures[handle.id];
				tex.Update(device, context, static_cast<UINT>(pitch), data);
			}


			void Clear(TextureHandle handle, float color[4]) override
			{
				if (!texHandleAlloc.InUse(handle.id))
					handle = defaultColorBuffer; // TODO another way to create swap chain buffer
				TextureDX11& tex = textures[handle.id];
				if (nullptr == tex.rtv) return;
				context->ClearRenderTargetView(tex.rtv, color);
			}

			void ClearDepth(TextureHandle handle, float depth) override
			{
				if (!texHandleAlloc.InUse(handle.id))
					handle = defaultDepthStencilBuffer; // TODO another way to create swap chain buffer
				TextureDX11& tex = textures[handle.id];
				if (nullptr == tex.dsv) return;
				context->ClearDepthStencilView(tex.dsv, D3D11_CLEAR_DEPTH, depth, 0);
			}

			void ClearDepthStencil(TextureHandle handle, float depth, uint8_t stencil) override
			{
				if (!texHandleAlloc.InUse(handle.id))
					handle = defaultDepthStencilBuffer; // TODO another way to create swap chain buffer
				TextureDX11& tex = textures[handle.id];
				if (nullptr == tex.dsv || tex.format != FORMAT_D24_UNORM_S8_UINT) return;
				context->ClearDepthStencilView(tex.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
			}

			SamplerHandle CreateSampler() override
			{
				uint16_t handle = sampHandleAlloc.Alloc();

				if (handle != invalid_handle)
				{
					SamplerDX11& s = samplers[handle];
					s.Reset(device);
				}

				return SamplerHandle{ handle };
			}

			void DestroySampler(SamplerHandle handle) override
			{
				if (!sampHandleAlloc.InUse(handle.id)) return;
				SamplerDX11& s = samplers[handle.id];
				s.Release();
				sampHandleAlloc.Free(handle.id);
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

			void Draw(PipelineStateHandle stateHandle, const DrawCall& drawcall) override
			{
				if (stateHandle.id != currentPipelineState.id)
				{
					if (!psoHandleAlloc.InUse(stateHandle.id))
					{
						return; // TODO error !
					}

					PipelineStateDX11& state = pipelineStates[stateHandle.id];
					SetPipelineState(state);

					currentPipelineState = stateHandle;
				}

				BindResources(drawcall);
				if (drawcall.HasIndexBuffer)
				{
					context->DrawIndexed(drawcall.ElementCount, 0, 0);
				}
				else
				{
					context->Draw(drawcall.ElementCount, 0);
				}
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

				CLEAR_ARRAY(bindingLayouts, MaxBindingLayoutCount, blHandleAlloc);
				CLEAR_ARRAY(pipelineStates, MaxPipelineStateCount, psoHandleAlloc);
				CLEAR_ARRAY(buffers, MaxBufferCount, bufHandleAlloc);
				CLEAR_ARRAY(textures, MaxTextureCount, texHandleAlloc);
				CLEAR_ARRAY(samplers, MaxSamplerCount, sampHandleAlloc);
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