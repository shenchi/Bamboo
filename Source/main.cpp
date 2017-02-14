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
		-0.5f, -0.25f, 0.0f
	};

	auto vb = api->CreateVertexBuffer(sizeof(vertices), false);

	auto vs_byte = LoadFile("vs_simple.cso");
	auto ps_byte = LoadFile("ps_simple.cso");

	auto vs = api->CreateVertexShader(vs_byte.ptr, vs_byte.size);
	auto ps = api->CreatePixelShader(ps_byte.ptr, ps_byte.size);

	bamboo::VertexLayout layout = {};
	layout.ElementCount = 1;
	layout.Elements[0] = 
	{
		bamboo::POSITION, 3, bamboo::COMPONENT_FLOAT, 0, 0
	};

	auto vl = api->CreateVertexLayout(layout);

	bamboo::PipelineState state = {};

	// TODO state

	while (win.ProcessEvent())
	{
		api->Draw(state, 3);
	}

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