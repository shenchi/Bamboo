#include "NativeWindow.h"

#include <Windows.h>

namespace
{
	wchar_t g_WindowClassName[] = L"BambooNativeWindow";

	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}

		return 0;
	}
}

namespace bamboo
{
	namespace win32
	{

		struct NativeWindow::Context
		{
			HWND hWnd;
			HINSTANCE hInstance;
		};

		NativeWindow::NativeWindow(const wchar_t* title, uint32_t w, uint32_t h)
			:
			data(new Context{ 0 }),
			width(w), height(h)
		{
			HINSTANCE hInstance = GetModuleHandle(nullptr);

			WNDCLASSEX cls{ 0 };
			cls.cbSize = sizeof(WNDCLASSEX);
			cls.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
			cls.hInstance = hInstance;
			cls.lpfnWndProc = WndProc;
			cls.lpszClassName = g_WindowClassName;
			cls.style = CS_HREDRAW | CS_VREDRAW;

			if (!RegisterClassEx(&cls))
			{
				delete data;
				data = nullptr;
				return;
			}

			DWORD winStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

			RECT windowRect = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };
			AdjustWindowRect(&windowRect, winStyle, FALSE);

			RECT screenRect;
			GetClientRect(GetDesktopWindow(), &screenRect);
			int windowX = screenRect.right / 2 - (windowRect.right - windowRect.left) / 2;
			int windowY = screenRect.bottom / 2 - (windowRect.bottom - windowRect.top) / 2;

			HWND hWnd = CreateWindow(
				g_WindowClassName,
				title,
				winStyle,
				windowX, windowY,
				windowRect.right - windowRect.left,
				windowRect.bottom - windowRect.top,
				nullptr,
				nullptr,
				hInstance,
				nullptr/*reinterpret_cast<void*>(this)*/);

			if (!hWnd)
			{
				UnregisterClass(g_WindowClassName, hInstance);
				delete data;
				data = nullptr;
				return;
			}

			data->hInstance = hInstance;
			data->hWnd = hWnd;

			ShowWindow(data->hWnd, SW_SHOW);
		}

		NativeWindow::~NativeWindow()
		{
			if (nullptr != data)
			{
				UnregisterClass(g_WindowClassName, data->hInstance);
				delete data;
				data = nullptr;
			}
		}

		bool NativeWindow::ProcessEvent()
		{
			MSG msg;
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
				{
					return false;
				}
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			return true;
		}

		void* NativeWindow::GetHandle()
		{
			return reinterpret_cast<void*>(data->hWnd);
		}

	}
}