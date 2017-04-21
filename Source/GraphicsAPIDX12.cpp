#include "GraphicsAPIDX12.h"

#include <Windows.h>
#include <dxgi1_5.h>
#include <d3d12.h>
#include <d3dx12.h>

#include <WICTextureLoader12.h>
#include <DDSTextureLoader12.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

#include <vector>

#include "UploadHeapDX12.h"

#define RELEASE(x) if (nullptr != (x)) { (x)->Release(); (x) = nullptr; }
#define FREE_HANDLE(h, a) if ((a).InUse(h)) { (a).Free(h); (h) = invalid_handle; }
#define CE(x, e) if (S_OK != (x)) return (e);
#define CHECKED(x) if (S_OK != (x)) return -1;

#define USING_SYNC_UPLOAD_HEAP 1

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

		D3D12_PRIMITIVE_TOPOLOGY TopologyTable[] =
		{
			D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
			D3D_PRIMITIVE_TOPOLOGY_LINELIST,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
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

		D3D12_SHADER_VISIBILITY ShaderVisibilityTable[] =
		{
			D3D12_SHADER_VISIBILITY_ALL,
			D3D12_SHADER_VISIBILITY_VERTEX,
			D3D12_SHADER_VISIBILITY_PIXEL,
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

		struct BindingLayoutDX12
		{
			ID3D12RootSignature*		rootSig;

			uint32_t					entryCount;
			BindingLayout				layout;
			uint32_t					offsets[MaxBindingLayoutEntry];
			uint32_t					slotId[MaxBindingLayoutEntry];

			BindingLayoutDX12()
				:
				rootSig(nullptr),
				entryCount(0),
				layout{},
				offsets{}
			{}
		};

		struct PipelineStateDX12
		{
			ID3D12PipelineState*		state;
			BindingLayoutHandle			bindingLayout;
			D3D12_PRIMITIVE_TOPOLOGY	topology;

			PipelineStateDX12()
				:
				state(nullptr),
				bindingLayout{ invalid_handle }
			{}
		};

		struct BufferDX12
		{
			ID3D12Resource*				buffer;
			//uint16_t					cbv;
			//uint16_t					srv;
			D3D12_RESOURCE_STATES		state;

			uint32_t					bindFlags;
			uint32_t					size;
			uint32_t					stride;
			PixelFormat					format;

			BufferDX12()
				:
				buffer(nullptr),
				//srv(invalid_handle),
#if defined(USING_SYNC_UPLOAD_HEAP)
				state(D3D12_RESOURCE_STATE_COPY_DEST),
#else
				state(D3D12_RESOURCE_STATE_COMMON),
#endif
				size(0),
				stride(0),
				format(FORMAT_AUTO)
			{}
		};

		struct TextureDX12
		{
			ID3D12Resource*				texture;
			//uint16_t					srv;
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
				//srv(invalid_handle),
				rtv(invalid_handle),
				dsv(invalid_handle),
#if defined(USING_SYNC_UPLOAD_HEAP)
				state(D3D12_RESOURCE_STATE_COPY_DEST),
#else
				state(D3D12_RESOURCE_STATE_COMMON),
#endif
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
			//uint32_t				sampler;
			D3D12_SAMPLER_DESC		desc;

			SamplerDX12()
				:
				//sampler(invalid_handle),
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
			//HandleAlloc<SRVHeapSize>		srvHeapAlloc;
			HandleAlloc<SamplerHeapSize>	sampHeapAlloc;

			ID3D12DescriptorHeap*		rtvHeap;
			ID3D12DescriptorHeap*		dsvHeap;
			ID3D12DescriptorHeap*		srvHeap;
			ID3D12DescriptorHeap*		sampHeap;

			UINT						rtvHeapInc;
			UINT						dsvHeapInc;
			UINT						srvHeapInc;
			UINT						sampHeapInc;

			UINT						srvHeapIndex;
			UINT						sampHeapIndex;

			BindingLayoutDX12			bindingLayouts[MaxBindingLayoutCount];
			PipelineStateDX12			pipelineStates[MaxPipelineStateCount];
			BufferDX12					buffers[MaxBufferCount];
			TextureDX12					textures[MaxTextureCount];
			SamplerDX12					samplers[MaxSamplerCount];
			ShaderDX12					vertexShaders[MaxVertexShaderCount];
			ShaderDX12					pixelShaders[MaxPixelShaderCount];

			BindingLayoutHandle			currentBindingLayout;
			PipelineStateHandle			currentPipelineState;

#if defined(USING_SYNC_UPLOAD_HEAP)
			UploadHeapSyncDX12			uploadHeap;
#else
			UploadHeapDX12				uploadHeap;
#endif

			int Init(void* windowHandle)
			{
				hWnd = reinterpret_cast<HWND>(windowHandle);

				int result = 0;

				if (0 != (result = InitDirect3D()))
					return result;

				if (0 != (result = InitRenderTargets()))
					return result;

				InitPipelineStates();

#if defined(USING_SYNC_UPLOAD_HEAP)
				if (!uploadHeap.Init(device, cmdList))
#else
				if (!uploadHeap.Init(device))
#endif
				{
					return -1;
				}

				srvHeapIndex = 0;
				sampHeapIndex = 0;

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
					RECT rect = {};
					GetClientRect(hWnd, &rect);
					width = rect.right - rect.left;
					height = rect.bottom - rect.top;
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
					desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
					CHECKED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvHeap)));
					srvHeapInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					//srvHeapAlloc.Reset();
				}

				{
					D3D12_DESCRIPTOR_HEAP_DESC desc = {};
					desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
					desc.NumDescriptors = SamplerHeapSize;
					desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
					CHECKED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&sampHeap)));
					sampHeapInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
					sampHeapAlloc.Reset();
				}

				CHECKED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)));

				CHECKED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList)));

				//cmdList->Close();
				ID3D12DescriptorHeap* heaps[] = { srvHeap, sampHeap };
				cmdList->SetDescriptorHeaps(2, heaps);

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
					uint16_t handle = InternalCreateTexture(res, BINDING_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
						/*i == backBufferIndex ? D3D12_RESOURCE_STATE_PRESENT :
						D3D12_RESOURCE_STATE_RENDER_TARGET);*/
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
				currentBindingLayout.id = invalid_handle;
				currentPipelineState.id = invalid_handle;
			}


			inline void TransistResource(ID3D12Resource* res, D3D12_RESOURCE_STATES& current, D3D12_RESOURCE_STATES dest)
			{
				if (dest != current)
				{
					cmdList->ResourceBarrier(
						1,
						&CD3DX12_RESOURCE_BARRIER::Transition(
							res,
							current,
							dest
						)
					);
					current = dest;
				}
			}

			bool SetPipelineState(PipelineStateDX12& state)
			{
				cmdList->SetPipelineState(state.state);

				if (state.bindingLayout.id != currentBindingLayout.id)
				{
					if (!blHandleAlloc.InUse(state.bindingLayout.id))
						return false;

					BindingLayoutDX12& layout = bindingLayouts[state.bindingLayout.id];
					cmdList->SetGraphicsRootSignature(layout.rootSig);
					currentBindingLayout = state.bindingLayout;
				}

				cmdList->IASetPrimitiveTopology(state.topology);

				return true;
			}

			bool BindResources(const DrawCall& drawcall)
			{
				// Input Assembly
				if (drawcall.VertexBufferCount > 0)
				{
					D3D12_VERTEX_BUFFER_VIEW vbvs[MaxVertexBufferBindingSlot];

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

						TransistResource(buf.buffer, buf.state, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
						vbvs[i].BufferLocation = buf.buffer->GetGPUVirtualAddress();
						vbvs[i].SizeInBytes = buf.size;
						vbvs[i].StrideInBytes = buf.stride;
					}

					cmdList->IASetVertexBuffers(0, drawcall.VertexBufferCount, vbvs);
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

					TransistResource(buf.buffer, buf.state, D3D12_RESOURCE_STATE_INDEX_BUFFER);
					D3D12_INDEX_BUFFER_VIEW ibv = {};
					ibv.BufferLocation = buf.buffer->GetGPUVirtualAddress();
					ibv.SizeInBytes = buf.size;
					ibv.Format = (buf.stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

					cmdList->IASetIndexBuffer(&ibv);
				}
				/////

				// Rasterizer
				{
					D3D12_VIEWPORT vp =
					{
						drawcall.Viewport.X,
						drawcall.Viewport.Y,
						drawcall.Viewport.Width,
						drawcall.Viewport.Height,
						drawcall.Viewport.ZMin,
						drawcall.Viewport.ZMax
					};

					cmdList->RSSetViewports(1, &vp);

					D3D12_RECT rect = { 0, 0, width, height };
					cmdList->RSSetScissorRects(1, &rect);
				}

				{
					uint16_t handle = currentBindingLayout.id;
					if (!blHandleAlloc.InUse(handle))
					{
						return false;
					}

					BindingLayoutDX12& layout = bindingLayouts[handle];
					const uint8_t* pData = reinterpret_cast<const uint8_t*>(drawcall.ResourceBindingData);

					for (size_t i = 0; i < layout.entryCount; i++)
					{
						auto& entry = layout.layout.table[i];
						switch (entry.Type)
						{
						case BINDING_SLOT_TYPE_CONSTANT:
							cmdList->SetGraphicsRoot32BitConstants(layout.slotId[i], entry.Count, pData + layout.offsets[i], 0);
							break;
						case BINDING_SLOT_TYPE_CBV:
							(void*)0;
							{
								uint16_t handle = static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>((pData + layout.offsets[i])));


								if (invalid_handle != handle)
								{
									if (!bufHandleAlloc.InUse(handle))
										return false;
									BufferDX12& buf = buffers[handle];
									if ((buf.bindFlags & BINDING_CONSTANT_BUFFER) == 0)
										return false;

									TransistResource(buf.buffer, buf.state, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
									cmdList->SetGraphicsRootConstantBufferView(layout.slotId[i], buf.buffer->GetGPUVirtualAddress());
								}

							}
							break;
						case BINDING_SLOT_TYPE_SRV:
							(void*)0;
							{
								uint32_t offset = layout.offsets[i];
								uint32_t data = *reinterpret_cast<const uint32_t*>((pData + offset));
								bool isBuffer = (data & 0x80000000u) != 0u;
								uint16_t handle = static_cast<uint16_t>(data & 0xffff);

								if (invalid_handle != handle)
								{
									if (isBuffer)
									{
										if (!bufHandleAlloc.InUse(handle))
											return false;
										BufferDX12& buf = buffers[handle];
										if ((buf.bindFlags & BINDING_SHADER_RESOURCE) == 0)
											return false;

										TransistResource(buf.buffer, buf.state, 
											entry.ShaderVisibility == SHADER_VISIBILITY_PIXEL ?
											D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE :
											D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
										);

										cmdList->SetGraphicsRootShaderResourceView(layout.slotId[i], buf.buffer->GetGPUVirtualAddress());
									}
									else
									{
										/*if (!texHandleAlloc.InUse(handle))
											return false;
										TextureDX12& tex = textures[handle];
										cmdList->SetGraphicsRootShaderResourceView(layout.slotId[i], tex.texture->g);*/
										return false;
									}
								}
							}
							break;
						case BINDING_SLOT_TYPE_TABLE:
							(void*)0;
							{
								bool isSamplerTable = false;
								bool isCBVSRVTable = false;

								uint32_t handleIdx = 0;
								for (uint32_t iRange = 0; iRange < entry.Count; iRange++)
								{
									auto& subEntry = layout.layout.table[i + iRange + 1];


									if (subEntry.Type == BINDING_SLOT_TYPE_SAMPLER)
									{
										if (isCBVSRVTable)
											return false;
										isSamplerTable = true;

										for (uint32_t iRangeEntry = 0; iRangeEntry < subEntry.Count; iRangeEntry++)
										{
											uint32_t offset = layout.offsets[i + iRange + 1] + 4u * iRangeEntry;
											uint16_t handle = static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>((pData + offset)));

											CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(sampHeap->GetCPUDescriptorHandleForHeapStart(), sampHeapIndex + handleIdx, sampHeapInc);
											handleIdx++;

											if (invalid_handle != handle)
											{
												if (!sampHandleAlloc.InUse(handle))
													return false;

												device->CreateSampler(&(samplers[handle].desc), cpuHandle);
											}
											else
											{
												D3D12_SAMPLER_DESC desc = {};

												desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
												desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
												desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
												desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
												desc.MaxLOD = D3D12_FLOAT32_MAX;
												desc.MaxAnisotropy = 1;

												device->CreateSampler(&desc, cpuHandle);
											}


										} // for loop - iRangeEntry
									}
									else
									{
										if (isSamplerTable)
											return false;
										isCBVSRVTable = true;

										for (uint32_t iRangeEntry = 0; iRangeEntry < subEntry.Count; iRangeEntry++)
										{
											uint32_t offset = layout.offsets[i + iRange + 1] + 4u * iRangeEntry;

											uint32_t data = *reinterpret_cast<const uint32_t*>((pData + offset));
											bool isBuffer = (data & 0x80000000u) != 0u;
											uint16_t handle = static_cast<uint16_t>(data & 0xffff);

											CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(srvHeap->GetCPUDescriptorHandleForHeapStart(), srvHeapIndex + handleIdx, srvHeapInc);
											handleIdx++;

											if (subEntry.Type == BINDING_SLOT_TYPE_CBV)
											{
												if (invalid_handle != handle)
												{
													if (!bufHandleAlloc.InUse(handle))
														return false;
													BufferDX12& buf = buffers[handle];
													if ((buf.bindFlags & BINDING_CONSTANT_BUFFER) == 0)
														return false;


													TransistResource(buf.buffer, buf.state, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

													D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
													desc.BufferLocation = buf.buffer->GetGPUVirtualAddress();
													desc.SizeInBytes = static_cast<UINT>(buf.size);

													device->CreateConstantBufferView(&desc, cpuHandle);
												}
												else
												{
													device->CreateConstantBufferView(nullptr, cpuHandle);
												}
											}
											else if (subEntry.Type == BINDING_SLOT_TYPE_SRV)
											{
												if (isBuffer)
												{
													if (invalid_handle != handle)
													{
														if (!bufHandleAlloc.InUse(handle))
															return false;
														BufferDX12& buf = buffers[handle];
														if ((buf.bindFlags & BINDING_SHADER_RESOURCE) == 0)
															return false;

														TransistResource(buf.buffer, buf.state,
															entry.ShaderVisibility == SHADER_VISIBILITY_PIXEL ?
															D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE :
															D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
														);

														D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};

														desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
														desc.Format = PixelFormatTable[buf.format];
														desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
														desc.Buffer.FirstElement = 0;
														desc.Buffer.NumElements = buf.size / buf.stride;
														desc.Buffer.StructureByteStride = buf.stride;
														desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

														device->CreateShaderResourceView(buf.buffer, &desc, cpuHandle);
													}
													else
													{
														D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
														desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
														device->CreateShaderResourceView(nullptr, &desc, cpuHandle);
													}
												}
												else
												{
													if (invalid_handle != handle)
													{
														if (!texHandleAlloc.InUse(handle))
															return false;
														TextureDX12& tex = textures[handle];

														TransistResource(tex.texture, tex.state,
															entry.ShaderVisibility == SHADER_VISIBILITY_PIXEL ?
															D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE :
															D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
														);

														device->CreateShaderResourceView(tex.texture, nullptr, cpuHandle);
													}
													else
													{
														D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
														desc.ViewDimension = D3D12_SRV_DIMENSION_UNKNOWN;
														device->CreateShaderResourceView(nullptr, &desc, cpuHandle);
													}
												}
											}

										} // for loop - iRangeEntry

									} // if - sampler or cbvsrv

								} // for loop - iRange

								if (isSamplerTable == isCBVSRVTable)
									return false;

								if (isSamplerTable)
								{
									CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(sampHeap->GetGPUDescriptorHandleForHeapStart(), sampHeapIndex, sampHeapInc);
									sampHeapIndex += handleIdx;

									cmdList->SetGraphicsRootDescriptorTable(layout.slotId[i], gpuHandle);
								}
								else
								{
									CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(srvHeap->GetGPUDescriptorHandleForHeapStart(), srvHeapIndex, srvHeapInc);
									srvHeapIndex += handleIdx;

									cmdList->SetGraphicsRootDescriptorTable(layout.slotId[i], gpuHandle);
								}

								i += entry.Count;
							}
							break;
						default:
							break;
						}
					}

				}
				//cmdList->SetDescriptorHeaps
				// Render Target
				if (drawcall.RenderTargetCount > 0 || drawcall.HasDepthStencil)
				{
					D3D12_CPU_DESCRIPTOR_HANDLE rtvs[MaxRenderTargetBindingSlot];
					D3D12_CPU_DESCRIPTOR_HANDLE dsv;

					for (size_t i = 0; i < drawcall.RenderTargetCount; ++i)
					{
						uint16_t handle = drawcall.RenderTargets[i].id;
#if _DEBUG
						if (!texHandleAlloc.InUse(handle))
							return false;
#endif
						auto& tex = textures[handle];
						if (!rtvHeapAlloc.InUse(tex.rtv))
							return false;

						TransistResource(tex.texture, tex.state, D3D12_RESOURCE_STATE_RENDER_TARGET);

						rtvs[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), tex.rtv, rtvHeapInc);
					}

					if (drawcall.HasDepthStencil)
					{
						uint16_t handle = drawcall.DepthStencil.id;
#if _DEBUG
						if (!texHandleAlloc.InUse(handle))
							return false;
#endif
						auto& tex = textures[handle];
						if (!dsvHeapAlloc.InUse(tex.dsv))
							return false;

						TransistResource(tex.texture, tex.state, D3D12_RESOURCE_STATE_DEPTH_WRITE);

						dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvHeap->GetCPUDescriptorHandleForHeapStart(), tex.dsv, dsvHeapInc);
					}

					cmdList->OMSetRenderTargets(drawcall.RenderTargetCount, rtvs, FALSE, &dsv);
				}
				else
				{
					TransistResource(textures[backBufferIndex].texture, textures[backBufferIndex].state, D3D12_RESOURCE_STATE_RENDER_TARGET);
					TransistResource(textures[2].texture, textures[2].state, D3D12_RESOURCE_STATE_DEPTH_WRITE);

					D3D12_CPU_DESCRIPTOR_HANDLE rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), textures[backBufferIndex].rtv, rtvHeapInc);
					D3D12_CPU_DESCRIPTOR_HANDLE dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvHeap->GetCPUDescriptorHandleForHeapStart(), textures[2].dsv, dsvHeapInc);

					cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
				}

				return true;
			}

