
use core::ffi::c_int as int;

#[repr(C)]
struct MallInfo {
  arena: int,
  ordblks: int
}



/*
typedef struct mallinfo {
  int arena;    /* non-mmapped space allocated from system */
  int ordblks;  /* number of free chunks */
  int smblks;   /* number of fastbin blocks */
  int hblks;    /* number of mmapped regions */
  int hblkhd;   /* space in mmapped regions */
  int usmblks;  /* always 0, preserved for backwards compatibility */
  int fsmblks;  /* space available in freed fastbin blocks */
  int uordblks; /* total allocated space */
  int fordblks; /* total free space */
  int keepcost; /* top-most, releasable (via malloc_trim) space */
} MallInfo;

/* SVID2/XPG mallinfo2 structure which can handle allocations
   bigger than 4GB.  */

typedef struct mallinfo2 {
  usize arena;    /* non-mmapped space allocated from system */
  usize ordblks;  /* number of free chunks */
  usize smblks;   /* number of fastbin blocks */
  usize hblks;    /* number of mmapped regions */
  usize hblkhd;   /* space in mmapped regions */
  usize usmblks;  /* always 0, preserved for backwards compatibility */
  usize fsmblks;  /* space available in freed fastbin blocks */
  usize uordblks; /* total allocated space */
  usize fordblks; /* total free space */
  usize keepcost; /* top-most, releasable (via malloc_trim) space */
} MallInfo2;

*/










