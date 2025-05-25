#ifndef CORE_MISC_H
#define CORE_MISC_H
#include "../prelude.h"


int IRAM os_get_cpu_frequency(void);
void os_update_cpu_frequency(int freq);
void os_delay_us(u16 us);
void os_install_putc1(void (*p)(char));
void os_putc(char c);
void gpio_output_set(u32 set_mask,u32 clear_mask,u32 enable_mask,u32 disable_mask);
u8 rtc_get_reset_reason(void);


#endif