#pragma region resource creation

			void InternalResetBindingLayout(BindingLayoutDX12& layout)
			{
				RELEASE(layout.rootSig);
				layout.entryCount = 0;
				layout.layout = {};
			}

			uint16_t InternalCreateBindingLayout(const BindingLayout& layoutDesc)
			{
				uint16_t handle = blHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return invalid_handle;

				BindingLayoutDX12& layout = bindingLayouts[handle];

				CD3DX12_DESCRIPTOR_RANGE ranges[MaxBindingLayoutEntry];
				CD3DX12_ROOT_PARAMETER params[MaxBindingLayoutEntry];
				uint32_t rangeIdx = 0;
				uint32_t paramIdx = 0;

				{
					uint32_t offset = 0, i = 0;

					for (; i < MaxBindingLayoutEntry; ++i)
					{
						auto& entry = layoutDesc.table[i];
						if (entry.Type == BINDING_SLOT_TYPE_NONE)
						{
							layout.entryCount = i;
							break;
						}

						layout.offsets[i] = offset;
						layout.slotId[i] = paramIdx;

						uint32_t count = (
							entry.Type == BINDING_SLOT_TYPE_TABLE ?
							0 : (entry.Count == 0 ? 1 : entry.Count));
						uint32_t size = 4 * count;
						offset += size;

						auto& par = params[paramIdx];

						if (entry.Type == BINDING_SLOT_TYPE_CONSTANT)
						{
							par.InitAsConstants(
								entry.Count,
								entry.Register,
								entry.Space,
								ShaderVisibilityTable[entry.ShaderVisibility]
							);
						}
						else if (entry.Type == BINDING_SLOT_TYPE_CBV)
						{
							par.InitAsConstantBufferView(
								entry.Register,
								entry.Space,
								ShaderVisibilityTable[entry.ShaderVisibility]
							);
						}
						else if (entry.Type == BINDING_SLOT_TYPE_SRV)
						{
							par.InitAsShaderResourceView(
								entry.Register,
								entry.Space,
								ShaderVisibilityTable[entry.ShaderVisibility]
							);
						}
						else if (entry.Type == BINDING_SLOT_TYPE_TABLE)
						{
							for (uint32_t j = 0; j < entry.Count; j++)
							{
								auto& range = ranges[rangeIdx + j];
								auto& subEntry = layoutDesc.table[i + j + 1];

								layout.offsets[i + j + 1] = offset;
								offset += subEntry.Count * 4u;
								//layout.slotId[i + j + 1] = paramIdx;

								if (subEntry.Type == BINDING_SLOT_TYPE_CBV)
								{
									range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
										subEntry.Count,
										subEntry.Register,
										subEntry.Space);
								}
								else if (subEntry.Type == BINDING_SLOT_TYPE_SRV)
								{
									range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
										subEntry.Count,
										subEntry.Register,
										subEntry.Space);
								}
								else if (subEntry.Type == BINDING_SLOT_TYPE_SAMPLER)
								{
									range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
										subEntry.Count,
										subEntry.Register,
										subEntry.Space);
								}
								else
								{
									InternalResetBindingLayout(layout);
									blHandleAlloc.Free(handle);
									return invalid_handle;
								}
							}

							par.InitAsDescriptorTable(
								entry.Count,
								&ranges[rangeIdx],
								ShaderVisibilityTable[entry.ShaderVisibility]
							);

							rangeIdx += entry.Count;
							i += entry.Count;
						}

						paramIdx++;
					}

					if (i == MaxBindingLayoutEntry)
					{
						layout.entryCount = MaxBindingLayoutEntry;
					}
				}


				CD3DX12_ROOT_SIGNATURE_DESC desc;
				desc.Init(paramIdx, params, 0U, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ID3DBlob* blob = nullptr;
				if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr)) ||
					FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&layout.rootSig))))
				{
					InternalResetBindingLayout(layout);
					blHandleAlloc.Free(handle);
					return invalid_handle;
				}

				layout.layout = layoutDesc;

				return handle;
			}

			void InternalDestroyBindingLayout(uint16_t handle)
			{
				if (!blHandleAlloc.InUse(handle))
					return;

				BindingLayoutDX12& layout = bindingLayouts[handle];
				InternalResetBindingLayout(layout);
				blHandleAlloc.Free(handle);
			}

			void InternalResetPipelineState(PipelineStateDX12& state)
			{
				RELEASE(state.state);
				state.bindingLayout.id = invalid_handle;
			}

			uint16_t InternalCreatePipelineState(const PipelineState& stateDesc)
			{
				uint16_t handle = psoHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return invalid_handle;

				PipelineStateDX12& state = pipelineStates[handle];

				D3D12_INPUT_ELEMENT_DESC elements[MaxVertexInputElement];
				D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

				{
					if (!blHandleAlloc.InUse(stateDesc.BindingLayout.id))
					{
						InternalResetPipelineState(state);
						psoHandleAlloc.Free(handle);
						return invalid_handle;
					}

					state.bindingLayout = stateDesc.BindingLayout;
					desc.pRootSignature = bindingLayouts[stateDesc.BindingLayout.id].rootSig;

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

						desc.InputLayout = { elements, stateDesc.VertexLayout.ElementCount };
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
					state.topology = TopologyTable[stateDesc.PrimitiveType];
					desc.NumRenderTargets = stateDesc.RenderTargetCount;
					for (size_t i = 0; i < desc.NumRenderTargets; i++)
					{
						desc.RTVFormats[i] = PixelFormatTable[stateDesc.RenderTargetFormats[i]];
					}
					desc.DSVFormat = PixelFormatTable[stateDesc.DepthStencilFormat];
					desc.SampleDesc.Count = 1;

					{
						desc.RasterizerState.CullMode = static_cast<D3D12_CULL_MODE>(stateDesc.CullMode + 1);

						desc.DepthStencilState.DepthEnable = stateDesc.DepthEnable;
						desc.DepthStencilState.DepthWriteMask = static_cast<D3D12_DEPTH_WRITE_MASK>(stateDesc.DepthWrite);
						desc.DepthStencilState.DepthFunc = static_cast<D3D12_COMPARISON_FUNC>(stateDesc.DepthFunc + 1);
					}
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
				//FREE_HANDLE(buf.cbv, srvHeapAlloc);
				//FREE_HANDLE(buf.srv, srvHeapAlloc);
			}

			uint16_t InternalCreateBuffer(uint32_t bindFlags, size_t size)
			{
				uint16_t handle = bufHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return handle;

				BufferDX12& buf = buffers[handle];

				buf.bindFlags = bindFlags;

				D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE;
				D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;// D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

				if (bindFlags & BINDING_CONSTANT_BUFFER)
				{
					size = (static_cast<UINT>(size) + 255) & ~255;
				}

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


				/*if (bindFlags & BINDING_CONSTANT_BUFFER)
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
					desc.SizeInBytes = static_cast<UINT>(size);

					device->CreateConstantBufferView(&desc, cbvHandle);
				}*/

				/*if (bindFlags & BINDING_SHADER_RESOURCE)
				{
					buf.srv = srvHeapAlloc.Alloc();
					if (invalid_handle == buf.srv)
					{
						InternalResetBuffer(buf);
						bufHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}*/

				buf.size = static_cast<uint32_t>(size);
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
				//if (invalid_handle != buf.srv)
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
					buf.stride = stride;
					buf.format = format;
				}
				/*if (stride != buf.stride || format != buf.format)
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
			}*/

#if defined(USING_SYNC_UPLOAD_HEAP)
				TransistResource(buf.buffer, buf.state, D3D12_RESOURCE_STATE_COPY_DEST);
#else
				TransistResource(buf.buffer, buf.state, D3D12_RESOURCE_STATE_COMMON);
#endif

				uploadHeap.UploadResource(buf.buffer, size, data, 0);
			}

			void InternalResetTexture(TextureDX12& tex)
			{
				RELEASE(tex.texture);
				//FREE_HANDLE(tex.srv, srvHeapAlloc);
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

				/*if ((bindFlags & BINDING_SHADER_RESOURCE))
				{
					tex.srv = srvHeapAlloc.Alloc();
					if (invalid_handle == tex.srv)
					{
						InternalResetTexture(tex);
						texHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}*/
				if (bindFlags & BINDING_RENDER_TARGET)
				{
					tex.rtv = rtvHeapAlloc.Alloc();
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
					tex.width = static_cast<uint32_t>(desc.Width);
					tex.height = static_cast<uint32_t>(desc.Height);
					tex.depth = 1u;
					tex.arraySize = desc.DepthOrArraySize;
					tex.mipLevels = desc.MipLevels;
					break;
				case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
					// TODO
					tex.type = (desc.DepthOrArraySize == 6 ? TEXTURE_CUBE : TEXTURE_2D);
					tex.width = static_cast<uint32_t>(desc.Width);
					tex.height = static_cast<uint32_t>(desc.Height);
					tex.depth = 1;
					tex.arraySize = desc.DepthOrArraySize;
					tex.mipLevels = desc.MipLevels;
					break;
				case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
					tex.type = TEXTURE_3D;
					tex.width = static_cast<uint32_t>(desc.Width);
					tex.height = static_cast<uint32_t>(desc.Height);
					tex.depth = desc.DepthOrArraySize;
					tex.arraySize = 1;
					tex.mipLevels = desc.MipLevels;
					break;
				default:
					InternalResetTexture(tex);
					texHandleAlloc.Free(handle);
					return invalid_handle;
				}

				/*if (invalid_handle != tex.srv)
				{
					CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvHeap->GetCPUDescriptorHandleForHeapStart(), tex.srv, srvHeapInc);
					device->CreateShaderResourceView(tex.texture, nullptr, srvHandle);
				}*/

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
					//tex.srv = invalid_handle;
					resFlag |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
				}
				/*else
				{
					tex.srv = srvHeapAlloc.Alloc();
					if (invalid_handle == tex.srv)
					{
						InternalResetTexture(tex);
						texHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}*/
				if (bindFlags & BINDING_RENDER_TARGET)
				{
					resFlag |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
					tex.rtv = rtvHeapAlloc.Alloc();
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

#if defined(USING_SYNC_UPLOAD_HEAP)
				D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COPY_DEST;
#else
				D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
#endif

				if (FAILED(device->CreateCommittedResource(&heapProp,
					D3D12_HEAP_FLAG_DENY_BUFFERS,
					&resDesc,
					initialState,
					nullptr,
					IID_PPV_ARGS(&tex.texture))))
				{
					InternalResetTexture(tex);
					texHandleAlloc.Free(handle);
					return invalid_handle;
				}

				/*if (invalid_handle != tex.srv)
				{
					CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvHeap->GetCPUDescriptorHandleForHeapStart(), tex.srv, srvHeapInc);
					device->CreateShaderResourceView(tex.texture, nullptr, srvHandle);
				}*/

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

				tex.state = initialState;

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
				std::vector<D3D12_SUBRESOURCE_DATA> data;
				std::unique_ptr<uint8_t[]> ptr;

				size_t fnLen = wcslen(filename);
				if (filename[fnLen - 4] == L'.' &&
					filename[fnLen - 3] == L'd' &&
					filename[fnLen - 2] == L'd' &&
					filename[fnLen - 1] == L's')
				{
					if (FAILED(DirectX::LoadDDSTextureFromFile(device, filename, &res, ptr, data)))
					{
						return invalid_handle;
					}
				}
				else
				{
					data.resize(1);

					if (FAILED(DirectX::LoadWICTextureFromFile(
						device,
						filename,
						&res,
						ptr,
						data[0])))
					{
						return invalid_handle;
					}
				}


#if defined(USING_SYNC_UPLOAD_HEAP)
				uint16_t handle = InternalCreateTexture(res, BINDING_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
#else

				uint16_t handle = InternalCreateTexture(res, BINDING_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);

				if (invalid_handle == handle)
				{
					return invalid_handle;
				}


				cmdList->ResourceBarrier(1,
					&CD3DX12_RESOURCE_BARRIER::Transition(res, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON)
				);
#endif

				uploadHeap.UploadResource(res, 0, ptr.get(), static_cast<uint32_t>(data[0].RowPitch));

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

#if defined(USING_SYNC_UPLOAD_HEAP)
				TransistResource(tex.texture, tex.state, D3D12_RESOURCE_STATE_COPY_DEST);
#else
				TransistResource(tex.texture, tex.state, D3D12_RESOURCE_STATE_COMMON);
#endif

				uploadHeap.UploadResource(tex.texture, 0 /* auto */, data, rowPitch);
			}

			void InternalResetSampler(SamplerDX12& samp)
			{
				//FREE_HANDLE(samp.sampler, sampHeapAlloc);
				samp.desc = {};
			}

			uint16_t InternalCreateSampler()
			{
				uint16_t handle = sampHandleAlloc.Alloc();
				if (invalid_handle == handle)
					return invalid_handle;

				SamplerDX12& samp = samplers[handle];
				/*samp.sampler = sampHeapAlloc.Alloc();
				if (invalid_handle == samp.sampler)
				{
					InternalResetSampler(samp);
					sampHandleAlloc.Free(handle);
					return invalid_handle;
				}*/

				samp.desc = {};
				samp.desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				samp.desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samp.desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samp.desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samp.desc.MaxLOD = D3D12_FLOAT32_MAX;
				samp.desc.MaxAnisotropy = 1;

				/*CD3DX12_CPU_DESCRIPTOR_HANDLE sampHandle(
					sampHeap->GetCPUDescriptorHandleForHeapStart(),
					samp.sampler,
					sampHeapInc
				);

				device->CreateSampler(&samp.desc, sampHandle);*/
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

			// Binding Layout
			BindingLayoutHandle CreateBindingLayout(const BindingLayout& layout) override
			{
				return BindingLayoutHandle{ InternalCreateBindingLayout(layout) };
			}

			void DestroyBindingLayout(BindingLayoutHandle handle) override
			{
				InternalDestroyBindingLayout(handle.id);
			}

			// Pipeline States
			PipelineStateHandle CreatePipelineState(const PipelineState& state) override
			{
				return PipelineStateHandle{ InternalCreatePipelineState(state) };
			}

			void DestroyPipelineState(PipelineStateHandle handle) override
			{
				InternalDestroyPipelineState(handle.id);
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
				InternalUpdateBuffer(handle.id, static_cast<uint32_t>(size), data, static_cast<uint32_t>(stride), format);
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
					handle.id = backBufferIndex;
				if (!texHandleAlloc.InUse(handle.id))
					return;

				TextureDX12& tex = textures[handle.id];

				if (invalid_handle == tex.rtv)
					return;

				TransistResource(tex.texture, tex.state, D3D12_RESOURCE_STATE_RENDER_TARGET);

				CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
					rtvHeap->GetCPUDescriptorHandleForHeapStart(),
					tex.rtv,
					rtvHeapInc);

				cmdList->ClearRenderTargetView(rtvHandle, color, 0, nullptr);
			}

			void ClearDepth(TextureHandle handle, float depth) override
			{
				if (handle.id == invalid_handle)
					handle.id = 2; // TODO default depth buffer index
				if (!texHandleAlloc.InUse(handle.id))
					return;

				TextureDX12& tex = textures[handle.id];

				if (invalid_handle == tex.dsv)
					return;

				TransistResource(tex.texture, tex.state, D3D12_RESOURCE_STATE_DEPTH_WRITE);

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
					handle.id = 2; // TODO default depth buffer index
				if (!texHandleAlloc.InUse(handle.id))
					return;

				TextureDX12& tex = textures[handle.id];

				if (invalid_handle == tex.dsv)
					return;

				TransistResource(tex.texture, tex.state, D3D12_RESOURCE_STATE_DEPTH_WRITE);

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
				if (stateHandle.id != currentPipelineState.id)
				{
					if (!psoHandleAlloc.InUse(stateHandle.id))
					{
						return; // TODO error !
					}

					PipelineStateDX12& state = pipelineStates[stateHandle.id];
					SetPipelineState(state);

					currentPipelineState = stateHandle;
				}

				BindResources(drawcall);
				if (drawcall.HasIndexBuffer)
				{
					cmdList->DrawIndexedInstanced(drawcall.ElementCount, 1, 0, 0, 0);
				}
				else
				{
					cmdList->DrawInstanced(drawcall.ElementCount, 1, 0, 0);
				}
			}


			// Swap Chains
			void Present() override
			{
				TextureDX12& tex = textures[backBufferIndex];
				TransistResource(tex.texture, tex.state, D3D12_RESOURCE_STATE_PRESENT);

				cmdList->Close();

				ID3D12CommandList* cmdLists[] = { cmdList };

				cmdQueue->ExecuteCommandLists(1, cmdLists);
				cmdQueue->Signal(fence, 1Ui64);

				swapChain->Present(0, 0);

				if (fence->GetCompletedValue() != 1Ui64)
				{
					fence->SetEventOnCompletion(1Ui64, fenceEvent);
					WaitForSingleObject(fenceEvent, INFINITE);
				}
				fence->Signal(0Ui64);

				uploadHeap.Clear();

				backBufferIndex = swapChain->GetCurrentBackBufferIndex();

				// clean descriptor heap  -  only safe after gpu has done using them
				srvHeapIndex = 0;
				sampHeapIndex = 0;

				cmdAlloc->Reset();
				cmdList->Reset(cmdAlloc, nullptr);
				ID3D12DescriptorHeap* heaps[] = { srvHeap, sampHeap };
				cmdList->SetDescriptorHeaps(2, heaps);

				currentBindingLayout.id = invalid_handle;
				currentPipelineState.id = invalid_handle;
			}


			// Clean up
			void Shutdown() override
			{
#define CLEAR_ARRAY(arr, count, alloc, func) \
				for (uint16_t handle = 0; handle < count; ++handle) \
					if (alloc.InUse(handle)) func(arr[handle]);

				CLEAR_ARRAY(bindingLayouts, MaxBindingLayoutCount, blHandleAlloc, InternalResetBindingLayout);
				CLEAR_ARRAY(pipelineStates, MaxPipelineStateCount, psoHandleAlloc, InternalResetPipelineState);
				CLEAR_ARRAY(buffers, MaxBufferCount, bufHandleAlloc, InternalResetBuffer);
				CLEAR_ARRAY(textures, MaxTextureCount, texHandleAlloc, InternalResetTexture);
				CLEAR_ARRAY(samplers, MaxSamplerCount, sampHandleAlloc, InternalResetSampler);
				CLEAR_ARRAY(vertexShaders, MaxVertexShaderCount, vsHandleAlloc, InternalResetShader);
				CLEAR_ARRAY(pixelShaders, MaxPixelShaderCount, psHandleAlloc, InternalResetShader);

#undef CLEAR_ARRAY

				CloseHandle(fenceEvent);

				uploadHeap.Clear();

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

		GraphicsAPI* InitGraphicsAPIDX12(void* windowHandle)
		{
			GraphicsAPIDX12* api = new GraphicsAPIDX12();
			if (0 != api->Init(windowHandle))
			{
				delete api;
				return nullptr;
			}
			return api;
		}
	}
}

