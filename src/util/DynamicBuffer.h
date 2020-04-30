
#pragma once

#include <src/common/Span.h>
#include <gsl/gsl>

#include <type_traits>

namespace riner {

    /**
     * simple replacement for std::vector<char> in situations where huge
     * allocations happen, since the standard doesn't define how
     * much more memory std::vector allocates on reserve or resize.
     *
     * basically an owning gsl::span
     */
    template<class T = uint8_t>
    class DynamicBuffer {

        gsl::owner<T*> owner = nullptr;
        span<T> buffer;

    public:

        DynamicBuffer() = default;
        // only use alignment if a stronger power of two alignment than the natural alignment of T is needed
        DynamicBuffer(size_t size, size_t alignment = alignof(T))
            : owner(reinterpret_cast<T*>(::operator new(size * sizeof(T) + alignment - 1)))
            , buffer(reinterpret_cast<T*>((uintptr_t(owner) + alignment - 1) & ~uintptr_t(alignment - 1))
                , ptrdiff_t(size)) {
            if (!std::is_trivially_constructible<T>::value) {
                new(buffer.data()) T[size];
            }
        }

        ~DynamicBuffer() {
            if (owner) { //if not moved-from
                if (!std::is_trivially_destructible<T>::value) {
                    for (auto &element : buffer) {
                        element.~T();
                    }
                }
                ::operator delete(owner);
            }
        }

        DynamicBuffer(DynamicBuffer &&o) noexcept
            : owner(o.owner)
            , buffer(o.buffer) {
            o.owner = nullptr; //remove ownership
        }

        DynamicBuffer &operator=(DynamicBuffer &&o) noexcept {
            owner = o.owner;
            buffer = o.buffer;
            o.owner = nullptr; //remove ownership
            return *this;
        }

        //copy
        DynamicBuffer(const DynamicBuffer &) = delete;
        DynamicBuffer &operator=(const DynamicBuffer &) = delete;

        T *data() {
            return buffer.data();
        }

        uint8_t *bytes() {
            return reinterpret_cast<uint8_t*>(buffer.data());
        }

        const T *data() const {
            return buffer.data();
        }

        const uint8_t *bytes() const {
            return reinterpret_cast<const uint8_t*>(buffer.data());
        }

        size_t size_bytes() const {
            return static_cast<size_t>(buffer.size_bytes());
        }

        size_t size() const {
            return static_cast<size_t>(buffer.size());
        }

        span<const T> getSpan() const {
            return buffer;
        }

        span<const uint8_t> getByteSpan() const {
            return {bytes(), static_cast<ptrdiff_t>(size_bytes())};
        }

        operator bool() const {
            return owner != nullptr;
        }

    };

}
