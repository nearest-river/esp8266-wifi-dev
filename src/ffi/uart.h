/** esp/uart.h
 *
 * Configuration of UART registers.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
 */

#ifndef _UART_H
#define _UART_H

#include "prelude.h"

/* Register definitions for the UART peripherals on the ESP8266.
 *
 * There are twp UART devices built into the ESP8266:
 *   UART(0) is at 0x60000000
 *   UART(1) is at 0x60000F00
 *
 * Each device is allocated a block of 64 32-bit registers (256 bytes of
 * address space) to communicate with application code.
 */

#define UART_BASE 0x60000000
#define UART(i) (*(struct UART_REGS *)(usize)(UART_BASE + (i)*0xf00))

#define UART0_BASE UART_BASE
#define UART1_BASE (UART_BASE + 0xf00)

struct UART_REGS {
    u32 volatile FIFO;           // 0x00
    u32 volatile INT_RAW;        // 0x04
    u32 volatile INT_STATUS;     // 0x08
    u32 volatile INT_ENABLE;     // 0x0c
    u32 volatile INT_CLEAR;      // 0x10
    u32 volatile CLOCK_DIVIDER;  // 0x14
    u32 volatile AUTOBAUD;       // 0x18
    u32 volatile STATUS;         // 0x1c
    u32 volatile CONF0;          // 0x20
    u32 volatile CONF1;          // 0x24
    u32 volatile LOW_PULSE;      // 0x28
    u32 volatile HIGH_PULSE;     // 0x2c
    u32 volatile PULSE_COUNT;    // 0x30
    u32 volatile _unused[17];    // 0x34 - 0x74
    u32 volatile DATE;           // 0x78
    u32 volatile ID;             // 0x7c
};

_Static_assert(sizeof(struct UART_REGS) == 0x80, "UART_REGS is the wrong size");

typedef enum {
    UART_STOPBITS_0 = 0b00,
    UART_STOPBITS_1 = 0b01,
    UART_STOPBITS_1_5 = 0b10,
    UART_STOPBITS_2 = 0b11
} UART_StopBits;

typedef enum {
    UART_PARITY_EVEN = 0b0,
    UART_PARITY_ODD = 0b1
} UART_Parity;

typedef enum {
    UART_BYTELENGTH_5 = 0b00,
    UART_BYTELENGTH_6 = 0b01,
    UART_BYTELENGTH_7 = 0b10,
    UART_BYTELENGTH_8 = 0b11,
} UART_ByteLength;

/* Details for FIFO register */

#define UART_FIFO_DATA_M  0x000000ff
#define UART_FIFO_DATA_S  0

/* Details for INT_RAW register */

#define UART_INT_RAW_RXFIFO_TIMEOUT          BIT(8)
#define UART_INT_RAW_BREAK_DETECTED          BIT(7)
#define UART_INT_RAW_CTS_CHANGED             BIT(6)
#define UART_INT_RAW_DSR_CHANGED             BIT(5)
#define UART_INT_RAW_RXFIFO_OVERFLOW         BIT(4)
#define UART_INT_RAW_FRAMING_ERR             BIT(3)
#define UART_INT_RAW_PARITY_ERR              BIT(2)
#define UART_INT_RAW_TXFIFO_EMPTY            BIT(1)
#define UART_INT_RAW_RXFIFO_FULL             BIT(0)

/* Details for INT_STATUS register */

#define UART_INT_STATUS_RXFIFO_TIMEOUT       BIT(8)
#define UART_INT_STATUS_BREAK_DETECTED       BIT(7)
#define UART_INT_STATUS_CTS_CHANGED          BIT(6)
#define UART_INT_STATUS_DSR_CHANGED          BIT(5)
#define UART_INT_STATUS_RXFIFO_OVERFLOW      BIT(4)
#define UART_INT_STATUS_FRAMING_ERR          BIT(3)
#define UART_INT_STATUS_PARITY_ERR           BIT(2)
#define UART_INT_STATUS_TXFIFO_EMPTY         BIT(1)
#define UART_INT_STATUS_RXFIFO_FULL          BIT(0)

/* Details for INT_ENABLE register */

#define UART_INT_ENABLE_RXFIFO_TIMEOUT       BIT(8)
#define UART_INT_ENABLE_BREAK_DETECTED       BIT(7)
#define UART_INT_ENABLE_CTS_CHANGED          BIT(6)
#define UART_INT_ENABLE_DSR_CHANGED          BIT(5)
#define UART_INT_ENABLE_RXFIFO_OVERFLOW      BIT(4)
#define UART_INT_ENABLE_FRAMING_ERR          BIT(3)
#define UART_INT_ENABLE_PARITY_ERR           BIT(2)
#define UART_INT_ENABLE_TXFIFO_EMPTY         BIT(1)
#define UART_INT_ENABLE_RXFIFO_FULL          BIT(0)

/* Details for INT_CLEAR register */

#define UART_INT_CLEAR_RXFIFO_TIMEOUT        BIT(8)
#define UART_INT_CLEAR_BREAK_DETECTED        BIT(7)
#define UART_INT_CLEAR_CTS_CHANGED           BIT(6)
#define UART_INT_CLEAR_DSR_CHANGED           BIT(5)
#define UART_INT_CLEAR_RXFIFO_OVERFLOW       BIT(4)
#define UART_INT_CLEAR_FRAMING_ERR           BIT(3)
#define UART_INT_CLEAR_PARITY_ERR            BIT(2)
#define UART_INT_CLEAR_TXFIFO_EMPTY          BIT(1)
#define UART_INT_CLEAR_RXFIFO_FULL           BIT(0)

/* Details for CLOCK_DIVIDER register */

