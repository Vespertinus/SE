
#define SE_IMPL
#include <gtest/gtest.h>
#include <Logging.h>
#include <Allocator.h>
#include <Allocator.tcc>

// ── FrameAllocator ─────────────────────────────────────────────────────────

TEST(FrameAllocator, BasicAllocAndReset) {
        SE::FrameAllocator alloc(1024);

        EXPECT_EQ(alloc.used(), 0u);

        void* p = alloc.allocate(64);
        EXPECT_NE(p, nullptr);
        EXPECT_EQ(alloc.used(), 64u);

        alloc.reset();
        EXPECT_EQ(alloc.used(), 0u);
}

TEST(FrameAllocator, AlignmentRespected) {
        SE::FrameAllocator alloc(1024);

        alloc.allocate(1);                          // advance by 1 byte
        void* p = alloc.allocate(4, 16);            // next alloc must be 16-byte aligned
        EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 16, 0u);
}

TEST(FrameAllocator, HighWaterNeverDecreases) {
        SE::FrameAllocator alloc(1024);

        alloc.allocate(128);
        size_t peak = alloc.high_water();
        EXPECT_GE(peak, 128u);

        alloc.reset();
        EXPECT_EQ(alloc.high_water(), peak);        // reset must not lower high_water

        alloc.allocate(32);                         // smaller than previous peak
        EXPECT_EQ(alloc.high_water(), peak);
}

TEST(FrameAllocator, UsedPlusRemainingEqualsCapacity) {
        SE::FrameAllocator alloc(256);

        EXPECT_LE(alloc.used(), alloc.capacity());

        alloc.allocate(100);
        EXPECT_LE(alloc.used(), alloc.capacity());

        alloc.allocate(50);
        EXPECT_LE(alloc.used(), alloc.capacity());
}

// ── StackAllocator ─────────────────────────────────────────────────────────

TEST(StackAllocator, BasicMarkerRewind) {
        SE::StackAllocator alloc(1024);

        SE::StackAllocator::Marker m = alloc.get_marker();
        EXPECT_EQ(m, 0u);

        alloc.allocate(64);
        EXPECT_EQ(alloc.used(), 64u);

        alloc.free_to_marker(m);
        EXPECT_EQ(alloc.used(), 0u);
}

TEST(StackAllocator, AlignmentRespected) {
        SE::StackAllocator alloc(1024);

        alloc.allocate(1);                          // advance by 1 byte
        void* p = alloc.allocate(4, 16);            // next alloc must be 16-byte aligned
        EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 16, 0u);
}

TEST(StackAllocator, HighWaterNeverDecreases) {
        SE::StackAllocator alloc(1024);

        alloc.allocate(200);
        size_t peak = alloc.high_water();
        EXPECT_GE(peak, 200u);

        alloc.free_to_marker(0);
        EXPECT_EQ(alloc.used(), 0u);
        EXPECT_EQ(alloc.high_water(), peak);        // free does not lower high_water
}

TEST(StackAllocator, NestedMarkers) {
        SE::StackAllocator alloc(1024);

        alloc.allocate(32);
        SE::StackAllocator::Marker m1 = alloc.get_marker(); // 32
        alloc.allocate(64);
        SE::StackAllocator::Marker m2 = alloc.get_marker(); // 96

        EXPECT_EQ(alloc.used(), 96u);

        alloc.free_to_marker(m2);
        EXPECT_EQ(alloc.used(), 96u);               // same position as m2

        alloc.free_to_marker(m1);
        EXPECT_EQ(alloc.used(), 32u);               // back to m1
}

// ── PoolAllocator ──────────────────────────────────────────────────────────

TEST(PoolAllocator, BasicAllocDealloc) {
        SE::PoolAllocator<32> pool(4);

        EXPECT_EQ(pool.allocated(), 0u);
        EXPECT_EQ(pool.capacity(),  4u);

        void* p = pool.allocate();
        EXPECT_NE(p, nullptr);
        EXPECT_EQ(pool.allocated(), 1u);

        pool.deallocate(p);
        EXPECT_EQ(pool.allocated(), 0u);
}

TEST(PoolAllocator, ReuseAfterFree) {
        SE::PoolAllocator<32> pool(1);

        void* p1 = pool.allocate();
        pool.deallocate(p1);
        void* p2 = pool.allocate();
        EXPECT_EQ(p1, p2);
}

TEST(PoolAllocator, FullWhenExhausted) {
        SE::PoolAllocator<32> pool(3);

        void* a = pool.allocate();
        void* b = pool.allocate();
        void* c = pool.allocate();

        EXPECT_TRUE(pool.full());

        pool.deallocate(b);
        EXPECT_FALSE(pool.full());

        (void)a; (void)c;
}

TEST(PoolAllocator, HighWaterTracking) {
        SE::PoolAllocator<32> pool(4);

        void* a = pool.allocate();
        void* b = pool.allocate();
        void* c = pool.allocate();
        EXPECT_EQ(pool.high_water(), 3u);

        pool.deallocate(a);
        pool.deallocate(b);
        EXPECT_EQ(pool.allocated(), 1u);
        EXPECT_EQ(pool.high_water(), 3u);           // peak must not decrease

        (void)c;
}

TEST(PoolAllocator, AlignmentRespected) {
        // Use Alignment=64 to catch the {16} bug
        SE::PoolAllocator<32, 64> pool(8);

        for (int i = 0; i < 8; ++i) {
                void* p = pool.allocate();
                EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 64, 0u)
                        << "slot " << i << " is not 64-byte aligned";
        }
}
