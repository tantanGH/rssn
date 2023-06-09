#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "himem.h"
#include "uart.h"
#include "rss.h"

// static buffers for RSSN APIs
static uint8_t request_buf[ 512 ];
static uint8_t response_buf[ 1024 * 128 ];
static uint8_t body_size_buf[ 16 ];

//
//  open rss
//
int32_t rss_open(RSS* rss) {

  // use high memory if we can
  rss->use_high_memory = himem_isavailable() ? 1 : 0;

  return 0;
}

//
//  close rss
//
void rss_close(RSS* rss) {
}

//
//  download rss channel content as dshell format
//
int32_t rss_download_channel_dshell(RSS* rss, const char* rss_url, FILE* output_file, UART* uart) {

  // default return code
  int32_t rc = -1;

  // request
  strcpy(request_buf, ">|        /dshell?link=");
  strcat(request_buf, rss_url);
  size_t request_size = strlen(request_buf);
  sprintf(body_size_buf, "%08x", request_size - 10);
  memcpy(request_buf + 2, body_size_buf, 8);
  if (uart_write(uart, request_buf, request_size) != 0) {
    //printf("error: request write error.\n");
    goto exit;
  }

  // response
  int32_t uart_result = uart_read(uart, response_buf, 14);
  if (uart_result != UART_OK) {
    rc = uart_result;
    goto exit;
  }
  if (memcmp(response_buf, "<|0200", 6) != 0) {
    //printf("error: unexpected error code.\n");
    goto exit;
  }

  size_t response_size;
  sscanf(response_buf + 6, "%08x", &response_size);
  if (response_size > 1024 * 128 - 16) {
    //printf("error: too large response.\n");
    goto exit;
  }
  uart_result = uart_read(uart, response_buf + 14, response_size);
  if (uart_result != UART_OK) {
    rc = uart_result;
    goto exit;
  }

  size_t write_len = 0;
  do {
    size_t len = fwrite(response_buf + 14 + write_len, 1, response_size - write_len, output_file);
    if (len == 0) break;
    write_len += len;
  } while (write_len < response_size);

  rc = 0;

exit:

  return rc;
}
