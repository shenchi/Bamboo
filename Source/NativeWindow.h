#pragma once

#include "common.h"

namespace bamboo
{
	namespace win32
	{
		class NativeWindow
		{
		public:

			NativeWindow(const wchar_t* title, uint32_t w, uint32_t h);

			NativeWindow(const NativeWindow&) = delete;

			~NativeWindow();

			bool ProcessEvent();

			inline operator bool() const { return nullptr != data; }

			inline uint32_t GetWidth() const { return width; }

			inline uint32_t GetHeight() const { return height; }

			void* GetHandle();

		private:
			struct Context;
			Context* data;

			uint32_t width, height;
		};
	}
}
