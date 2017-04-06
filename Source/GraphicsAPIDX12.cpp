#include "GraphicsAPIDX12.h"

#include <Windows.h>
#include <dxgi1_5.h>
#include <d3d12.h>
#include <d3dx12.h>

#include <WICTextureLoader12.h>
#include <DDSTextureLoader12.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

#define CE(x, e) if (S_OK != (x)) return (e);
#define CHECKED(x) if (S_OK != (x)) return -1;

namespace bamboo
{
	namespace dx12
	{

		struct PipelineStateDX12
		{

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


			ID3D12DescriptorHeap*		rtvHeap;
			UINT						rtvHeapInc;

			ID3D12DescriptorHeap*		dsvHeap;
			UINT						dsvHeapInc;

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
					desc.NumDescriptors = MaxRenderTargetCount;
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
					desc.NumDescriptors = MaxRenderTargetCount;
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


			// Pipeline States
			PipelineStateHandle CreatePipelineState(const PipelineState& state) override
			{

			}

			void DestroyPipelineState(PipelineStateHandle handle) override
			{

			}

			// Buffers
			VertexBufferHandle CreateVertexBuffer(size_t size, bool dynamic) override
			{

			}

			void DestroyVertexBuffer(VertexBufferHandle handle) override
			{

			}

			void UpdateVertexBuffer(VertexBufferHandle handle, size_t size, const void* data, size_t stride) override
			{

			}

			IndexBufferHandle CreateIndexBuffer(size_t size, bool dynamic) override
			{

			}

			void DestroyIndexBuffer(IndexBufferHandle handle) override
			{

			}

			void UpdateIndexBuffer(IndexBufferHandle handle, size_t size, const void* data, DataType type) override
			{

			}

			ConstantBufferHandle CreateConstantBuffer(size_t size) override
			{

			}

			void DestroyConstantBuffer(ConstantBufferHandle handle) override
			{

			}

			void UpdateConstantBuffer(ConstantBufferHandle handle, size_t size, const void* data) override
			{

			}

			// Render targets
			RenderTargetHandle CreateRenderTarget(PixelFormat format, uint32_t width, uint32_t height, bool isDepth, bool hasStencil) override
			{

			}

			void DestroyRenderTarget(RenderTargetHandle handle) override
			{

			}

			void Clear(RenderTargetHandle handle, float color[4]) override
			{

			}

			void ClearDepth(RenderTargetHandle handle, float depth) override
			{

			}

			void ClearDepthStencil(RenderTargetHandle handle, float depth, uint8_t stencil) override
			{

			}

			// Textures
			TextureHandle CreateTexture(PixelFormat format, uint32_t width, uint32_t height, bool dynamic) override
			{

			}

			TextureHandle CreateTexture(const wchar_t* filename) override
			{

			}

			void DestroyTexture(TextureHandle handle) override
			{

			}

			void UpdateTexture(TextureHandle handle, size_t pitch, const void* data) override
			{

			}

			// Samplers
			SamplerHandle CreateSampler() override
			{

			} 
			
			void DestroySampler(SamplerHandle handle) override
			{

			}

			// Shaders
			VertexShaderHandle CreateVertexShader(const void* bytecode, size_t size) override
			{

			}

			void DestroyVertexShader(VertexShaderHandle handle) override
			{

			}

			PixelShaderHandle CreatePixelShader(const void* bytecode, size_t size) override
			{

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

		};

		GraphicsAPI* InitGraphicsAPIDX12(void * windowHandle)
		{
			GraphicsAPIDX12* api = new GraphicsAPIDX12();
			return nullptr;
		}
	}
}

