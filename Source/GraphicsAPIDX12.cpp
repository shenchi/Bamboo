#include "GraphicsAPIDX12.h"

#include <Windows.h>
#include <dxgi1_5.h>
#include <d3d12.h>
#include <d3dx12.h>

#include <WICTextureLoader12.h>
#include <DDSTextureLoader12.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

#include "UploadHeapDX12.h"

#define RELEASE(x) if (nullptr != (x)) { (x)->Release(); (x) = nullptr; }
#define FREE_HANDLE(h, a) if ((a).InUse(h)) { (a).Free(h); (h) = invalid_handle; }
#define CE(x, e) if (S_OK != (x)) return (e);
#define CHECKED(x) if (S_OK != (x)) return -1;

namespace bamboo
{
	namespace dx12
	{
		constexpr size_t RTVHeapSize = 1024;
		constexpr size_t DSVHeapSize = 1024;
		constexpr size_t SRVHeapSize = 1024;
		constexpr size_t SamplerHeapSize = MaxSamplerCount;


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

		D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyTypeTable[] =
		{
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		};

		uint32_t PixelFormatSizeTable[] =
		{
			0,
			4,
			4,
			8,
			8,
			16,
			2,
			4,
			2,
			4,
			4,
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

		struct PipelineStateDX12
		{
			ID3D12PipelineState*	state;

			PipelineStateDX12()
				:
				state(nullptr)
			{}
		};

		struct BufferDX12
		{
			ID3D12Resource*				buffer;
			uint16_t					cbv;
			uint16_t					srv;
			D3D12_RESOURCE_STATES		state;

			uint32_t					size;
			uint32_t					stride;
			PixelFormat					format;

			BufferDX12()
				:
				buffer(nullptr),
				srv(invalid_handle),
				state(D3D12_RESOURCE_STATE_COMMON),
				size(0),
				stride(0),
				format(FORMAT_AUTO)
			{}
		};

		struct TextureDX12
		{
			ID3D12Resource*				texture;
			uint16_t					srv;
			uint16_t					rtv;
			uint16_t					dsv;
			D3D12_RESOURCE_STATES		state;

			TextureType					type;
			PixelFormat					format;
			uint32_t					width;
			uint32_t					height;
			uint32_t					depth;
			uint32_t					arraySize;
			uint32_t					mipLevels;

			TextureDX12()
				:
				texture(nullptr),
				srv(invalid_handle),
				rtv(invalid_handle),
				dsv(invalid_handle),
				state(D3D12_RESOURCE_STATE_COMMON),
				format(FORMAT_AUTO),
				width(0),
				height(0),
				depth(0),
				arraySize(0),
				mipLevels(0)
			{}
		};

		struct SamplerDX12
		{
			uint32_t				sampler;
			D3D12_SAMPLER_DESC		desc;

			SamplerDX12()
				:
				sampler(invalid_handle),
				desc{}
			{}
		};

		struct ShaderDX12
		{
			uint8_t*			data;
			size_t				size;

			ShaderDX12()
				:
				data(nullptr),
				size(0)
			{}
		};

		struct GraphicsAPIDX12 : public GraphicsAPI
		{
			int							width;
			int							height;

			HWND						hWnd;

			ID3D12Device*				device;
			ID3D12CommandQueue*			cmdQueue;

			IDXGISwapChain4*			swapChain;

			ID3D12CommandAllocator*		cmdAlloc;
			ID3D12GraphicsCommandList*	cmdList;

			ID3D12Fence*				fence;
			HANDLE						fenceEvent;

			UINT						backBufferIndex;
			UINT64						frameIndex;


			HandleAlloc<RTVHeapSize>		rtvHeapAlloc;
			HandleAlloc<DSVHeapSize>		dsvHeapAlloc;
			HandleAlloc<SRVHeapSize>		srvHeapAlloc;
			HandleAlloc<SamplerHeapSize>	sampHeapAlloc;

			ID3D12DescriptorHeap*		rtvHeap;
			ID3D12DescriptorHeap*		dsvHeap;
			ID3D12DescriptorHeap*		srvHeap;
			ID3D12DescriptorHeap*		sampHeap;

			UINT						rtvHeapInc;
			UINT						dsvHeapInc;
			UINT						srvHeapInc;
			UINT						sampHeapInc;

			PipelineStateDX12			pipelineStates[MaxPipelineStateCount];
			BufferDX12					buffers[MaxBufferCount];
			TextureDX12					textures[MaxTextureCount];
			SamplerDX12					samplers[MaxSamplerCount];
			ShaderDX12					vertexShaders[MaxVertexShaderCount];
			ShaderDX12					pixelShaders[MaxPixelShaderCount];

			UploadHeapDX12				uploadHeap;

			int Init(void* windowHandle)
			{
				hWnd = reinterpret_cast<HWND>(windowHandle);

				int result = 0;

				if (0 != (result = InitDirect3D()))
					return result;

				if (0 != (result = InitRenderTargets()))
					return result;

				InitPipelineStates();

				if (!uploadHeap.Init(device))
				{
					return -1;
				}

				return 0;
			}

			int InitDirect3D()
			{
				UINT dxgi_flag = 0;
				IDXGIFactory5* factory = nullptr;
				IDXGIAdapter1* adaptor = nullptr;

#if defined(_DEBUG)
				{
					ID3D12Debug* debug = nullptr;
					if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
					{
						debug->EnableDebugLayer();
						debug->Release();

						dxgi_flag |= DXGI_CREATE_FACTORY_DEBUG;
					}
				}
#endif

				CHECKED(CreateDXGIFactory2(dxgi_flag, IID_PPV_ARGS(&factory)));

				{
					IDXGIAdapter1* adpt = nullptr;
					for (UINT i = 0;
						SUCCEEDED(factory->EnumAdapters1(i, &adpt));
						i++)
					{
						DXGI_ADAPTER_DESC1 desc;
						CHECKED(adpt->GetDesc1(&desc));

						if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
						{
							adaptor = adpt;
							break;
						}

						adpt->Release();
					}

					if (nullptr == adaptor)
						return false;
				}

				CHECKED(D3D12CreateDevice(adaptor, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
				adaptor->Release();

				{
					D3D12_COMMAND_QUEUE_DESC desc = {};
					desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
					CHECKED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&cmdQueue)));
				}

				{
					IDXGISwapChain1* swapChain_ = nullptr;
					DXGI_SWAP_CHAIN_DESC1 desc = {};
					desc.BufferCount = 2;
					desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					desc.Width = width;
					desc.Height = height;
					desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
					desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
					desc.SampleDesc.Count = 1;

					CHECKED(factory->CreateSwapChainForHwnd(cmdQueue, hWnd, &desc, nullptr, nullptr, &swapChain_));
					CHECKED(swapChain_->QueryInterface(IID_PPV_ARGS(&swapChain)));
					swapChain_->Release();

				}

				factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
				factory->Release();

				{
					D3D12_DESCRIPTOR_HEAP_DESC desc = {};
					desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
					desc.NumDescriptors = RTVHeapSize;
					CHECKED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap)));
					rtvHeapInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
					rtvHeapAlloc.Reset();
				}

