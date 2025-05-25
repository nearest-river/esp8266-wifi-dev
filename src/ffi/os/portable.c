#include "portable.h"
#include "../prelude.h"


inline
void* pvPortMalloc(usize size) {
  return malloc(size);
}

inline
void vPortFree(void* ptr) {
  free(ptr);
}








