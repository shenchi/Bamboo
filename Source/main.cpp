#include <Windows.h>
#include "Engine.h"

#define BAMBOO_TEST_GRAPHICS_API 1

#if BAMBOO_TEST_GRAPHICS_API

#include "GraphicsAPI.h"
#include "NativeWindow.h"

#include <cstdio>

struct Memory
{
	void*	ptr;
	size_t	size;

	Memory() : ptr(nullptr), size(0) {}
	Memory(size_t size) : ptr(nullptr), size(size) { ptr = reinterpret_cast<void*>(new char[size]); }

	~Memory() { if (nullptr != ptr) delete[] reinterpret_cast<char*>(ptr); }

	Memory(const Memory&) = delete;
	Memory(Memory&& m) : ptr(m.ptr), size(m.size) { m.ptr = nullptr; m.size = 0; }
};

Memory LoadFile(const char* filename)
{
	FILE* fp = fopen(filename, "rb");
	if (nullptr == fp) return Memory();

	fseek(fp, 0, SEEK_END);
	size_t length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	Memory mem(length);
	size_t length_read = fread(mem.ptr, 1, length, fp);
	fclose(fp);

	if (length != length_read)
		return Memory();

	return mem;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	bamboo::win32::NativeWindow win(L"Bamboo", 800, 600);

	bamboo::GraphicsAPI* api = bamboo::InitGraphicsAPI(bamboo::Direct3D11, win.GetHandle());

	if (nullptr == api)
		return -1;

	float vertices[] =
	{
		0.0f, 0.5f, 0.0f,
		0.5f, -0.25f, 0.0f,
		-0.5f, -0.25f, 0.0f,
	};

	auto vb = api->CreateVertexBuffer(sizeof(vertices), false);
	api->UpdateVertexBuffer(vb, sizeof(vertices), reinterpret_cast<void*>(vertices), sizeof(float) * 3);

	auto vs_byte = LoadFile("vs_simple.cso");
	auto ps_byte = LoadFile("ps_simple.cso");

	auto vs = api->CreateVertexShader(vs_byte.ptr, vs_byte.size);
	auto ps = api->CreatePixelShader(ps_byte.ptr, ps_byte.size);

	bamboo::VertexLayout layout = {};
	layout.ElementCount = 1;
	layout.Elements[0] =
	{
		bamboo::SEMANTIC_POSITION, 3 - 1 /* 0~3 stands for 1~4 */, bamboo::TYPE_FLOAT, 0, 0
	};

	auto vl = api->CreateVertexLayout(layout);

	float color[] = { 1.0f, 0.0f, 0.0f, 1.0f };
	auto cb = api->CreateConstantBuffer(sizeof(float) * 4);
	api->UpdateConstantBuffer(cb, sizeof(float) * 4, color);

	bamboo::PipelineState state = {};

	state.VertexBufferCount = 1;
	state.HasVertexShader = 1;
	state.HasPixelShader = 1;

	state.VertexBuffers[0] = vb;

	state.VertexShader = vs;
	state.PixelShader = ps;

	state.VertexLayout = vl;

	state.ConstantBufferCount = 1;
	state.ConstantBuffers[0] = { cb, { 0, 1, 0} };

	state.Viewport = { 0, 0, 800, 600, 0.0f, 1.0f };

	state.PrimitiveType = bamboo::PRIMITIVE_TRIANGLES;

	float clearColor[] = { 0.7f, 0.7f, 0.7f, 1.0f };
	auto invalidRTHandle = bamboo::RenderTargetHandle{ bamboo::invalid_handle }; // TODO

	while (win.ProcessEvent())
	{
		api->Clear(invalidRTHandle, clearColor);
		api->ClearDepthStencil(invalidRTHandle, 1.0f, 0);
		api->Draw(state, 3);
		api->Present();
	}

	api->Shutdown();

	return 0;
}

#elif BAMBOO_TEST_RENDERER

#include "Renderer.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	bamboo::Engine engine;

	engine.Init();

	return engine.Run();
}

#else

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	bamboo::Engine engine;

	engine.Init();

	return engine.Run();
}

#endif