#pragma once

#include "BuddyAllocator.h"

#include <cstdint>

struct ID3D12Device;
struct ID3D12CommandAllocator;
struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList;
struct ID3D12Fence;
struct ID3D12Heap;
struct ID3D12Resource;

struct D3D12_SUBRESOURCE_DATA;

namespace bamboo
{
	namespace dx12
	{
		constexpr size_t UploadHeapSize = 32 * 1024 * 1024; // 32 MB
		constexpr size_t UploadHeapBufferMinSize = 64 * 1024; // 64 KB
		constexpr size_t UploadHeapQueueSize = 1024;

		struct UploadHeapSyncDX12
		{
			typedef bamboo::memory::BuddyAllocator<UploadHeapSize, UploadHeapBufferMinSize> alloc_t;

			bool Init(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

			bool UploadResource(ID3D12Resource* destRes, uint32_t firstSubRes, uint32_t subResCount, D3D12_SUBRESOURCE_DATA* data);

			void Clear();

			void Release();

			ID3D12Device*				device;
			ID3D12GraphicsCommandList*	cmdList;
			ID3D12Heap*					heap;

			alloc_t*					alloc;
			uint8_t						treeMem[alloc_t::treeSize];

			struct
			{
				uint32_t				offset;
				ID3D12Resource*			resource;
			}							buffers[UploadHeapQueueSize];
			uint32_t					bufferCount;
		};

		struct UploadHeapDX12
		{
			typedef bamboo::memory::BuddyAllocator<UploadHeapSize, UploadHeapBufferMinSize> alloc_t;

			UploadHeapDX12()
				:
				device(nullptr),
				cmdAlloc{},
				cmdQueue(nullptr),
				cmdList(nullptr),
				flagFence(nullptr),
				queueFence(nullptr),
				heap(nullptr),
				currentCmdAlloc(0),
				alloc(nullptr)
			{}

			~UploadHeapDX12();

			bool Init(ID3D12Device* device);

			bool UploadResource(ID3D12Resource* destRes, size_t size, const void* data, uint32_t rowPitch);

			void Execute();
			
			void WaitForExecution();

			void CheckFence();

			ID3D12Device*				device;
			ID3D12CommandAllocator*		cmdAlloc[2];
			ID3D12CommandQueue*			cmdQueue;
			ID3D12GraphicsCommandList*	cmdList;
			ID3D12Fence*				flagFence;
			ID3D12Fence*				queueFence;
			ID3D12Heap*					heap;

			size_t						currentCmdAlloc;

			void*						hEvent;

			alloc_t*					alloc;
			uint8_t						treeMem[alloc_t::treeSize];

			struct
			{
				uint32_t				offset;
				ID3D12Resource*			resource;
			}							ringBuffer[UploadHeapQueueSize];

			size_t						tail;
			size_t						head;
		};
	}
}