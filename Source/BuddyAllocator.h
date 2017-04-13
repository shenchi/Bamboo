#pragma once

#include <cassert>

namespace bamboo
{
	namespace memory
	{
		/*
		This struct is actually only for the tree data, which is an array form
		of complete binary tree. Each node is 2 bits in width (for 4 states of
		the node). The space it is managing is actually imaginary to it.
		*/
		template<unsigned int memSize, unsigned int minSize>
		struct BuddyAllocator
		{
			// size of tree data
			static constexpr unsigned int treeSize = memSize / minSize / 8 * 2;

			static constexpr unsigned int flag_unused = 0;      // the node is free
			static constexpr unsigned int flag_used = 1;        // the node is allocated as a whole
			static constexpr unsigned int flag_split = 2;       // the node is splitted into children, partially allocated
			static constexpr unsigned int flag_split_full = 3;  // the node is splitted into children, and all children are allocated

			unsigned char space[1];

			static BuddyAllocator* create(void* addr)
			{
				BuddyAllocator* obj = reinterpret_cast<BuddyAllocator*>(addr);
				obj->initialize();
				return obj;
			}

			static inline bool is_pow_of_2(unsigned int x)
			{
				return !(x & (x - 1));
			}

			// found at http://www.geeksforgeeks.org/smallest-power-of-2-greater-than-or-equal-to-n/
			static inline unsigned int next_pow_of_2(unsigned int x)
			{
				if (is_pow_of_2(x)) return x;
				x |= x >> 1;
				x |= x >> 2;
				x |= x >> 4;
				x |= x >> 8;
				x |= x >> 16;
				return x + 1;
			}

			// read a node in the tree (array of 2-bit)
			inline unsigned int read(unsigned int index) const
			{
				return (space[index >> 2] >> ((index & 3) << 1)) & 3;
			}

			// write to a node in the tree (array of 2-bit)
			inline void write(unsigned int index, unsigned int flag)
			{
				unsigned int a = (index & 3) << 1;
				(space[index >> 2] &= ~(3 << a)) |= ((flag & 3) << a);
			}

			void initialize()
			{
				// set the root node's flag to flag_unused
				space[0] = 0;
			}

			void* binarySearchAlloc(unsigned int index, unsigned int nodeSize, unsigned int need, unsigned int offset, void* baseAddr)
			{
				unsigned int flag = read(index);
				if (need == nodeSize)
				{
					if (flag == flag_unused)
					{
						write(index, flag_used);

						// trace back and set splitted nodes to flag_split_full
						for (unsigned int i = index; i > 0;)
						{
							unsigned int buddy = ((i + 1) ^ 1) - 1;
							unsigned int buddy_flag = read(buddy);
							i = (i - 1) >> 1;

							if (buddy_flag == flag_used || buddy_flag == flag_split_full)
								write(i, flag_split_full);
							else
								break;
						}

						// the final address of the allocation
						return reinterpret_cast<void*>(reinterpret_cast<char*>(baseAddr) + offset);
					}
				}
				else // need <= nodeSize / 2
				{
					if (flag == flag_unused)
					{
						// split this node
						flag = flag_split;
						write(index, flag_split);
						write(index * 2 + 1, flag_unused);
						write(index * 2 + 2, flag_unused);
					}
					if (flag == flag_split)
					{
						// traverse its children 
						unsigned int halfSize = nodeSize >> 1;
						// left child
						void* ptr = binarySearchAlloc(index * 2 + 1, halfSize, need, offset, baseAddr);
						if (nullptr != ptr) return ptr;
						// right child
						return binarySearchAlloc(index * 2 + 2, halfSize, need, offset + halfSize, baseAddr);
					}
				}
				return nullptr;
			}

			// the base addres of the managed memory is provided here,
			// because it is imaginary to the buddy allocator, we don't
			// keep it. The algorithm is actually operating on offsets.
			void* allocate(void* baseAddr, unsigned int size)
			{
				// align size with power of 2
				if (size < minSize)
					size = minSize;
				else
					size = next_pow_of_2(size);

				if (size > memSize)
					return nullptr;

				return binarySearchAlloc(0, memSize, size, 0, baseAddr);
			}

			void binarySearchDealloc(unsigned int index, unsigned int nodeSize, unsigned int nodeOffset, unsigned int offset)
			{
				int flag = read(index);
				assert(flag != flag_unused);

				if (nodeOffset == offset && flag == flag_used)
				{
					write(index, flag_unused);

					// trace back and merge all free buddies.
					for (unsigned int i = index; i > 0;)
					{
						unsigned int buddy = ((i + 1) ^ 1) - 1;
						unsigned int buddy_flag = read(buddy);
						i = (i - 1) >> 1;

						if (buddy_flag == flag_unused)
							write(i, flag_unused);
						else
							break;
					}

					return;
				}

				unsigned int halfSize = nodeSize >> 1;
				unsigned int half = nodeOffset + halfSize;
				if (offset < half) // left child
					binarySearchDealloc(index * 2 + 1, halfSize, nodeOffset, offset);
				else // right child
					binarySearchDealloc(index * 2 + 2, halfSize, half, offset);
			}

			void deallocate(void* baseAddr, void* addr)
			{
				unsigned int offset = static_cast<unsigned int>(
					reinterpret_cast<char*>(addr) - reinterpret_cast<char*>(baseAddr)
					);

				binarySearchDealloc(0, memSize, 0, offset);
			}

			//unsigned int binarySearchFreeSpace(unsigned int index, unsigned int nodeSize, unsigned int op) const
			//{
			//	int flag = read(index);
			//	if (flag == flag_unused)
			//	{
			//		return nodeSize;
			//	}
			//	else if (flag == flag_split)
			//	{
			//		unsigned int l = binarySearchFreeSpace(index * 2 + 1, nodeSize >> 1, op);
			//		unsigned int r = binarySearchFreeSpace(index * 2 + 2, nodeSize >> 1, op);

			//		// TODO: we can use lambda, if allowed
			//		switch (op)
			//		{
			//		case 1:
			//			return l < r ? r : l;
			//		case 2:
			//			return l > r ? r : l;
			//		}
			//		return l + r;
			//	}
			//	return 0;
			//}

			//unsigned int freeRemaining() const
			//{
			//	return binarySearchFreeSpace(0, memSize, 0);
			//}

			//unsigned int largestFree() const
			//{
			//	return binarySearchFreeSpace(0, memSize, 1);
			//}

			//unsigned int smallestFree() const
			//{
			//	return binarySearchFreeSpace(0, memSize, 2);
			//}
		};
	}
}