#include "../prelude.h"
#include "../xtensa/instructions.h"
#include "../uart.h"
#include "gpio_regs.h"
#include "rtc_regs.h"


static int cpu_freq=80;

void (*_putc1)(char);

int IRAM os_get_cpu_frequency(void) {
  return cpu_freq;
}

void os_update_cpu_frequency(int freq) {
  cpu_freq = freq;
}

void ets_update_cpu_frequency(int freq) __attribute__ (( alias ("os_update_cpu_frequency") ));

void os_delay_us(u16 us) {
  u32 ccount;
  u32 start_ccount;
  u32 delay_ccount=cpu_freq*us;

  RSR(start_ccount, ccount);

  do {
    RSR(ccount, ccount);
  } while (ccount - start_ccount < delay_ccount);
}

void ets_delay_us(u16 us) __attribute__ (( alias ("os_delay_us") ));

void os_install_putc1(void (*p)(char)) {
  _putc1 = p;
}

void os_putc(char c) {
  _putc1(c);
}

void gpio_output_set(u32 set_mask,u32 clear_mask,u32 enable_mask,u32 disable_mask) {
  GPIO.OUT_SET = set_mask;
  GPIO.OUT_CLEAR = clear_mask;
  GPIO.ENABLE_OUT_SET = enable_mask;
  GPIO.ENABLE_OUT_CLEAR = disable_mask;
}

u8 rtc_get_reset_reason(void) {
  u8 reason;

  reason = FIELD2VAL(RTC_RESET_REASON1_CODE, RTC.RESET_REASON1);
  if(reason == 5) {
    if(FIELD2VAL(RTC_RESET_REASON2_CODE, RTC.RESET_REASON2) == 1) {
      reason = 6;
    } else {
      if (FIELD2VAL(RTC_RESET_REASON2_CODE, RTC.RESET_REASON2) != 8) {
        reason = 0;
      }
    }
  }
  RTC.RESET_REASON0 &= ~RTC_RESET_REASON0_BIT21;
  return reason;
}
