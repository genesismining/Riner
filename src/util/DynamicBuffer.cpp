
#include "DynamicBuffer.h"

namespace miner {

    DynamicBuffer::DynamicBuffer(size_t size)
    : owner(new value_type[size])
    , buffer(owner, static_cast<int64_t>(size)) {
    }

    DynamicBuffer::~DynamicBuffer() {
        if (owner) //if not moved-from
            delete[] owner;
    }

    DynamicBuffer::DynamicBuffer(DynamicBuffer &&o) noexcept
    : owner(o.owner)
    , buffer(o.buffer) {
        o.owner = nullptr; //remove ownership
    }

    DynamicBuffer &DynamicBuffer::operator=(DynamicBuffer &&o) noexcept {
        owner = o.owner;
        buffer = o.buffer;
        o.owner = nullptr; //remove ownership
        return *this;
    }


}