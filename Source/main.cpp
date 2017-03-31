#include <Windows.h>
#include "Engine.h"

#define BAMBOO_TEST_GRAPHICS_API 1

#if BAMBOO_TEST_GRAPHICS_API

#include "GraphicsAPI.h"
#include "NativeWindow.h"
#include "AssimpLoader.h"
#include "Camera.h"

#include <DirectXMath.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <memory>
#include <cstdio>

using namespace DirectX;

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

struct Timer
{
public:
	Timer()
	{
		__int64 perfFreq;
		QueryPerformanceFrequency((LARGE_INTEGER*)&perfFreq);
		perfCounterSeconds = 1.0 / (double)perfFreq;
	}

	void Start()
	{
		__int64 now;
		QueryPerformanceCounter((LARGE_INTEGER*)&now);
		startTime = now;
		currentTime = now;
		previousTime = now;
	}

	void Update()
	{
		__int64 now;
		QueryPerformanceCounter((LARGE_INTEGER*)&now);
		currentTime = now;

		deltaTime = max((float)((currentTime - previousTime) * perfCounterSeconds), 0.0f);

		// Calculate the total time from start to now
		totalTime = (float)((currentTime - startTime) * perfCounterSeconds);

		// Save current time for next frame
		previousTime = currentTime;
	}

	double perfCounterSeconds;
	long long currentTime;
	long long previousTime;
	long long startTime;

