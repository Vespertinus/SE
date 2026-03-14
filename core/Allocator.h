#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__ 1

#include <cstddef>
#include <cstdint>

namespace SE {

inline uint8_t* align_up(uint8_t* ptr, size_t align) noexcept {
        auto val = reinterpret_cast<uintptr_t>(ptr);
        return reinterpret_cast<uint8_t*>((val + align - 1) & ~(align - 1));
}

inline size_t align_up(size_t val, size_t align) noexcept {
        return (val + align - 1) & ~(align - 1);
}

constexpr size_t align_up_constexpr(size_t val, size_t align) noexcept {
        return (val + align - 1) & ~(align - 1);
}

class FrameAllocator {
        uint8_t* base_;
        uint8_t* current_;
        size_t   capacity_;
        size_t   high_water_;

public:
        FrameAllocator();
        explicit FrameAllocator(size_t capacity);
        ~FrameAllocator() noexcept;
        FrameAllocator(const FrameAllocator&) = delete;
        FrameAllocator& operator=(const FrameAllocator&) = delete;

        void*  allocate(size_t size, size_t align = 16);
        void   reset() noexcept;
        size_t used()       const noexcept;
        size_t capacity()   const noexcept;
        size_t high_water() const noexcept;
};

class StackAllocator {
        uint8_t* base_;
        uint8_t* top_;
        size_t   capacity_;
        size_t   high_water_;

public:
        using Marker = size_t;

        explicit StackAllocator(size_t capacity);
        ~StackAllocator() noexcept;
        StackAllocator(const StackAllocator&) = delete;
        StackAllocator& operator=(const StackAllocator&) = delete;

        void*  allocate(size_t size, size_t align = 16);
        Marker get_marker()                    const noexcept;
        void   free_to_marker(Marker m)              noexcept;
        size_t used()                          const noexcept;
        size_t capacity()                      const noexcept;
        size_t high_water()                    const noexcept;
};

template<size_t ObjectSize, size_t Alignment = 16>
class PoolAllocator {
        struct FreeNode { FreeNode* next; };

        static constexpr size_t kSlotSize = align_up_constexpr(
                ObjectSize >= sizeof(void*) ? ObjectSize : sizeof(void*), Alignment);

        uint8_t*  memory_;
        FreeNode* free_list_;
        size_t    capacity_;
        size_t    allocated_;
        size_t    high_water_;

public:
        explicit PoolAllocator(size_t slot_count);
        ~PoolAllocator() noexcept;
        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        void*  allocate();
        void   deallocate(void* ptr) noexcept;
        size_t capacity()   const noexcept;
        size_t allocated()  const noexcept;
        size_t high_water() const noexcept;
        bool   full()       const noexcept;
};

} //namespace SE

#endif
