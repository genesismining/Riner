#pragma once

// delete copy constructor and copy assignment operator for a given class C
#define DELETE_COPY(C) \
  C(const C&) = delete; \
  C& operator=(const C&) = delete

// delete copy/move constructors and copy/move assignment operators for a given class C
#define DELETE_COPY_AND_MOVE(C) \
    C(const C &) = delete; \
    C(C &&) = delete; \
    C &operator=(const C &) = delete; \
    C &operator=(C &&) = delete;