				{
					D3D12_DESCRIPTOR_HEAP_DESC desc = {};
					desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
					desc.NumDescriptors = DSVHeapSize;
					CHECKED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsvHeap)));
					dsvHeapInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
					dsvHeapAlloc.Reset();
				}

				{
					D3D12_DESCRIPTOR_HEAP_DESC desc = {};
					desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
					desc.NumDescriptors = SRVHeapSize;
					CHECKED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvHeap)));
					srvHeapInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					srvHeapAlloc.Reset();
				}

				{
					D3D12_DESCRIPTOR_HEAP_DESC desc = {};
					desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
					desc.NumDescriptors = SamplerHeapSize;
					CHECKED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&sampHeap)));
					sampHeapInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
					sampHeapAlloc.Reset();
				}

				CHECKED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)));

				CHECKED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList)));

				cmdList->Close();

				frameIndex = 1;
				CHECKED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
				fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

				return 0;
			}

			int InitRenderTargets()
			{
				backBufferIndex = swapChain->GetCurrentBackBufferIndex();

				for (UINT i = 0; i < 2; ++i)
				{
					ID3D12Resource* res = nullptr;
					CHECKED(swapChain->GetBuffer(i, IID_PPV_ARGS(&res)));
					uint16_t handle = InternalCreateTexture(res, BINDING_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET);
					assert(handle == i);
				}

				{
					ID3D12Resource* res = nullptr;

					D3D12_HEAP_PROPERTIES prop = {};
					prop.Type = D3D12_HEAP_TYPE_DEFAULT;

					D3D12_RESOURCE_DESC desc = {};
					desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
					desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					desc.Width = width;
					desc.Height = height;
					desc.DepthOrArraySize = 1;
					desc.MipLevels = 1;
					desc.SampleDesc.Count = 1;
					desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

					D3D12_CLEAR_VALUE clearValue[1];
					clearValue[0].Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					clearValue[0].DepthStencil.Depth = 1.0f;
					clearValue[0].DepthStencil.Stencil = 0;

					CHECKED(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, clearValue, IID_PPV_ARGS(&res)));
					uint16_t handle = InternalCreateTexture(res, BINDING_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE);
					assert(handle == 2);
				}

				return 0;
			}

			void InitPipelineStates()
			{

			}

