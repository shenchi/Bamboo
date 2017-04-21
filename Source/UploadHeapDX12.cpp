#include "UploadHeapDX12.h"

#include <d3d12.h>
#include <d3dx12.h>

#define RELEASE(x) if (nullptr != (x)) { (x)->Release(); (x) = nullptr; }

namespace bamboo
{
	namespace dx12
	{

#pragma region Sync Upload Heap

		bool UploadHeapSyncDX12::Init(ID3D12Device * device, ID3D12GraphicsCommandList * cmdList)
		{
			this->device = device;
			this->cmdList = cmdList;

			CD3DX12_HEAP_DESC heapDesc(UploadHeapSize, D3D12_HEAP_TYPE_UPLOAD, 0Ui64,
				(D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES));
			if (FAILED(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap))))
			{
				return false;
			}

			alloc = alloc_t::create(treeMem);

			bufferCount = 0;
			return true;
		}

		bool UploadHeapSyncDX12::UploadResource(ID3D12Resource* destRes, uint32_t firstSubRes, uint32_t subResCount, D3D12_SUBRESOURCE_DATA* data)
		{
			if (bufferCount >= UploadHeapQueueSize)
				return false;

			uint32_t bufIdx = bufferCount;

			UINT64 size = GetRequiredIntermediateSize(destRes, firstSubRes, subResCount);
			/*if (0 != rowPitch && 0 == size)
			{
				D3D12_RESOURCE_DESC resDesc = destRes->GetDesc();
				UINT64 requiredSize = 0;
				device->GetCopyableFootprints(&resDesc, 0, 1, 0, nullptr, nullptr, nullptr, &requiredSize);
				size = static_cast<uint32_t>(requiredSize);
			}*/

			void* ptr = alloc->allocate(reinterpret_cast<void*>(0x10), static_cast<uint32_t>(size));
			if (nullptr == ptr)
			{
				return false;
			}

			UINT64 offset = reinterpret_cast<UINT64>(ptr) - 0x10u;

			ID3D12Resource* uploadRes = nullptr;
			D3D12_RESOURCE_DESC uploadDesc = {};
			uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			uploadDesc.Width = size;
			uploadDesc.Height = 1;
			uploadDesc.DepthOrArraySize = 1;
			uploadDesc.MipLevels = 1;
			uploadDesc.SampleDesc.Count = 1;
			uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			if (FAILED(device->CreatePlacedResource(
				heap,
				offset,
				&uploadDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&uploadRes))))
			{
				alloc->deallocate(nullptr, reinterpret_cast<void*>(offset));
				return false;
			}

			buffers[bufIdx].offset = static_cast<uint32_t>(offset);
			buffers[bufIdx].resource = uploadRes;

			UpdateSubresources(cmdList, destRes, uploadRes, 0, firstSubRes, subResCount, data);

			/*void* pData = nullptr;
			D3D12_RANGE range = { 0, 0 };
			if (FAILED(uploadRes->Map(0, &range, &pData)))
			{
				return false;
			}
			memcpy(pData, data, size);
			uploadRes->Unmap(0, nullptr);*/



			/*cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destRes,
				D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_STATE_COPY_DEST));*/

			//if (0 == rowPitch) // for buffer
			//{
			//	cmdList->CopyBufferRegion(destRes, 0, uploadRes, 0, size);
			//}
			//else // for texture
			//{
			//	D3D12_RESOURCE_DESC destDesc = destRes->GetDesc();
			//	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint =
			//	{
			//		0,
			//		CD3DX12_SUBRESOURCE_FOOTPRINT(destDesc, rowPitch)
			//	};
			//	CD3DX12_TEXTURE_COPY_LOCATION srcLoc(uploadRes, footprint);
			//	CD3DX12_TEXTURE_COPY_LOCATION destLoc(destRes, 0);

			//	cmdList->CopyTextureRegion(&destLoc, 0, 0, 0, &srcLoc, nullptr);
			//}

			/*cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destRes,
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_COMMON));*/

			bufferCount++;

			return true;
		}

		void UploadHeapSyncDX12::Clear()
		{
			for (uint32_t i = 0; i < bufferCount; i++)
			{
				UINT64 addr = buffers[i].offset;
				alloc->deallocate(nullptr, reinterpret_cast<void*>(addr));
				buffers[i].resource->Release();
			}
			bufferCount = 0;
		}

		void UploadHeapSyncDX12::Release()
		{
			Clear();
			RELEASE(heap);
		}


#pragma endregion


