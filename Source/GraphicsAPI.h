#pragma once

#include "common.h"
#include "HandleAlloc.h"

namespace bamboo
{
#define HANDLE_DECLARE(type) struct type##Handle { uint16_t id; }

	constexpr uint16_t invalid_handle = UINT16_MAX;

	HANDLE_DECLARE(VertexLayout);
	HANDLE_DECLARE(VertexBuffer);
	HANDLE_DECLARE(IndexBuffer);
	HANDLE_DECLARE(RenderTarget);
	HANDLE_DECLARE(Texture);
	HANDLE_DECLARE(VertexShader);
	HANDLE_DECLARE(PixelShader);

#undef HANDLE_DECLARE

	enum PrimitiveType
	{
		POINT,
		LINES,
		TRIANGLES,
		NUM_PRIMITIVE_TYPE
	};

	enum PixelFormat
	{
		AUTO_PIXEL_FORMAT,
		R8G8B8A8_UNORM,
		R8G8B8A8_SNORM,
		R16_INT,
		R32_INT,
		R16_UINT,
		R32_UINT,
		D24_UNORM_S8_UINT,
		NUM_PIXEL_FORMAT
	};

	enum VertexInputSemantics
	{
		POSITION,
		COLOR,
		NORMAL,
		TANGENT,
		BINORMAL,
		TEXCOORD0,
		TEXCOORD1,
		TEXCOORD2,
		TEXCOORD3,
	};

	enum VertexInputType
	{
		COMPONENT_FLOAT,
	};

	constexpr size_t MaxVertexInputSlot = 8;
	constexpr size_t MaxVertexBufferBindingSlot = 8;
	constexpr size_t MaxRenderTargetBindingSlot = 8;

	constexpr size_t MaxVertexLayoutCount = 1024;
	constexpr size_t MaxVertexBufferCount = 1024;
	constexpr size_t MaxIndexBufferCount = 1024;
	constexpr size_t MaxRenderTargetCount = 32;
	constexpr size_t MaxTextureCount = 1024;
	constexpr size_t MaxVertexShaderCount = 1024;
	constexpr size_t MaxPixelShaderCount = 1024;

#pragma pack(push, 1)
	struct VertexInputSlot
	{
		uint16_t				SemanticId : 4;
		uint16_t				ComponentCount : 2;
		uint16_t				ComponentType : 3;
		uint16_t				Reserved : 7;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct VertexLayout
	{
		VertexInputSlot			Slots[MaxVertexInputSlot];
		uint16_t				SlotCount;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct PipelineStateSwitches
	{
		uint32_t				VertexBufferCount : 16;
		uint32_t				HasIndexBuffer : 1;
		uint32_t				HasVertexShader : 1;
		uint32_t				HasPixelShader : 1;
		uint32_t				Reserved : 13;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct PipelineState
	{
		VertexBufferHandle			VertexBuffers[MaxVertexBufferBindingSlot];
		IndexBufferHandle			IndexBuffer;

		VertexLayoutHandle			VertexLayout;

		VertexShaderHandle			VertexShader;
		PixelShaderHandle			PixelShader;

		PrimitiveType				PrimitiveType;

		union
		{
			PipelineStateSwitches	Switches;
			uint32_t				Switches_raw;
		};
	};
#pragma pack(pop)

	struct GraphicsAPI
	{
		// Vertex Layout
		virtual VertexLayoutHandle CreateVertexLayout(VertexLayout layout) = 0;
		virtual void DestroyVertexLayout(VertexLayoutHandle handle) = 0;

		// Buffers
		virtual VertexBufferHandle CreateVertexBuffer(size_t size, bool dynamic) = 0;
		virtual void DestroyVertexBuffer(VertexBufferHandle handle) = 0;
		virtual void UpdateVertexBuffer(VertexBufferHandle handle, size_t size, const void* data) = 0;

		virtual IndexBufferHandle CreateIndexBuffer(size_t size, bool dynamic) = 0;
		virtual void DestroyIndexBuffer(IndexBufferHandle handle) = 0;
		virtual void UpdateIndexBuffer(IndexBufferHandle handle, size_t size, const void* data) = 0;

		virtual RenderTargetHandle CreateRenderTarget(PixelFormat format, uint32_t width, uint32_t height, bool isDepth, bool hasStencil) = 0;
		virtual void DestroyRenderTarget(RenderTargetHandle handle) = 0;

		// Textures

		// Shaders
		virtual VertexShaderHandle CreateVertexShader(const void* bytecode, size_t size) = 0;
		virtual void DestroyVertexShader(VertexShaderHandle handle) = 0;

		virtual PixelShaderHandle CreatePixelShader(const void* bytecode, size_t size) = 0;
		virtual void DestroyPixelShader(PixelShaderHandle handle) = 0;

		// Draw Functions
		virtual void Draw(const PipelineState& state) = 0;

		PipelineState							internalState;
		HandleAlloc<MaxVertexLayoutCount>		vlHandleAlloc;
		HandleAlloc<MaxVertexBufferCount>		vbHandleAlloc;
		HandleAlloc<MaxIndexBufferCount>		ibHandleAlloc;
		HandleAlloc<MaxRenderTargetCount>		rtHandleAlloc;
		HandleAlloc<MaxTextureCount>			texHandleAlloc;
		HandleAlloc<MaxVertexShaderCount>		vsHandleAlloc;
		HandleAlloc<MaxPixelShaderCount>		psHandleAlloc;
	};


	enum GraphicsAPIType
	{
		Direct3D11,
		Direct3D12,
		GNM
	};


	GraphicsAPI* InitGraphicsAPI(GraphicsAPIType type, void* windowHandle);
}
