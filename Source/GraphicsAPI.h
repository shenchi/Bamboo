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
	HANDLE_DECLARE(ConstantBuffer);
	HANDLE_DECLARE(RenderTarget);
	HANDLE_DECLARE(Texture);
	HANDLE_DECLARE(VertexShader);
	HANDLE_DECLARE(PixelShader);

#undef HANDLE_DECLARE

	enum PrimitiveType
	{
		PRIMITIVE_POINTS,
		PRIMITIVE_LINES,
		PRIMITIVE_TRIANGLES,
		NUM_PRIMITIVE_TYPE
	};

	enum PixelFormat
	{
		FORMAT_AUTO,
		FORMAT_R8G8B8A8_UNORM,
		FORMAT_R8G8B8A8_SNORM,
		FORMAT_R16_INT,
		FORMAT_R32_INT,
		FORMAT_R16_UINT,
		FORMAT_R32_UINT,
		FORMAT_D24_UNORM_S8_UINT,
		NUM_PIXEL_FORMAT
	};

	enum Semantics
	{
		SEMANTIC_POSITION,
		SEMANTIC_COLOR,
		SEMANTIC_NORMAL,
		SEMANTIC_TANGENT,
		SEMANTIC_BINORMAL,
		SEMANTIC_TEXCOORD0,
		SEMANTIC_TEXCOORD1,
		SEMANTIC_TEXCOORD2,
		SEMANTIC_TEXCOORD3,
	};

	enum DataType
	{
		TYPE_FLOAT,
		TYPE_INT8,
		TYPE_UINT8,
		TYPE_INT16,
		TYPE_UINT16,
		TYPE_INT32,
		TYPE_UINT32,
	};

	constexpr size_t MaxVertexInputElement = 16;
	constexpr size_t MaxVertexBufferBindingSlot = 8;
	constexpr size_t MaxConstantBufferBindingSlot = 16;
	constexpr size_t MaxRenderTargetBindingSlot = 8;

	constexpr size_t MaxVertexLayoutCount = 1024;
	constexpr size_t MaxVertexBufferCount = 1024;
	constexpr size_t MaxIndexBufferCount = 1024;
	constexpr size_t MaxConstantBufferCount = 1024;
	constexpr size_t MaxRenderTargetCount = 32;
	constexpr size_t MaxTextureCount = 1024;
	constexpr size_t MaxVertexShaderCount = 1024;
	constexpr size_t MaxPixelShaderCount = 1024;

#pragma pack(push, 1)
	struct VertexInputElement
	{
		uint16_t				SemanticId : 4;
		uint16_t				ComponentCount : 2;  // 0 ~ 3 stands for 1 ~ 4
		uint16_t				ComponentType : 3;
		uint16_t				Reserved : 3;
		uint16_t				BindingSlot : 4;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct VertexLayout
	{
		VertexInputElement		Elements[MaxVertexInputElement];
		uint16_t				ElementCount;
	};
#pragma pack(pop)

#pragma pack(push, 4)
	struct Viewport
	{
		float X, Y;
		float Width, Height;
		float ZMin, ZMax;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct PipelineState
	{
		union
		{
			struct
			{
				uint32_t			VertexBufferCount : 8;
				uint32_t			ConstantBufferCount : 8;
				uint32_t			RenderTargetCount : 8;
				uint32_t			HasIndexBuffer : 1;
				uint32_t			HasVertexShader : 1;
				uint32_t			HasPixelShader : 1;
				uint32_t			HasDepthStencil : 1;
				uint32_t			_Reserved : 4;
			};
			uint32_t				InfoBits;
		};

		VertexBufferHandle			VertexBuffers[MaxVertexBufferBindingSlot];
		IndexBufferHandle			IndexBuffer;

		VertexLayoutHandle			VertexLayout;

		struct
		{
			ConstantBufferHandle	Handle;
			uint16_t				BindingFlag;  // TBD: ? bit 0 : Vertex Shader, bit 1 : Pixel Shader
		}							ConstantBuffers[MaxConstantBufferBindingSlot];

		VertexShaderHandle			VertexShader;
		PixelShaderHandle			PixelShader;

		RenderTargetHandle			RenderTargets[MaxRenderTargetBindingSlot];
		RenderTargetHandle			DepthStencil;


		Viewport					Viewport;

		PrimitiveType				PrimitiveType;
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
		virtual void UpdateVertexBuffer(VertexBufferHandle handle, size_t size, const void* data, size_t stride) = 0;

		virtual IndexBufferHandle CreateIndexBuffer(size_t size, bool dynamic) = 0;
		virtual void DestroyIndexBuffer(IndexBufferHandle handle) = 0;
		virtual void UpdateIndexBuffer(IndexBufferHandle handle, size_t size, const void* data, DataType type) = 0;

		virtual ConstantBufferHandle CreateConstantBuffer(size_t size) = 0;
		virtual void DestroyConstantBuffer(ConstantBufferHandle handle) = 0;
		virtual void UpdateConstantBuffer(ConstantBufferHandle handle, size_t size, const void* data) = 0;

		// Render targets
		virtual RenderTargetHandle CreateRenderTarget(PixelFormat format, uint32_t width, uint32_t height, bool isDepth, bool hasStencil) = 0;
		virtual void DestroyRenderTarget(RenderTargetHandle handle) = 0;

		virtual void Clear(RenderTargetHandle handle, float color[4]) = 0;
		virtual void ClearDepth(RenderTargetHandle handle, float depth) = 0;
		virtual void ClearDepthStencil(RenderTargetHandle handle, float depth, uint8_t stencil) = 0;

		// Textures

		// Shaders
		virtual VertexShaderHandle CreateVertexShader(const void* bytecode, size_t size) = 0;
		virtual void DestroyVertexShader(VertexShaderHandle handle) = 0;

		virtual PixelShaderHandle CreatePixelShader(const void* bytecode, size_t size) = 0;
		virtual void DestroyPixelShader(PixelShaderHandle handle) = 0;

		// Draw Functions
		virtual void Draw(const PipelineState& state, uint32_t vertexCount) = 0;
		virtual void DrawIndex(const PipelineState& state, uint32_t indexCount) = 0;

		// Swap Chains
		// TODO, bind swap chains with render targets
		virtual void Present() = 0;

		// Clean up
		virtual void Shutdown() = 0;

		PipelineState							internalState;
		HandleAlloc<MaxVertexLayoutCount>		vlHandleAlloc;
		HandleAlloc<MaxVertexBufferCount>		vbHandleAlloc;
		HandleAlloc<MaxIndexBufferCount>		ibHandleAlloc;
		HandleAlloc<MaxConstantBufferCount>		cbHandleAlloc;
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
