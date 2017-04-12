#include "GraphicsAPIDX12.h"

#include <Windows.h>
#include <dxgi1_5.h>
#include <d3d12.h>
#include <d3dx12.h>

#include <WICTextureLoader12.h>
#include <DDSTextureLoader12.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

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

		/*D3D11_PRIMITIVE_TOPOLOGY PrimitiveTypeTable[] =
		{
			D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
			D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		};*/

		DXGI_FORMAT TextureFormatTable[] =
		{
			DXGI_FORMAT_R32G32B32A32_FLOAT, // AUTO
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
				if (TextureFormatTable[i] == format)
				{
					return static_cast<PixelFormat>(i);
				}
			}
			return FORMAT_AUTO;
		}

		struct PipelineStateDX12
		{

		};

		struct TextureDX12
		{
			ID3D12Resource*				texture;
			uint16_t					srv;
			uint16_t					rtv;
			uint16_t					dsv;

			TextureType					type;
			PixelFormat					format;
			uint32_t					width;
			uint32_t					height;
			uint32_t					depthOrArraySize;
			uint32_t					mipLevels;
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


			HandleAlloc<RTVHeapSize>	rtvHeapAlloc;
			HandleAlloc<DSVHeapSize>	dsvHeapAlloc;
			HandleAlloc<SRVHeapSize>	srvHeapAlloc;

			ID3D12DescriptorHeap*		rtvHeap;
			ID3D12DescriptorHeap*		dsvHeap;
			ID3D12DescriptorHeap*		srvHeap;

			UINT						rtvHeapInc;
			UINT						dsvHeapInc;
			UINT						srvHeapInc;

			TextureDX12					textures[MaxTextureCount];



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

				{
					D3D12_DESCRIPTOR_HEAP_DESC desc = {};
					desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
					desc.NumDescriptors = RTVHeapSize;
					CHECKED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap)));
					rtvHeapInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				}

				{
					auto handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

					for (UINT i = 0; i < 2; ++i)
					{
						CHECKED(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])));
						device->CreateRenderTargetView(backBuffers[i], nullptr, handle);
						handle.ptr += rtvHeapInc;
					}
				}

				{
					D3D12_DESCRIPTOR_HEAP_DESC desc = {};
					desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
					desc.NumDescriptors = DSVHeapSize;
					CHECKED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsvHeap)));
					dsvHeapInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
				}

				{
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

					CHECKED(device->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, clearValue, IID_PPV_ARGS(&depthBuffer)));

					device->CreateDepthStencilView(depthBuffer, nullptr, dsvHeap->GetCPUDescriptorHandleForHeapStart());
				}

				return 0;
			}

			void InitPipelineStates()
			{

			}


#pragma region resource creation
			void ResetTexture(TextureDX12& tex)
			{
				RELEASE(tex.texture);
				FREE_HANDLE(tex.srv, srvHeapAlloc);
				FREE_HANDLE(tex.rtv, rtvHeapAlloc);
				FREE_HANDLE(tex.dsv, dsvHeapAlloc);
			}

			uint16_t CreateTexture(TextureType type, PixelFormat format, uint32_t bindFlags, uint32_t width, uint32_t height, uint32_t depth, uint32_t arraySize, uint32_t mipLevels)
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
						ResetTexture(tex);
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
						ResetTexture(tex);
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
						ResetTexture(tex);
						texHandleAlloc.Free(handle);
						return invalid_handle;
					}
				}
				else
				{
					tex.dsv = invalid_handle;
				}

				CD3DX12_RESOURCE_DESC resDesc;

				if (type == TEXTURE_1D)
				{
					resDesc = CD3DX12_RESOURCE_DESC::Tex1D(
						TextureFormatTable[format],
						width,
						arraySize,
						mipLevels,
						resFlag
					);
				}
				else if (type == TEXTURE_2D || type == TEXTURE_CUBE)
				{
					resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
						TextureFormatTable[format],
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
						TextureFormatTable[format],
						width, height, depth,
						mipLevels,
						resFlag
					);
				}

				if (FAILED(device->CreateCommittedResource(&heapProp,
					D3D12_HEAP_FLAG_DENY_BUFFERS,
					&resDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&tex.texture))))
				{

					ResetTexture(tex);
					texHandleAlloc.Free(handle);
					return invalid_handle;
				}

				if (invalid_handle != tex.srv)
				{
					D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
					// MARK
					CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvHeap->GetCPUDescriptorHandleForHeapStart(), tex.srv, srvHeapInc);
					device->CreateShaderResourceView(tex.texture, &desc, srvHandle);
				}

				return handle;
			}

			void DestroyTexture(uint16_t handle)
			{
				if (invalid_handle == handle)
					return;

				TextureDX12& tex = textures[handle];

				RELEASE(tex.texture);
				if (invalid_handle != tex.srv) rtvHeapAlloc.Free(tex.srv);
				if (invalid_handle != tex.rtv) rtvHeapAlloc.Free(tex.rtv);
				if (invalid_handle != tex.dsv) rtvHeapAlloc.Free(tex.dsv);
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
				return BufferHandle{ invalid_handle };
			}

			void DestroyBuffer(BufferHandle handle) override
			{

			}

			void UpdateBuffer(BufferHandle handle, size_t size, const void* data, size_t stride = 0) override
			{

			}


			// Textures
			TextureHandle CreateTexture(TextureType type, PixelFormat format, uint32_t bindFlags, uint32_t width, uint32_t height = 1, uint32_t depth = 1, uint32_t arraySize = 1, uint32_t mipLevels = 1, bool dynamic = false) override
			{
				return TextureHandle{ invalid_handle };
			}

			TextureHandle CreateTexture(const wchar_t* filename) override
			{
				return TextureHandle{ invalid_handle };
			}

			void DestroyTexture(TextureHandle handle) override
			{

			}

			void UpdateTexture(TextureHandle handle, size_t pitch, const void* data) override
			{

			}


			void Clear(TextureHandle handle, float color[4]) override
			{

			}

			void ClearDepth(TextureHandle handle, float depth) override
			{

			}

			void ClearDepthStencil(TextureHandle handle, float depth, uint8_t stencil) override
			{

			}


			// Samplers
			SamplerHandle CreateSampler() override
			{
				return SamplerHandle{ invalid_handle };
			}

			void DestroySampler(SamplerHandle handle) override
			{

			}


			// Shaders
			VertexShaderHandle CreateVertexShader(const void* bytecode, size_t size) override
			{
				return VertexShaderHandle{ invalid_handle };
			}

			void DestroyVertexShader(VertexShaderHandle handle) override
			{

			}


			PixelShaderHandle CreatePixelShader(const void* bytecode, size_t size) override
			{
				return PixelShaderHandle{ invalid_handle };
			}

			void DestroyPixelShader(PixelShaderHandle handle) override
			{

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

