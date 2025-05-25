#ifndef LOCK_H
#define LOCK_H


typedef int _lock_t;

void _lock_init(_lock_t* lock);
void _lock_init_recursive(_lock_t* lock);




#endif // !LOCK_H
