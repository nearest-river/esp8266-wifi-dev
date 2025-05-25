#ifndef OS_XTENSA_H
#define OS_XTENSA_H


#define SP(var) asm volatile ("mov %0, a1" : "=r" (var))
#define RSR(var, reg) asm volatile ("rsr %0, " #reg : "=r" (var));
#define WSR(var, reg) asm volatile ("wsr %0, " #reg : : "r" (var));
#define ESYNC() asm volatile ("esync")
#define XSR(var, reg) asm volatile ("xsr %0, " #reg : "+r" (var));




#endif // !OS_XTENSA_H