#pragma region resource creation

			void InternalResetPipelineState(PipelineStateDX12& state)
			{
				RELEASE(state.state);
			}

			uint16_t InternalCreatePipelineState(PipelineState& stateDesc)
			{
				uint16_t handle = psoHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return invalid_handle;

				PipelineStateDX12& state = pipelineStates[handle];

				D3D12_INPUT_ELEMENT_DESC elements[MaxVertexInputElement];
				D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

				{
					desc.pRootSignature = nullptr; // TODO

					uint16_t vsHandle = stateDesc.VertexShader.id;
					if (vsHandleAlloc.InUse(vsHandle))
					{
						desc.VS = CD3DX12_SHADER_BYTECODE(
							vertexShaders[vsHandle].data,
							vertexShaders[vsHandle].size
						);

						uint16_t elementCount = stateDesc.VertexLayout.ElementCount;

						UINT offset = 0;
						UINT lastSlot = 0;

						for (size_t i = 0; i < elementCount; ++i)
						{
							const VertexInputElement& elem = stateDesc.VertexLayout.Elements[i];
							D3D12_INPUT_ELEMENT_DESC& desc = elements[i];

							size_t size = InputSlotSizeTable[elem.ComponentType] * (elem.ComponentCount + 1);

							if (elem.BindingSlot != lastSlot)
								offset = 0;

							desc.AlignedByteOffset = offset;
							desc.Format = InputSlotTypeTable[elem.ComponentType][elem.ComponentCount];
							desc.InputSlot = elem.BindingSlot;
							desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
							desc.InstanceDataStepRate = 0;
							desc.SemanticIndex = InputSemanticsIndex[elem.SemanticId];
							desc.SemanticName = InputSemanticsTable[elem.SemanticId];

							offset += static_cast<UINT>(size);
							lastSlot = elem.BindingSlot;
						}

						desc.InputLayout = { nullptr, stateDesc.VertexLayout.ElementCount };
					}
					else
					{
						desc.VS = CD3DX12_SHADER_BYTECODE();
					}

					uint16_t psHandle = stateDesc.PixelShader.id;
					if (psHandleAlloc.InUse(psHandle))
					{
						desc.PS = CD3DX12_SHADER_BYTECODE(
							pixelShaders[psHandle].data,
							pixelShaders[psHandle].size
						);
					}
					else
					{
						desc.PS = CD3DX12_SHADER_BYTECODE();
					}

					desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
					desc.SampleMask = UINT_MAX;
					desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
					desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
					desc.PrimitiveTopologyType = TopologyTypeTable[stateDesc.PrimitiveType];
					desc.NumRenderTargets = stateDesc.RenderTargetCount;
					for (size_t i = 0; i < desc.NumRenderTargets; i++)
					{
						desc.RTVFormats[i] = PixelFormatTable[stateDesc.RenderTargetFormats[i]];
					}
					desc.DSVFormat = PixelFormatTable[stateDesc.DepthStencilFormat];
					desc.SampleDesc.Count = 1;
				}

				if (FAILED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&(state.state)))))
				{
					InternalResetPipelineState(state);
					psoHandleAlloc.Free(handle);
					return invalid_handle;
				}

				return handle;
			}

			void InternalDestroyPipelineState(uint16_t handle)
			{
				if (!psoHandleAlloc.InUse(handle))
					return;

				PipelineStateDX12& state = pipelineStates[handle];
				InternalResetPipelineState(state);
				psoHandleAlloc.Free(handle);
			}

			void InternalResetBuffer(BufferDX12& buf)
			{
				RELEASE(buf.buffer);
				FREE_HANDLE(buf.cbv, srvHeapAlloc);
				FREE_HANDLE(buf.srv, srvHeapAlloc);
			}

			uint16_t InternalCreateBuffer(uint32_t bindFlags, size_t size)
			{
				uint16_t handle = bufHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return handle;

				BufferDX12& buf = buffers[handle];

				D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE;
				D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

				if (FAILED(device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					heapFlags,
					&CD3DX12_RESOURCE_DESC::Buffer(size, resFlags),
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&(buf.buffer))
				)))
				{
					InternalResetBuffer(buf);
					bufHandleAlloc.Free(handle);
					return invalid_handle;
				}


				if (bindFlags & BINDING_CONSTANT_BUFFER)
				{
					buf.cbv = srvHeapAlloc.Alloc();
					if (invalid_handle == buf.cbv)
					{
						InternalResetBuffer(buf);
						bufHandleAlloc.Free(handle);
						return invalid_handle;
					}

					CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(
						srvHeap->GetCPUDescriptorHandleForHeapStart(),
						buf.cbv,
						srvHeapInc);

					D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
					desc.BufferLocation = buf.buffer->GetGPUVirtualAddress();
					desc.SizeInBytes = (static_cast<UINT>(size) + 255) & ~255;

					device->CreateConstantBufferView(&desc, cbvHandle);
				}

				if (bindFlags & BINDING_SHADER_RESOURCE)
				{
					buf.srv = srvHeapAlloc.Alloc();
					if (invalid_handle == buf.srv)
					{
						InternalResetBuffer(buf);
						bufHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}

				buf.stride = 0;
				buf.format = FORMAT_AUTO;

				return handle;
			}

			void InternalDestroyBuffer(uint16_t handle)
			{
				if (!bufHandleAlloc.InUse(handle))
					return;

				BufferDX12& buf = buffers[handle];

				InternalResetBuffer(buf);
				bufHandleAlloc.Free(handle);
			}

			void InternalUpdateBuffer(uint16_t handle, uint32_t size, const void* data, uint32_t stride, PixelFormat format)
			{
				if (!bufHandleAlloc.InUse(handle))
					return;

				BufferDX12& buf = buffers[handle];

				// update resource buffer view if needed
				if (invalid_handle != buf.srv)
				{
					if (stride == 0)
					{
						if (format == FORMAT_AUTO)
						{
							stride = 4;
							format = FORMAT_R8G8B8A8_UNORM;
						}
						else
						{
							stride = PixelFormatSizeTable[format];
						}
					}
					else
					{
						format = FORMAT_AUTO;
					}

					if (stride != buf.stride || format != buf.format)
					{
						CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
							srvHeap->GetCPUDescriptorHandleForHeapStart(),
							buf.srv,
							srvHeapInc
						);

						D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};

						desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
						desc.Format = PixelFormatTable[format];
						desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						desc.Buffer.FirstElement = 0;
						desc.Buffer.NumElements = size / stride;
						desc.Buffer.StructureByteStride = stride;
						desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

						device->CreateShaderResourceView(buf.buffer, nullptr, srvHandle);
					}
				}

				if (buf.state != D3D12_RESOURCE_STATE_COMMON)
				{
					CD3DX12_RESOURCE_BARRIER trans = CD3DX12_RESOURCE_BARRIER::Transition(buf.buffer, buf.state, D3D12_RESOURCE_STATE_COMMON);
					cmdList->ResourceBarrier(1, &trans);
					buf.state = D3D12_RESOURCE_STATE_COMMON;
				}

				uploadHeap.UploadResource(buf.buffer, size, data, 0);
			}

			void InternalResetTexture(TextureDX12& tex)
			{
				RELEASE(tex.texture);
				FREE_HANDLE(tex.srv, srvHeapAlloc);
				FREE_HANDLE(tex.rtv, rtvHeapAlloc);
				FREE_HANDLE(tex.dsv, dsvHeapAlloc);
			}

			uint16_t InternalCreateTexture(ID3D12Resource* res, uint32_t bindFlags, D3D12_RESOURCE_STATES initialState)
			{
				uint16_t handle = texHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return handle;

				TextureDX12& tex = textures[handle];

				tex.texture = res;
				tex.state = initialState;

				if ((bindFlags & BINDING_SHADER_RESOURCE))
				{
					tex.srv = srvHeapAlloc.Alloc();
					if (invalid_handle == tex.srv)
					{
						InternalResetTexture(tex);
						texHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}
				if (bindFlags & BINDING_RENDER_TARGET)
				{
					tex.rtv = srvHeapAlloc.Alloc();
					if (invalid_handle == tex.rtv)
					{
						InternalResetTexture(tex);
						texHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}
				if (bindFlags & BINDING_DEPTH_STENCIL)
				{
					tex.dsv = dsvHeapAlloc.Alloc();
					if (invalid_handle == tex.dsv)
					{
						InternalResetTexture(tex);
						texHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}

				D3D12_RESOURCE_DESC desc = res->GetDesc();

				PixelFormat format = PixelFormatFromDXGI(desc.Format);
				if (format == FORMAT_AUTO)
				{
					InternalResetTexture(tex);
					texHandleAlloc.Free(handle);
					return invalid_handle;
				}

				tex.format = format;

				switch (desc.Dimension)
				{
				case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
					tex.type = TEXTURE_1D;
					tex.width = desc.Width;
					tex.height = desc.Height;
					tex.depth = 1;
					tex.arraySize = desc.DepthOrArraySize;
					tex.mipLevels = desc.MipLevels;
					break;
				case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
					// TODO
					tex.type = (desc.DepthOrArraySize == 6 ? TEXTURE_CUBE : TEXTURE_2D);
					tex.width = desc.Width;
					tex.height = desc.Height;
					tex.depth = 1;
					tex.arraySize = desc.DepthOrArraySize;
					tex.mipLevels = desc.MipLevels;
					break;
				case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
					tex.type = TEXTURE_3D;
					tex.width = desc.Width;
					tex.height = desc.Height;
					tex.depth = desc.DepthOrArraySize;
					tex.arraySize = 1;
					tex.mipLevels = desc.MipLevels;
					break;
				default:
					InternalResetTexture(tex);
					texHandleAlloc.Free(handle);
					return invalid_handle;
				}

				if (invalid_handle != tex.srv)
				{
					CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvHeap->GetCPUDescriptorHandleForHeapStart(), tex.srv, srvHeapInc);
					device->CreateShaderResourceView(tex.texture, nullptr, srvHandle);
				}

				if (invalid_handle != tex.rtv)
				{
					CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), tex.rtv, rtvHeapInc);
					device->CreateRenderTargetView(tex.texture, nullptr, rtvHandle);
				}

				if (invalid_handle != tex.dsv)
				{
					CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart(), tex.dsv, dsvHeapInc);
					device->CreateDepthStencilView(tex.texture, nullptr, dsvHandle);
				}

				return handle;
			}

			uint16_t InternalCreateTexture(TextureType type, PixelFormat format, uint32_t bindFlags, uint32_t width, uint32_t height, uint32_t depth, uint32_t arraySize, uint32_t mipLevels)
			{
				uint16_t handle = texHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return handle;

				TextureDX12& tex = textures[handle];

				CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);

				D3D12_RESOURCE_FLAGS resFlag = D3D12_RESOURCE_FLAG_NONE;
				if (!(bindFlags & BINDING_SHADER_RESOURCE))
				{
					tex.srv = invalid_handle;
					resFlag |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
				}
				else
				{
					tex.srv = srvHeapAlloc.Alloc();
					if (invalid_handle == tex.srv)
					{
						InternalResetTexture(tex);
						texHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}
				if (bindFlags & BINDING_RENDER_TARGET)
				{
					resFlag |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
					tex.rtv = srvHeapAlloc.Alloc();
					if (invalid_handle == tex.rtv)
					{
						InternalResetTexture(tex);
						texHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}
				else
				{
					tex.rtv = invalid_handle;
				}
				if (bindFlags & BINDING_DEPTH_STENCIL)
				{
					resFlag |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
					tex.dsv = dsvHeapAlloc.Alloc();
					if (invalid_handle == tex.dsv)
					{
						InternalResetTexture(tex);
						texHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}
				else
				{
					tex.dsv = invalid_handle;
				}

				CD3DX12_RESOURCE_DESC resDesc;
				DXGI_FORMAT dxgiFormat = PixelFormatTable[format];

				if (type == TEXTURE_1D)
				{
					resDesc = CD3DX12_RESOURCE_DESC::Tex1D(
						dxgiFormat,
						width,
						arraySize,
						mipLevels,
						resFlag
					);
				}
				else if (type == TEXTURE_2D || type == TEXTURE_CUBE)
				{
					resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
						dxgiFormat,
						width, height,
						(type == TEXTURE_CUBE ? arraySize * 6 : arraySize),
						mipLevels,
						1, 0,
						resFlag
					);
				}
				else if (type == TEXTURE_3D)
				{
					resDesc = CD3DX12_RESOURCE_DESC::Tex3D(
						dxgiFormat,
						width, height, depth,
						mipLevels,
						resFlag
					);
				}

				if (FAILED(device->CreateCommittedResource(&heapProp,
					D3D12_HEAP_FLAG_DENY_BUFFERS,
					&resDesc,
					D3D12_RESOURCE_STATE_COMMON,
					nullptr,
					IID_PPV_ARGS(&tex.texture))))
				{
					InternalResetTexture(tex);
					texHandleAlloc.Free(handle);
					return invalid_handle;
				}

				if (invalid_handle != tex.srv)
				{
					CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvHeap->GetCPUDescriptorHandleForHeapStart(), tex.srv, srvHeapInc);
					device->CreateShaderResourceView(tex.texture, nullptr, srvHandle);
				}

				if (invalid_handle != tex.rtv)
				{
					CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), tex.rtv, rtvHeapInc);
					device->CreateRenderTargetView(tex.texture, nullptr, rtvHandle);
				}

				if (invalid_handle != tex.dsv)
				{
					CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart(), tex.dsv, dsvHeapInc);
					device->CreateDepthStencilView(tex.texture, nullptr, dsvHandle);
				}

				tex.state = D3D12_RESOURCE_STATE_COMMON;

				tex.type = type;
				tex.format = format;
				tex.width = width;
				tex.height = height;
				tex.depth = depth;
				tex.arraySize = arraySize;
				tex.mipLevels = mipLevels;

				return handle;
			}

			uint16_t InternalCreateTexture(const wchar_t* filename)
			{
				ID3D12Resource* res = nullptr;
				D3D12_SUBRESOURCE_DATA data = {};
				std::unique_ptr<uint8_t[]> ptr;
				if (FAILED(DirectX::LoadWICTextureFromFile(
					device,
					filename,
					&res,
					ptr,
					data)))
				{
					return invalid_handle;
				}

				uint16_t handle = InternalCreateTexture(res, BINDING_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);

				if (invalid_handle == handle)
				{
					return invalid_handle;
				}

				cmdList->ResourceBarrier(1,
					&CD3DX12_RESOURCE_BARRIER::Transition(res, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON)
				);

				uploadHeap.UploadResource(res, 0, ptr.get(), data.RowPitch);

				return handle;
			}

			void InternalDestroyTexture(uint16_t handle)
			{
				if (!texHandleAlloc.InUse(handle))
					return;

				TextureDX12& tex = textures[handle];

				InternalResetTexture(tex);
				texHandleAlloc.Free(handle);
			}

			void InternalUpdateTexture(uint16_t handle, const void* data, uint32_t rowPitch)
			{
				if (!texHandleAlloc.InUse(handle))
					return;

				TextureDX12& tex = textures[handle];

				if (tex.state != D3D12_RESOURCE_STATE_COMMON)
				{
					CD3DX12_RESOURCE_BARRIER trans = CD3DX12_RESOURCE_BARRIER::Transition(tex.texture, tex.state, D3D12_RESOURCE_STATE_COMMON);
					cmdList->ResourceBarrier(1, &trans);
					tex.state = D3D12_RESOURCE_STATE_COMMON;
				}

				uploadHeap.UploadResource(tex.texture, 0 /* auto */, data, rowPitch);
			}

			void InternalResetSampler(SamplerDX12& samp)
			{
				FREE_HANDLE(samp.sampler, sampHeapAlloc);
				samp.desc = {};
			}

			uint16_t InternalCreateSampler()
			{
				uint16_t handle = sampHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return invalid_handle;

				SamplerDX12& samp = samplers[handle];
				samp.sampler = sampHeapAlloc.Alloc();
				if (invalid_handle == samp.sampler)
				{
					InternalResetSampler(samp);
					sampHandleAlloc.Free(handle);
					return invalid_handle;
				}

				samp.desc = {};
				samp.desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				samp.desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samp.desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samp.desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samp.desc.MaxLOD = D3D12_FLOAT32_MAX;
				samp.desc.MaxAnisotropy = 1;

				CD3DX12_CPU_DESCRIPTOR_HANDLE sampHandle(
					sampHeap->GetCPUDescriptorHandleForHeapStart(),
					samp.sampler,
					sampHeapInc
				);

				device->CreateSampler(&samp.desc, sampHandle);
				return handle;
			}

			void InternalDestroySampler(uint16_t handle)
			{
				if (!sampHandleAlloc.InUse(handle))
					return;

				SamplerDX12& samp = samplers[handle];
				InternalResetSampler(samp);
				sampHandleAlloc.Free(handle);
			}

			void InternalResetShader(ShaderDX12& shader)
			{
				delete[] shader.data;
				shader.data = nullptr;
				shader.size = 0;
			}

			uint16_t InternalCreateVertexShader(const void* data, size_t size)
			{
				uint16_t handle = vsHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return invalid_handle;

				ShaderDX12& vs = vertexShaders[handle];
				vs.data = new uint8_t[size];
				memcpy(vs.data, data, size);
				vs.size = size;

				return handle;
			}

			void InternalDestroyVertexShader(uint16_t handle)
			{
				if (!vsHandleAlloc.InUse(handle))
					return;

				ShaderDX12& vs = vertexShaders[handle];
				InternalResetShader(vs);
				vsHandleAlloc.Free(handle);
			}

			uint16_t InternalCreatePixelShader(const void* data, size_t size)
			{
				uint16_t handle = psHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return invalid_handle;

				ShaderDX12& ps = pixelShaders[handle];
				ps.data = new uint8_t[size];
				memcpy(ps.data, data, size);
				ps.size = size;

				return handle;
			}

			void InternalDestroyPixelShader(uint16_t handle)
			{
				if (!psHandleAlloc.InUse(handle))
					return;

				ShaderDX12& ps = pixelShaders[handle];
				InternalResetShader(ps);
				psHandleAlloc.Free(handle);
			}