#pragma region Async Upload Heap


		UploadHeapDX12::~UploadHeapDX12()
		{
			WaitForExecution();
			CheckFence();

			CloseHandle(hEvent);
			RELEASE(cmdAlloc[0]);
			RELEASE(cmdAlloc[1]);
			RELEASE(cmdQueue);
			RELEASE(cmdList);
			RELEASE(flagFence);
			RELEASE(queueFence);
			RELEASE(heap);
		}

		bool UploadHeapDX12::Init(ID3D12Device * device)
		{
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

			if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue))))
			{
				return false;
			}

			if (FAILED(device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_COPY,
				IID_PPV_ARGS(&cmdAlloc[0]))))
			{
				return false;
			}

			if (FAILED(device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_COPY,
				IID_PPV_ARGS(&cmdAlloc[0]))))
			{
				return false;
			}

			if (FAILED(device->CreateCommandList(0,
				D3D12_COMMAND_LIST_TYPE_COPY,
				cmdAlloc[currentCmdAlloc],
				nullptr,
				IID_PPV_ARGS(&cmdList))))
			{
				return false;
			}

			if (FAILED(device->CreateFence(1 - currentCmdAlloc, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&flagFence))))
			{
				return false;
			}

			if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&queueFence))))
			{
				return false;
			}

			hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

			CD3DX12_HEAP_DESC heapDesc(UploadHeapSize, D3D12_HEAP_TYPE_UPLOAD, 0Ui64,
				(D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES));
			if (FAILED(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap))))
			{
				return false;
			}

			this->device = device;

			alloc = alloc_t::create(treeMem);

			return true;
		}

		bool UploadHeapDX12::UploadResource(ID3D12Resource* destRes, size_t size, const void* data, uint32_t rowPitch)
		{
			CheckFence();

			size_t newTail = (tail + 1) % UploadHeapQueueSize;
			if (newTail == head) // ring buffer is full
			{
				return false;
			}

			if (0 != rowPitch && 0 == size)
			{
				D3D12_RESOURCE_DESC resDesc = destRes->GetDesc();
				UINT64 requiredSize = 0;
				device->GetCopyableFootprints(&resDesc, 0, 1, 0, nullptr, nullptr, nullptr, &requiredSize);
				size = static_cast<uint32_t>(requiredSize);
			}

			void* ptr = alloc->allocate(reinterpret_cast<void*>(0x10), static_cast<uint32_t>(size));
			if (nullptr == ptr)
			{
				return false;
			}

			UINT64 offset = reinterpret_cast<UINT64>(ptr) - 0x10u;

			ID3D12Resource* uploadRes = nullptr;
			D3D12_RESOURCE_DESC uploadDesc = {};
			uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			uploadDesc.Width = size;
			uploadDesc.Height = 1;
			uploadDesc.DepthOrArraySize = 1;
			uploadDesc.MipLevels = 1;
			uploadDesc.SampleDesc.Count = 1;
			uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			if (FAILED(device->CreatePlacedResource(
				heap,
				offset,
				&uploadDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&uploadRes))))
			{
				alloc->deallocate(nullptr, reinterpret_cast<void*>(offset));
				return false;
			}

			ringBuffer[tail].offset = static_cast<uint32_t>(offset);
			ringBuffer[tail].resource = uploadRes;

			void* pData = nullptr;
			D3D12_RANGE range = { 0, 0 };
			if (FAILED(uploadRes->Map(0, &range, &pData)))
			{
				return false;
			}
			memcpy(pData, data, size);
			uploadRes->Unmap(0, nullptr);


			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destRes,
				D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_STATE_COPY_DEST));

			if (0 == rowPitch) // for buffer
			{
				cmdList->CopyBufferRegion(destRes, 0, uploadRes, 0, size);
			}
			else // for texture
			{
				D3D12_RESOURCE_DESC destDesc = destRes->GetDesc();
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint =
				{
					0,
					CD3DX12_SUBRESOURCE_FOOTPRINT(destDesc, rowPitch)
				};
				CD3DX12_TEXTURE_COPY_LOCATION srcLoc(uploadRes, footprint);
				CD3DX12_TEXTURE_COPY_LOCATION destLoc(destRes, 0);

				cmdList->CopyTextureRegion(&destLoc, 0, 0, 0, &srcLoc, nullptr);
			}

			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destRes,
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_COMMON));

			tail = newTail;

			return true;
		}

		void UploadHeapDX12::Execute()
		{
			cmdList->Close();

			ID3D12CommandList* lists[1] = { cmdList };
			cmdQueue->ExecuteCommandLists(1, lists);
			cmdQueue->Signal(queueFence, static_cast<UINT64>(tail));
			cmdQueue->Signal(flagFence, currentCmdAlloc);

			WaitForExecution();

			cmdAlloc[currentCmdAlloc]->Reset();
			cmdList->Reset(cmdAlloc[currentCmdAlloc], nullptr);

			CheckFence();
		}

		void UploadHeapDX12::WaitForExecution()
		{
			currentCmdAlloc = 1 - currentCmdAlloc;
			if (flagFence->GetCompletedValue() != currentCmdAlloc)
			{
				flagFence->SetEventOnCompletion(currentCmdAlloc, hEvent);
				WaitForSingleObject(hEvent, INFINITE);
			}
		}

		void UploadHeapDX12::CheckFence()
		{
			size_t completed = static_cast<size_t>(queueFence->GetCompletedValue());

			while (head != completed)
			{
				UINT64 addr = ringBuffer[head].offset;
				alloc->deallocate(nullptr, reinterpret_cast<void*>(addr));
				ringBuffer[head].resource->Release();

				head = (head + 1) % UploadHeapQueueSize;
			}
		}

#pragma endregion

	}

}