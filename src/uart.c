#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <doslib.h>
#include <iocslib.h>
#include "himem.h"
#include "keyboard.h"
#include "uart.h"

static inline void e_set232c(int32_t mode) {
    
  struct REGS in_regs = { 0 };
  struct REGS out_regs = { 0 };

  in_regs.d0 = 0xf1;
  in_regs.d1 = mode;
  in_regs.d2 = 0x0030;

  TRAP15(&in_regs, &out_regs);
}

static inline uint8_t* e_buf232c(uint8_t* buf_addr, size_t buf_size, size_t* orig_size) {

  struct REGS in_regs = { 0 };
  struct REGS out_regs = { 0 };

  in_regs.d0 = 0xf1;
  in_regs.d1 = buf_size;
  in_regs.d2 = 0x0036;
  in_regs.a1 = (uint32_t)buf_addr;

  TRAP15(&in_regs, &out_regs);

  *orig_size = out_regs.d1;

  return (uint8_t*)out_regs.a1;
}

// open UART
int32_t uart_open(UART* uart, int32_t baud_rate, int32_t timeout) {

  int32_t rc = -1;

  uart->tmsio = 0;
  uart->rsdrv = 0;

  uart->baud_rate = baud_rate;
  uart->timeout = timeout;

  uart->buffer = NULL;
  uart->buffer_size = 0;

  uart->buffer_original = NULL;
  uart->buffer_size_original = 0;

  // TMSIO.X が組み込まれているか？
  uint32_t eye_catch_addr = INTVCG(0x58) - 4;
  uint8_t eye_catch[4];
  for (int16_t i = 0; i < 4; i++) {
    eye_catch[i] = B_BPEEK((uint8_t*)(eye_catch_addr + i));
  }
  uart->tmsio = (memcmp(eye_catch, "TmS2", 4) == 0) ? 1 : 0;

  // RSDRV.SYS が組み込まれているか？
  int32_t v = INTVCG(0x1f1);
  uart->rsdrv = (v < 0 || (v >= 0xfe0000 && v <= 0xffffff)) ? 0 : 1;
  // convert baud rate to IOCS SET232C() speed value
  int16_t speed = 8;
  switch (baud_rate) {
    case 9600:
      speed = 7;
      break;
    case 19200:
      if (uart->tmsio == 0 && uart->rsdrv == 0) {
        printf("error: need TMSIO.X or RSDRV.SYS for 19200bps use.\n");
        goto exit;
      }
      speed = 8;
      break;
    case 38400:
      if (uart->tmsio == 0 && uart->rsdrv == 0) {
        printf("error: need TMSIO.X or RSDRV.SYS for 38400bps use.\n");
        goto exit;
      }
      speed = 9;
      break;
    default:
      printf("error: unsupported baud rate.\n");
      goto exit;      
  }

  // setup RS232C port
  if (uart->rsdrv) {
    // RSDRV拡張IOCSコールを使って設定する
    e_set232c( 0x4C00 + speed );    // 8bit, non-P, 1stop, no flow control
    // RSDRV拡張IOCSコールが使える場合は、バッファサイズを増やしておく
    uart->buffer_size = UART_BUFFER_SIZE;
    uart->buffer = himem_malloc(uart->buffer_size, 0);
    uart->buffer_original = e_buf232c(uart->buffer, uart->buffer_size, &(uart->buffer_size_original));
  } else {
    SET232C( 0x4C00 + speed );      // 8bit, non-P, 1stop, no flow control
  }

  // flush read buffer
  while (LOF232C() > 0) {
    INP232C();
  }

  rc = 0;

exit:
  return rc;
}

// close UART
void uart_close(UART* uart) {

  // RSDRV拡張IOCSコールを使った場合はバッファを元に戻しておく
  if (uart->rsdrv) {
    if (uart->buffer_original != NULL) {
      size_t sz;
      e_buf232c(uart->buffer_original, uart->buffer_size_original, &sz);
    }
    if (uart->buffer != NULL) {
      himem_free(uart->buffer, 0);
      uart->buffer = NULL;
    }
  }

}

// write uart
int32_t uart_write(UART* uart, uint8_t* buffer, size_t len) {

  int32_t rc = -1;

  uint32_t t0 = ONTIME();
  int32_t timeout = uart->timeout * 100;

  for (size_t i = 0; i < len; i++) {

    while (OSNS232C() == 0) {
      uint32_t t1 = ONTIME();
      if ((t1 - t0) > timeout) {
        //printf("error: transfer timeout.\n");
        goto exit;
      }
    }

    OUT232C(buffer[i]);

  }

  rc = 0;

exit:
  return rc;
}

// read uart
int32_t uart_read(UART* uart, uint8_t* buffer, size_t len) {

  int32_t rc = -1;

  uint32_t t0 = ONTIME();
  int32_t timeout = uart->timeout * 100;

  size_t i = 0; 
  while (i < len) {
    if (LOF232C() > 0) {
      buffer[ i++ ] = INP232C() & 0xff;
      continue;
    }
    uint32_t t1 = ONTIME();
    if ((t1 - t0) > timeout) {
      rc = UART_TIMEOUT;
      goto exit;
    }

    if (B_KEYSNS() != 0) {
      int16_t scan_code = B_KEYINP() >> 8;
      if (scan_code == KEY_SCAN_CODE_ESC) {
        rc = UART_QUIT;
        goto exit;
      } else if (scan_code == KEY_SCAN_CODE_F10) {
        rc = UART_EXIT;
        goto exit;
      }
    }
  }

  rc = UART_OK;

exit:
  return rc;
}
