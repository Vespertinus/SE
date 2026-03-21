
#ifndef __SPSC_QUEUE_H__
#define __SPSC_QUEUE_H__ 1

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace SE {

/**
 * Lock-free single-producer / single-consumer ring buffer.
 * Capacity must be a power of two.
 * Uses acquire/release ordering only — no sequentially consistent ops.
 */
template <class T, uint32_t Capacity> class SPSCQueue {

        static_assert((Capacity & (Capacity - 1)) == 0,
                        "SPSCQueue: Capacity must be a power of two");
        static_assert(std::is_copy_assignable<T>::value,
                        "SPSCQueue: T must be copy assignable");

        static constexpr uint32_t MASK = Capacity - 1;

        struct alignas(64) AlignedAtomic {
                std::atomic<uint32_t> value{0};
        };

        AlignedAtomic oHead;   // written by consumer, read by producer
        AlignedAtomic oTail;   // written by producer, read by consumer
        T             vBuf[Capacity];

public:

        SPSCQueue() = default;
        SPSCQueue(const SPSCQueue&) = delete;
        SPSCQueue& operator=(const SPSCQueue&) = delete;

        /** Producer: returns false if the queue is full. */
        bool TryPush(const T& item) {
                const uint32_t t = oTail.value.load(std::memory_order_relaxed);
                const uint32_t next = (t + 1) & MASK;
                if (next == oHead.value.load(std::memory_order_acquire))
                        return false;  // full
                vBuf[t] = item;
                oTail.value.store(next, std::memory_order_release);
                return true;
        }

        /** Consumer: returns false if the queue is empty. */
        bool TryPop(T& out) {
                const uint32_t h = oHead.value.load(std::memory_order_relaxed);
                if (h == oTail.value.load(std::memory_order_acquire))
                        return false;  // empty
                out = vBuf[h];
                oHead.value.store((h + 1) & MASK, std::memory_order_release);
                return true;
        }

        bool Empty() const {
                return oHead.value.load(std::memory_order_acquire) ==
                        oTail.value.load(std::memory_order_acquire);
        }
};

} // namespace SE

#endif