#define UART_CLOCK_DIVIDER_VALUE_M           0x000fffff
#define UART_CLOCK_DIVIDER_VALUE_S           0

/* Details for AUTOBAUD register */

#define UART_AUTOBAUD_GLITCH_FILTER_M        0x000000FF
#define UART_AUTOBAUD_GLITCH_FILTER_S        8
#define UART_AUTOBAUD_ENABLE                 BIT(0)

/* Details for STATUS register */

#define UART_STATUS_TXD                      BIT(31)
#define UART_STATUS_RTS                      BIT(30)
#define UART_STATUS_DTR                      BIT(29)
#define UART_STATUS_TXFIFO_COUNT_M           0x000000ff
#define UART_STATUS_TXFIFO_COUNT_S           16
#define UART_STATUS_RXD                      BIT(15)
#define UART_STATUS_CTS                      BIT(14)
#define UART_STATUS_DSR                      BIT(13)
#define UART_STATUS_RXFIFO_COUNT_M           0x000000ff
#define UART_STATUS_RXFIFO_COUNT_S           0

/* Details for CONF0 register */

#define UART_CONF0_DTR_INVERTED              BIT(24)
#define UART_CONF0_RTS_INVERTED              BIT(23)
#define UART_CONF0_TXD_INVERTED              BIT(22)
#define UART_CONF0_DSR_INVERTED              BIT(21)
#define UART_CONF0_CTS_INVERTED              BIT(20)
#define UART_CONF0_RXD_INVERTED              BIT(19)
#define UART_CONF0_TXFIFO_RESET              BIT(18)
#define UART_CONF0_RXFIFO_RESET              BIT(17)
#define UART_CONF0_IRDA_ENABLE               BIT(16)
#define UART_CONF0_TX_FLOW_ENABLE            BIT(15)
#define UART_CONF0_LOOPBACK                  BIT(14)
#define UART_CONF0_IRDA_RX_INVERTED          BIT(13)
#define UART_CONF0_IRDA_TX_INVERTED          BIT(12)
#define UART_CONF0_IRDA_WCTL                 BIT(11)
#define UART_CONF0_IRDA_TX_ENABLE            BIT(10)
#define UART_CONF0_IRDA_DUPLEX               BIT(9)
#define UART_CONF0_TXD_BREAK                 BIT(8)
#define UART_CONF0_SW_DTR                    BIT(7)
#define UART_CONF0_SW_RTS                    BIT(6)
#define UART_CONF0_STOP_BITS_M               0x00000003
#define UART_CONF0_STOP_BITS_S               4
#define UART_CONF0_BYTE_LEN_M                0x00000003
#define UART_CONF0_BYTE_LEN_S                2
#define UART_CONF0_PARITY_ENABLE             BIT(1)
#define UART_CONF0_PARITY                    BIT(0) //where 0 means even

/* Details for CONF1 register */

#define UART_CONF1_RX_TIMEOUT_ENABLE         BIT(31)
#define UART_CONF1_RX_TIMEOUT_THRESHOLD_M    0x0000007f
#define UART_CONF1_RX_TIMEOUT_THRESHOLD_S    24
#define UART_CONF1_RX_FLOWCTRL_ENABLE        BIT(23)
#define UART_CONF1_RX_FLOWCTRL_THRESHOLD_M   0x0000007f
#define UART_CONF1_RX_FLOWCTRL_THRESHOLD_S   16
#define UART_CONF1_TXFIFO_EMPTY_THRESHOLD_M  0x0000007f
#define UART_CONF1_TXFIFO_EMPTY_THRESHOLD_S  8
#define UART_CONF1_RXFIFO_FULL_THRESHOLD_M   0x0000007f
#define UART_CONF1_RXFIFO_FULL_THRESHOLD_S   0

/* Details for LOW_PULSE register */

#define UART_LOW_PULSE_MIN_M                 0x000fffff
#define UART_LOW_PULSE_MIN_S                 0

/* Details for HIGH_PULSE register */

#define UART_HIGH_PULSE_MIN_M                0x000fffff
#define UART_HIGH_PULSE_MIN_S                0

/* Details for PULSE_COUNT register */

#define UART_PULSE_COUNT_VALUE_M             0x000003ff
#define UART_PULSE_COUNT_VALUE_S             0



#define UART_FIFO_MAX 0x7f

#define VAL2FIELD(fieldname, value) ((value) << fieldname##_S)
#define FIELD2VAL(fieldname, regbits) (((regbits) >> fieldname##_S) & fieldname##_M)

#define FIELD_MASK(fieldname) (fieldname##_M << fieldname##_S)
#define SET_FIELD(regbits, fieldname, value) (((regbits) & ~FIELD_MASK(fieldname)) | VAL2FIELD(fieldname, value))


static inline int uart_txfifo_wait(int uart_num, int min_count) {
  int count;
  do {
    count = UART_FIFO_MAX - FIELD2VAL(UART_STATUS_TXFIFO_COUNT, UART(uart_num).STATUS);
  } while (count < min_count);
  return count;
}

/* Write a character to the UART.  Blocks if necessary until there is space in
 * the TX FIFO.
 */
inline
static void uart_putc(int uart_num, char c) {
  uart_txfifo_wait(uart_num, 1);
  UART(uart_num).FIFO=c;
}

inline
static int uart_rxfifo_wait(int uart_num, int min_count) {
  int count;
  do {
    count=FIELD2VAL(UART_STATUS_RXFIFO_COUNT, UART(uart_num).STATUS);
  } while(count<min_count);
  return count;
}

inline
static int uart_getc_nowait(int uart_num) {
  if(FIELD2VAL(UART_STATUS_RXFIFO_COUNT, UART(uart_num).STATUS)) {
    return UART(uart_num).FIFO;
  }
  return -1;
}




#endif /* _UART_H */
