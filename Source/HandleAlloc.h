#pragma once

#include "common.h"

namespace bamboo
{
	// This handle allocator is adopted from bgfx (https://github.com/bkaradzic/bgfx)
	template<size_t size>
	class HandleAlloc
	{
		constexpr uint16 invalid = 0;
	public:

		HandleAlloc()
		{
			Reset();
		}

		void Reset()
		{
			for (uint16_t i = 0; i < size; ++i) handles[i] = i;
			handleCount = 0;
		}

		uint16_t Alloc()
		{
			if (handleCount < size)
			{
				size_t index = handleCount;
				++handleCount;

				uint16_t id = handles[index];
				indices[id] = static_cast<uint16>(index);

				return id;
			}

			return invalid;
		}

		void Free(uint16_t handle)
		{
			// switch the handle we are freeing with the last handle allocated;
			if (handleCount == 0) return;

			--handleCount;
			size_t index = indices[handle];
			size_t last_index = handleCount;
			uint16_t last_handle = handles[last_index];

			handles[index] = last_handle;
			indices[last_handle] = index;

			handles[last_index] = handle;
		}

		bool InUse(uint16_t handle) const
		{
			if (handle >= size) return false;

			size_t index = indices[handle];
			return index < handleCount && handles[index] = handle;
		}

	private:
		uint16_t 	handles[size];		// handles[i] - id of the i-th handle
		uint16_t	indices[size];		// indices[i] - index of the handle whose id == i;
		size_t		handleCount;
	};
}
