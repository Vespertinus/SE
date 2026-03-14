
#ifdef SE_IMPL

#include <new>

namespace SE {

// ── FrameAllocator ────────────────────────────────────────────────────────────

static constexpr size_t kFrameAllocatorDefaultCapacity = 4 * 1024 * 1024; // 4 MB

FrameAllocator::FrameAllocator()
        : FrameAllocator(kFrameAllocatorDefaultCapacity) {}

FrameAllocator::FrameAllocator(size_t capacity)
        : base_(static_cast<uint8_t*>(::operator new(capacity, std::align_val_t{16})))
        , current_(base_)
        , capacity_(capacity)
        , high_water_(0) {}

FrameAllocator::~FrameAllocator() noexcept {
        log_d("FrameAllocator destroyed, high_water=%zu bytes", high_water_);
        ::operator delete(base_, capacity_, std::align_val_t{16});
}

void* FrameAllocator::allocate(size_t size, size_t align) {
        uint8_t* aligned = align_up(current_, align);
        uint8_t* next    = aligned + size;
        if (next > base_ + capacity_) {
                log_e("FrameAllocator exhausted, used=%zu, capacity=%zu", used(), capacity_);
                se_assert(false);
        }
        current_ = next;
        size_t w = static_cast<size_t>(current_ - base_);
        if (w > high_water_) high_water_ = w;
        return aligned;
}

void FrameAllocator::reset() noexcept {
        current_ = base_;
}

size_t FrameAllocator::used()       const noexcept { return static_cast<size_t>(current_ - base_); }
size_t FrameAllocator::capacity()   const noexcept { return capacity_; }
size_t FrameAllocator::high_water() const noexcept { return high_water_; }

// ── StackAllocator ────────────────────────────────────────────────────────────

StackAllocator::StackAllocator(size_t capacity)
        : base_(static_cast<uint8_t*>(::operator new(capacity, std::align_val_t{16})))
        , top_(base_)
        , capacity_(capacity)
        , high_water_(0) {}

StackAllocator::~StackAllocator() noexcept {
        ::operator delete(base_, capacity_, std::align_val_t{16});
}

void* StackAllocator::allocate(size_t size, size_t align) {
        uint8_t* aligned = align_up(top_, align);
        uint8_t* next    = aligned + size;
        if (next > base_ + capacity_) {
                log_e("StackAllocator exhausted, used=%zu, capacity=%zu", used(), capacity_);
                se_assert(false);
        }
        top_ = next;
        size_t w = static_cast<size_t>(top_ - base_);
        if (w > high_water_) high_water_ = w;
        return aligned;
}

StackAllocator::Marker StackAllocator::get_marker() const noexcept {
        return static_cast<Marker>(top_ - base_);
}

void StackAllocator::free_to_marker(Marker m) noexcept {
        se_assert(m <= capacity_);
        top_ = base_ + m;
}

size_t StackAllocator::used()       const noexcept { return static_cast<size_t>(top_ - base_); }
size_t StackAllocator::capacity()   const noexcept { return capacity_; }
size_t StackAllocator::high_water() const noexcept { return high_water_; }

} //namespace SE

#endif // SE_IMPL

// ── PoolAllocator (template — no SE_IMPL guard) ───────────────────────────────

#include <new>

namespace SE {

template<size_t ObjectSize, size_t Alignment>
PoolAllocator<ObjectSize, Alignment>::PoolAllocator(size_t slot_count)
        : memory_(static_cast<uint8_t*>(::operator new(slot_count * kSlotSize, std::align_val_t{16})))
        , free_list_(nullptr)
        , capacity_(slot_count)
        , allocated_(0)
        , high_water_(0) {
        // build free list
        for (size_t i = 0; i + 1 < slot_count; ++i) {
                auto* node  = reinterpret_cast<FreeNode*>(memory_ + i * kSlotSize);
                node->next  = reinterpret_cast<FreeNode*>(memory_ + (i + 1) * kSlotSize);
        }
        if (slot_count > 0) {
                auto* last = reinterpret_cast<FreeNode*>(memory_ + (slot_count - 1) * kSlotSize);
                last->next = nullptr;
                free_list_ = reinterpret_cast<FreeNode*>(memory_);
        }
}

template<size_t ObjectSize, size_t Alignment>
PoolAllocator<ObjectSize, Alignment>::~PoolAllocator() noexcept {
        ::operator delete(memory_, capacity_ * kSlotSize, std::align_val_t{16});
}

template<size_t ObjectSize, size_t Alignment>
void* PoolAllocator<ObjectSize, Alignment>::allocate() {
        if (!free_list_) {
                log_e("PoolAllocator exhausted, capacity=%zu slots", capacity_);
                se_assert(false);
        }
        FreeNode* node = free_list_;
        free_list_ = node->next;
        ++allocated_;
        if (allocated_ > high_water_) high_water_ = allocated_;
        return node;
}

template<size_t ObjectSize, size_t Alignment>
void PoolAllocator<ObjectSize, Alignment>::deallocate(void* ptr) noexcept {
        auto* node  = reinterpret_cast<FreeNode*>(ptr);
        node->next  = free_list_;
        free_list_  = node;
        --allocated_;
}

template<size_t ObjectSize, size_t Alignment>
size_t PoolAllocator<ObjectSize, Alignment>::capacity()   const noexcept { return capacity_; }

template<size_t ObjectSize, size_t Alignment>
size_t PoolAllocator<ObjectSize, Alignment>::allocated()  const noexcept { return allocated_; }

template<size_t ObjectSize, size_t Alignment>
size_t PoolAllocator<ObjectSize, Alignment>::high_water() const noexcept { return high_water_; }

template<size_t ObjectSize, size_t Alignment>
bool   PoolAllocator<ObjectSize, Alignment>::full()       const noexcept { return free_list_ == nullptr; }

} //namespace SE