#pragma endregion


			// interface implementation
#pragma region interface implementation

			// Pipeline States
			PipelineStateHandle CreatePipelineState(const PipelineState& state) override
			{
				return PipelineStateHandle{ invalid_handle };
			}

			void DestroyPipelineState(PipelineStateHandle handle) override
			{

			}


			// Buffers
			BufferHandle CreateBuffer(size_t size, uint32_t bindingFlags, bool dynamic = false) override
			{
				return BufferHandle{
					InternalCreateBuffer(bindingFlags, size)
				};
			}

			void DestroyBuffer(BufferHandle handle) override
			{
				InternalDestroyBuffer(handle.id);
			}

			void UpdateBuffer(BufferHandle handle, size_t size, const void* data, size_t stride = 0, PixelFormat format = FORMAT_AUTO) override
			{
				InternalUpdateBuffer(handle.id, size, data, stride, format);
			}


			// Textures
			TextureHandle CreateTexture(TextureType type, PixelFormat format, uint32_t bindFlags, uint32_t width, uint32_t height = 1, uint32_t depth = 1, uint32_t arraySize = 1, uint32_t mipLevels = 1, bool dynamic = false) override
			{
				return TextureHandle{
					InternalCreateTexture(
						type,
						format,
						bindFlags,
						width,
						height,
						depth,
						arraySize,
						mipLevels)
				};
			}

			TextureHandle CreateTexture(const wchar_t* filename) override
			{
				return TextureHandle{
					InternalCreateTexture(filename)
				};
			}

			void DestroyTexture(TextureHandle handle) override
			{
				InternalDestroyTexture(handle.id);
			}

			void UpdateTexture(TextureHandle handle, size_t pitch, const void* data) override
			{
				InternalUpdateTexture(handle.id, data, static_cast<uint32_t>(pitch));
			}


			void Clear(TextureHandle handle, float color[4]) override
			{
				if (handle.id == invalid_handle)
					handle.id = 0; // TODO current back buffer index
				if (!texHandleAlloc.InUse(handle.id))
					return;

				TextureDX12& tex = textures[handle.id];

				if (invalid_handle == tex.rtv)
					return;

				if (tex.state != D3D12_RESOURCE_STATE_RENDER_TARGET)
				{
					cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
						tex.texture,
						tex.state,
						D3D12_RESOURCE_STATE_RENDER_TARGET
					));

					tex.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
				}

				CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
					rtvHeap->GetCPUDescriptorHandleForHeapStart(),
					tex.rtv,
					rtvHeapInc);

				cmdList->ClearRenderTargetView(rtvHandle, color, 0, nullptr);
			}

			void ClearDepth(TextureHandle handle, float depth) override
			{
				if (handle.id == invalid_handle)
					handle.id = 0; // TODO default depth buffer index
				if (!texHandleAlloc.InUse(handle.id))
					return;

				TextureDX12& tex = textures[handle.id];

				if (invalid_handle == tex.dsv)
					return;

				if (tex.state != D3D12_RESOURCE_STATE_DEPTH_WRITE)
				{
					cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
						tex.texture,
						tex.state,
						D3D12_RESOURCE_STATE_DEPTH_WRITE
					));

					tex.state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				}

				CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
					dsvHeap->GetCPUDescriptorHandleForHeapStart(),
					tex.dsv,
					dsvHeapInc);

				cmdList->ClearDepthStencilView(
					dsvHandle,
					D3D12_CLEAR_FLAG_DEPTH,
					depth,
					0, 0, nullptr);
			}

			void ClearDepthStencil(TextureHandle handle, float depth, uint8_t stencil) override
			{
				if (handle.id == invalid_handle)
					handle.id = 0; // TODO default depth buffer index
				if (!texHandleAlloc.InUse(handle.id))
					return;

				TextureDX12& tex = textures[handle.id];

				if (invalid_handle == tex.dsv)
					return;

				if (tex.state != D3D12_RESOURCE_STATE_DEPTH_WRITE)
				{
					cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
						tex.texture,
						tex.state,
						D3D12_RESOURCE_STATE_DEPTH_WRITE
					));

					tex.state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				}

				CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
					dsvHeap->GetCPUDescriptorHandleForHeapStart(),
					tex.dsv,
					dsvHeapInc);

				cmdList->ClearDepthStencilView(
					dsvHandle,
					D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
					depth, stencil, 0, nullptr);
			}


			// Samplers
			SamplerHandle CreateSampler() override
			{
				return SamplerHandle{ InternalCreateSampler() };
			}

			void DestroySampler(SamplerHandle handle) override
			{
				InternalDestroySampler(handle.id);
			}


			// Shaders
			VertexShaderHandle CreateVertexShader(const void* bytecode, size_t size) override
			{
				return VertexShaderHandle{ InternalCreateVertexShader(bytecode, size) };
			}

			void DestroyVertexShader(VertexShaderHandle handle) override
			{
				InternalDestroyVertexShader(handle.id);
			}


			PixelShaderHandle CreatePixelShader(const void* bytecode, size_t size) override
			{
				return PixelShaderHandle{ InternalCreatePixelShader(bytecode, size) };
			}

			void DestroyPixelShader(PixelShaderHandle handle) override
			{
				InternalDestroyPixelShader(handle.id);
			}


			// Draw Functions
			void Draw(PipelineStateHandle stateHandle, const DrawCall& drawcall) override
			{

			}


			// Swap Chains
			// TODO, bind swap chains with render targets
			void Present() override
			{

			}


			// Clean up
			void Shutdown() override
			{
#define CLEAR_ARRAY(arr, count, alloc, func) \
				for (uint16_t handle = 0; handle < count; ++handle) \
					if (alloc.InUse(handle)) func(arr[handle]);

				CLEAR_ARRAY(pipelineStates, MaxPipelineStateCount, psoHandleAlloc, InternalResetPipelineState);
				CLEAR_ARRAY(buffers, MaxBufferCount, bufHandleAlloc, InternalResetBuffer);
				CLEAR_ARRAY(textures, MaxTextureCount, texHandleAlloc, InternalResetTexture);
				CLEAR_ARRAY(samplers, MaxSamplerCount, sampHandleAlloc, InternalResetSampler);
				CLEAR_ARRAY(vertexShaders, MaxVertexShaderCount, vsHandleAlloc, InternalResetShader);
				CLEAR_ARRAY(pixelShaders, MaxPixelShaderCount, psHandleAlloc, InternalResetShader);

#undef CLEAR_ARRAY

				rtvHeap->Release();
				dsvHeap->Release();
				srvHeap->Release();

				fence->Release();

				cmdList->Release();
				cmdAlloc->Release();

				swapChain->Release();
				cmdQueue->Release();
				device->Release();
			}

#pragma endregion
			// interface end

		};

		GraphicsAPI* InitGraphicsAPIDX12(void * windowHandle)
		{
			GraphicsAPIDX12* api = new GraphicsAPIDX12();
			return nullptr;
		}
	}
}

