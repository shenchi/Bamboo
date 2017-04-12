#pragma once

#include "common.h"
#include "HandleAlloc.h"

namespace bamboo
{
#define HANDLE_DECLARE(type) struct type##Handle { uint16_t id; }

	constexpr uint16_t invalid_handle = UINT16_MAX;

	HANDLE_DECLARE(PipelineState);
	HANDLE_DECLARE(Buffer);
	HANDLE_DECLARE(Texture);
	HANDLE_DECLARE(Sampler);
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
		FORMAT_R16G16B16B16_UNORM,
		FORMAT_R16G16B16B16_SNORM,
		FORMAT_R32G32B32A32_FLOAT,
		FORMAT_R16_SINT,
		FORMAT_R32_SINT,
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

	enum CullMode
	{
		CULL_NONE,
		CULL_FRONT,
		CULL_BACK
	};

	enum ComparisonFunc
	{
		COMPARISON_NEVER,
		COMPARISON_LESS,
		COMPARISON_EQUAL,
		COMPARISON_LESS_EQUAL,
		COMPARISON_GREATER,
		COMPARISON_NOT_EQUAL,
		COMPARISON_GREATER_EQUAL,
		COMPARISON_ALWAYS
	};

	enum BindingFlag
	{
		BINDING_VERTEX_BUFFER = 1 << 0,
		BINDING_INDEX_BUFFER = 1 << 1,
		BINDING_CONSTANT_BUFFER = 1 << 2,
		BINDING_SHADER_RESOURCE = 1 << 3,
		BINDING_RENDER_TARGET = 1 << 5,
		BINDING_DEPTH_STENCIL = 1 << 6,
	};

	enum TextureType
	{
		TEXTURE_1D,
		TEXTURE_2D,
		TEXTURE_3D,
		TEXTURE_CUBE,
	};

	constexpr size_t MaxVertexInputElement = 16;
	constexpr size_t MaxVertexBufferBindingSlot = 8;
	constexpr size_t MaxConstantBufferBindingSlot = 16;
	constexpr size_t MaxRenderTargetBindingSlot = 8;
	constexpr size_t MaxShaderResourceBindingSlot = 16; // 128;
	constexpr size_t MaxSamplerBindingSlot = 16;

	constexpr size_t MaxPipelineStateCount = 1024;
	constexpr size_t MaxBufferCount = 4096;
	constexpr size_t MaxTextureCount = 1024;
	constexpr size_t MaxSamplerCount = 1024;
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
		VertexLayout				VertexLayout;

		union
		{
			struct
			{
				uint32_t			CullMode : 2;
				uint32_t			_Reserved : 30;
			};
			uint32_t				RasterizerState;
		};

		union
		{
			struct
			{
				uint32_t			DepthEnable : 1;
				uint32_t			DepthWrite : 1;
				uint32_t			DepthFunc : 3;
				uint32_t			_Reserved : 27;
			};
			uint32_t				DepthStencilState;
		};

		VertexShaderHandle			VertexShader;
		PixelShaderHandle			PixelShader;

		PrimitiveType				PrimitiveType;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct DrawCall
	{
		uint32_t					ElementCount;

		union
		{
			struct
			{
				uint32_t			VertexBufferCount : 8;
				uint32_t			ConstantBufferCount : 4;
				uint32_t			RenderTargetCount : 4;
				uint32_t			TextureCount : 4;
				uint32_t			SamplerCount : 4;
				uint32_t			HasIndexBuffer : 1;
				uint32_t			HasDepthStencil : 1;
				uint32_t			_Reserved : 6;
			};
			uint32_t				InfoBits;
		};

		BufferHandle				VertexBuffers[MaxVertexBufferBindingSlot];
		BufferHandle				IndexBuffer;

		struct
		{
			BufferHandle			Handle;
			union
			{
				struct
				{
					uint16_t			BindingVertexShader : 1;
					uint16_t			BindingPixelShader : 1;
					uint16_t			_Reserved : 14;
				};
				uint16_t				BindingFlag;
			};
		}							ConstantBuffers[MaxConstantBufferBindingSlot];

		TextureHandle				RenderTargets[MaxRenderTargetBindingSlot];
		TextureHandle				DepthStencil;

		struct
		{
			union
			{
				TextureHandle			Texture;
				BufferHandle			Buffer;
			};
			union
			{
				struct
				{
					uint16_t			BindingVertexShader : 1;
					uint16_t			BindingPixelShader : 1;
					uint16_t			_Reserved : 13;
					uint16_t			IsBuffer : 1;
				};
				uint16_t				BindingFlag;
			};
		}							ShaderResources[MaxShaderResourceBindingSlot];

		struct
		{
			SamplerHandle			Handle;
			union
			{
				struct
				{
					uint16_t			BindingVertexShader : 1;
					uint16_t			BindingPixelShader : 1;
					uint16_t			_Reserved : 14;
				};
				uint16_t				BindingFlag;
			};
		}							Samplers[MaxSamplerBindingSlot];

		Viewport					Viewport;
	};
#pragma pack(pop)

	struct GraphicsAPI
	{
		// Pipeline States
		virtual PipelineStateHandle CreatePipelineState(const PipelineState& state) = 0;
		virtual void DestroyPipelineState(PipelineStateHandle handle) = 0;

		// Buffers
		virtual BufferHandle CreateBuffer(size_t size, uint32_t bindingFlags, bool dynamic = false) = 0;
		virtual void DestroyBuffer(BufferHandle handle) = 0;
		virtual void UpdateBuffer(BufferHandle handle, size_t size, const void* data, size_t stride = 0) = 0;

		// Textures
		virtual TextureHandle CreateTexture(TextureType type, PixelFormat format, uint32_t bindFlags, uint32_t width, uint32_t height = 1, uint32_t depth = 1, uint32_t arraySize = 1, uint32_t mipLevels = 1, bool dynamic = false) = 0;
		virtual TextureHandle CreateTexture(const wchar_t* filename) = 0;
		virtual void DestroyTexture(TextureHandle handle) = 0;
		virtual void UpdateTexture(TextureHandle handle, size_t pitch, const void* data) = 0;

		virtual void Clear(TextureHandle handle, float color[4]) = 0;
		virtual void ClearDepth(TextureHandle handle, float depth) = 0;
		virtual void ClearDepthStencil(TextureHandle handle, float depth, uint8_t stencil) = 0;

		// Samplers
		virtual SamplerHandle CreateSampler() = 0; // TODO
		virtual void DestroySampler(SamplerHandle handle) = 0;

		// Shaders
		virtual VertexShaderHandle CreateVertexShader(const void* bytecode, size_t size) = 0;
		virtual void DestroyVertexShader(VertexShaderHandle handle) = 0;

		virtual PixelShaderHandle CreatePixelShader(const void* bytecode, size_t size) = 0;
		virtual void DestroyPixelShader(PixelShaderHandle handle) = 0;

		// Draw Functions
		virtual void Draw(PipelineStateHandle stateHandle, const DrawCall& drawcall) = 0;

		// Swap Chains
		// TODO, bind swap chains with render targets
		virtual void Present() = 0;

		// Clean up
		virtual void Shutdown() = 0;

		HandleAlloc<MaxPipelineStateCount>		psoHandleAlloc;
		HandleAlloc<MaxBufferCount>				bufHandleAlloc;
		HandleAlloc<MaxTextureCount>			texHandleAlloc;
		HandleAlloc<MaxSamplerCount>			sampHandleAlloc;
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
