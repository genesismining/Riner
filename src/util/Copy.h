#pragma once

#define DELETE_COPY(C) \
  C(const C&) = delete; \
  C& operator=(const C&) = delete

#define DELETE_COPY_AND_MOVE(C) \
    C(const C &) = delete; \
    C(C &&) = delete; \
    C &operator=(const C &) = delete; \
    C &operator=(C &&) = delete;
