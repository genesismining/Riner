
#pragma once

#include <src/common/Span.h>
#include <gsl/gsl>

namespace miner {

    //simple replacement for std::vector<char> in situations where huge
    //allocations happen, since the standard doesn't define how
    //much more memory std::vector allocates on reserve or resize.

    //basically an owning gsl::span
    class DynamicBuffer {
        using value_type = ByteSpan<>::value_type;
        static_assert(sizeof(value_type) == 1, "value_type is expected to be 1 byte");

        gsl::owner<value_type *> owner = nullptr;
        ByteSpan<> buffer;

    public:

        DynamicBuffer() = default;
        explicit DynamicBuffer(size_t allocSize); //allocates allocSize bytes
        ~DynamicBuffer();

        //move (ownership transfer without memcopy)
        DynamicBuffer(DynamicBuffer &&) noexcept;
        DynamicBuffer &operator=(DynamicBuffer &&) noexcept;

        //copy
        DynamicBuffer(const DynamicBuffer &) = delete;
        DynamicBuffer &operator=(const DynamicBuffer &) = delete;

        uint8_t *data() {
            return buffer.data();
        }

        const uint8_t *data() const {
            return buffer.data();
        }

        size_t size_bytes() const {
            return static_cast<size_t>(buffer.size_bytes());
        }

        cByteSpan<> getSpan() const {
            return buffer;
        }

        operator bool() const {
            return owner != nullptr;
        }

    };

}