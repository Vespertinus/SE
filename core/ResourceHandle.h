
#ifndef RESOURCE_HANDLE_H
#define RESOURCE_HANDLE_H 1

#include <cstdint>
#include <limits>

namespace SE {

struct ResourceHandle {
        uint32_t index      { std::numeric_limits<uint32_t>::max() };
        uint32_t generation { 0 };

        bool IsValid() const { return index != std::numeric_limits<uint32_t>::max(); }
        bool operator==(const ResourceHandle & o) const { return index == o.index && generation == o.generation; }

        static constexpr ResourceHandle Null() { return {}; }
};

template <class T> struct Handle {
        ResourceHandle raw;
        bool IsValid() const { return raw.IsValid(); }
        static Handle Null()  { return { ResourceHandle::Null() }; }
        bool operator==(const Handle & o) const { return raw == o.raw; }
};

template <typename T> using H = Handle<T>;

} // namespace SE

#endif
