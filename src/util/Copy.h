#pragma once

#define DELETE_COPY_AND_ASSIGNMENT(ClassName) \
  ClassName(const ClassName&) = delete; \
  ClassName& operator=(const ClassName&) = delete

