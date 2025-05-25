#include "../prelude.h"

never_inline
never
void panic(void);

inline
void abort() {
  panic();
}