	float deltaTime;
	float totalTime;
};

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	bamboo::win32::NativeWindow win(L"Bamboo", 800, 600);

	bamboo::GraphicsAPI* api = bamboo::InitGraphicsAPI(bamboo::Direct3D11, win.GetHandle());

	std::unique_ptr<Keyboard> keyboard = std::make_unique<Keyboard>();
	std::unique_ptr<Mouse> mouse = std::make_unique<Mouse>();
	Timer timer;
	Camera camera;

	mouse->SetWindow(reinterpret_cast<HWND>(win.GetHandle()));

	if (nullptr == api)
		return -1;

	AssimpLoader assimp;
	assimp.LoadFromFile("Assets/Models/cube.fbx");
	if (0 == assimp.GetVerticesCount())
		return -1;

	Memory vertices(assimp.GetVerticesCount() * AssimpLoader::VertexSize);
	assimp.FillInVerticesData(vertices.ptr);

	auto vb = api->CreateVertexBuffer(vertices.size, false);
	api->UpdateVertexBuffer(vb, vertices.size, reinterpret_cast<void*>(vertices.ptr), AssimpLoader::VertexSize);

	auto ib = api->CreateIndexBuffer(assimp.GetIndicesCount() * sizeof(unsigned int), false);
	api->UpdateIndexBuffer(ib, assimp.GetIndicesCount() * sizeof(unsigned int), reinterpret_cast<const void*>(assimp.GetIndices()), bamboo::TYPE_UINT32);

	auto vs_byte = LoadFile("Assets/Shaders/D3D11/vs_opaque.cso");
	auto ps_byte = LoadFile("Assets/Shaders/D3D11/ps_opaque.cso");

	auto vs_skybox_byte = LoadFile("Assets/Shaders/D3D11/vs_skybox.cso");
	auto ps_skybox_byte = LoadFile("Assets/Shaders/D3D11/ps_skybox.cso");

	if (nullptr == vs_byte.ptr || nullptr == ps_byte.ptr ||
		nullptr == vs_skybox_byte.ptr || nullptr == ps_skybox_byte.ptr)
		return -1;

	auto vs = api->CreateVertexShader(vs_byte.ptr, vs_byte.size);
	auto ps = api->CreatePixelShader(ps_byte.ptr, ps_byte.size);

	auto vs_skybox = api->CreateVertexShader(vs_skybox_byte.ptr, vs_skybox_byte.size);
	auto ps_skybox = api->CreatePixelShader(ps_skybox_byte.ptr, ps_skybox_byte.size);

	bamboo::VertexLayout layout = {};
	layout.ElementCount = 4;
	layout.Elements[0] =
	{
		bamboo::SEMANTIC_POSITION, 3 - 1 /* 0~3 stands for 1~4 */, bamboo::TYPE_FLOAT, 0, 0
	};
	layout.Elements[1] =
	{
		bamboo::SEMANTIC_NORMAL, 3 - 1 /* 0~3 stands for 1~4 */, bamboo::TYPE_FLOAT, 0, 0
	};
	layout.Elements[2] =
	{
		bamboo::SEMANTIC_TANGENT, 3 - 1 /* 0~3 stands for 1~4 */, bamboo::TYPE_FLOAT, 0, 0
	};
	layout.Elements[3] =
	{
		bamboo::SEMANTIC_TEXCOORD0, 2 - 1 /* 0~3 stands for 1~4 */, bamboo::TYPE_FLOAT, 0, 0
	};

	auto vl = api->CreateVertexLayout(layout);

	Memory frameConstants(sizeof(XMFLOAT4X4) * 2);
	Memory instanceConstants(sizeof(XMFLOAT4X4) * 2);


	camera.SetPosition(0, 0, -5.0f);
	camera.SetPerspective((float)win.GetWidth() / win.GetHeight(), 0.25f * XM_PI, 0.1f, 100.0f);

	*reinterpret_cast<XMFLOAT4X4*>(frameConstants.ptr) = camera.GetViewMatrix();
	*(reinterpret_cast<XMFLOAT4X4*>(frameConstants.ptr) + 1) = camera.GetProjectionMatrix();

	XMStoreFloat4x4(
		reinterpret_cast<XMFLOAT4X4*>(instanceConstants.ptr),
		XMMatrixIdentity()
	);
	XMStoreFloat4x4(
		reinterpret_cast<XMFLOAT4X4*>(instanceConstants.ptr) + 1,
		XMMatrixIdentity()
	);

	auto cb1 = api->CreateConstantBuffer(frameConstants.size);
	api->UpdateConstantBuffer(cb1, frameConstants.size, frameConstants.ptr);

	auto cb2 = api->CreateConstantBuffer(instanceConstants.size);
	api->UpdateConstantBuffer(cb2, instanceConstants.size, instanceConstants.ptr);

	auto cubemap = api->CreateTexture(L"Assets/Textures/skybox.dds");

	auto tex = api->CreateTexture(L"Assets/Textures/wood.jpg");

	auto sampler = api->CreateSampler();

	bamboo::PipelineState state = {};

	state.VertexBufferCount = 1;
	state.HasIndexBuffer = 1;
	state.HasVertexShader = 1;
	state.HasPixelShader = 1;

	state.VertexBuffers[0] = vb;
	state.IndexBuffer = ib;

	state.VertexShader = vs;
	state.PixelShader = ps;

	state.VertexLayout = vl;

	state.CullMode = bamboo::CULL_BACK;

	state.ConstantBufferCount = 2;
	state.ConstantBuffers[0] = { cb1, { 1, 0, 0} };
	state.ConstantBuffers[1] = { cb2, { 1, 0, 0 } };

	state.TextureCount = 1;
	state.Textures[0] = { tex, {0, 1, 0} };

	state.SamplerCount = 1;
	state.Samplers[0] = { sampler, {0, 1, 0} };

	state.Viewport = { 0, 0, 800, 600, 0.0f, 1.0f };

	state.DepthEnable = 1;
	state.DepthWrite = 1;
	state.DepthFunc = bamboo::COMPARISON_LESS;

	state.PrimitiveType = bamboo::PRIMITIVE_TRIANGLES;

	bamboo::PipelineState stateSkyBox = state;
	stateSkyBox.VertexShader = vs_skybox;
	stateSkyBox.PixelShader = ps_skybox;
	stateSkyBox.CullMode = bamboo::CULL_FRONT;
	stateSkyBox.ConstantBufferCount = 1;
	stateSkyBox.Textures[0] = { cubemap, {0, 1, 0} };
	stateSkyBox.DepthFunc = bamboo::COMPARISON_LESS_EQUAL;

	float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	auto invalidRTHandle = bamboo::RenderTargetHandle{ bamboo::invalid_handle }; // TODO

	float pitch = 0.0f, yaw = 0.0f;
	timer.Start();

	while (win.ProcessEvent())
	{
		timer.Update();

		{
			auto m = mouse->GetState();
			if (m.positionMode == Mouse::MODE_RELATIVE)
			{
				constexpr float sensitive = 0.001f;
				pitch += m.y * sensitive;
				yaw += m.x * sensitive;

				float limit = XM_PI / 2.0f - 0.01f;
				pitch = max(-limit, pitch);
				pitch = min(+limit, pitch);

				// keep longitude in sane range by wrapping
				if (yaw > XM_PI)
				{
					yaw -= XM_PI * 2.0f;
				}
				else if (yaw < -XM_PI)
				{
					yaw += XM_PI * 2.0f;
				}
			}
			mouse->SetMode(m.leftButton ? Mouse::MODE_RELATIVE : Mouse::MODE_ABSOLUTE);

			camera.SetRotation(yaw, pitch);

			auto kb = keyboard->GetState();
			if (kb.Escape)
				PostQuitMessage(0);

			constexpr float speed = 4.0f;
			float fw = 0.0f, rt = 0.0f, up = 0.0f;
			if (kb.W)
			{
				fw += speed;
			}
			if (kb.S)
			{
				fw -= speed;
			}
			if (kb.D)
			{
				rt += speed;
			}
			if (kb.A)
			{
				rt -= speed;
			}
			if (kb.Space)
			{
				up += speed;
			}
			if (kb.LeftShift)
			{
				up -= speed;
			}

			camera.MoveAlongDirection(fw * timer.deltaTime, rt * timer.deltaTime, 0.0f);
			camera.MoveAlongWorldAxes(0.0f, up * timer.deltaTime, 0.0f);
		}

		{
			*reinterpret_cast<XMFLOAT4X4*>(frameConstants.ptr) = camera.GetViewMatrix();
			*(reinterpret_cast<XMFLOAT4X4*>(frameConstants.ptr) + 1) = camera.GetProjectionMatrix();

			api->UpdateConstantBuffer(cb1, frameConstants.size, frameConstants.ptr);
		}

		api->Clear(invalidRTHandle, clearColor);
		api->ClearDepthStencil(invalidRTHandle, 1.0f, 0);

		api->DrawIndex(state, static_cast<uint32_t>(assimp.GetIndicesCount()));

		// skybox
		api->DrawIndex(stateSkyBox, static_cast<uint32_t>(assimp.GetIndicesCount()));

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