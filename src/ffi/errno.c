

_Atomic static int _errno=0;

_Atomic int* __errno(void) {
  return &_errno;
}


