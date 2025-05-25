#ifndef HWRAND_H
#define HWRAND_H
#include "prelude.h"



/* Return a random 32-bit number */
u32 hwrand(void);

/* Fill a variable size buffer with data from the Hardware RNG */
void hwrand_fill(u8* buf,usize len);


#endif
