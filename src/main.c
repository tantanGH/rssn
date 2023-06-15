#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <doslib.h>
#include <iocslib.h>
#include "uart.h"
#include "rss.h"

#define PROGRAM_VERSION "0.5.1"

//
//  helper: vdisp interrupt handler for progress bar display
//
static uint32_t vdisp_counter;
static void __attribute__((interrupt)) vdisp_handler() {

  int16_t c = vdisp_counter;

  if (c < 16) {
    B_PUTMES(7, 31 + c, 31, 0, ">");
  } else {
    B_PUTMES(7, 31 + c - 16, 31, 0, "_");
  }  

  vdisp_counter = ( vdisp_counter + 1 ) % 32;
}

//
//  helper: show help message
//
static void show_help_message() {
  printf("RSSN.X - RSSN client for X680x0/Human68k version " PROGRAM_VERSION " by tantan\n");
  printf("usage: rssn [options] <rss-url> [output-file]\n");
  printf("options:\n");
  printf("     -s <speed> ... baud rate (9600/19200/38400) (default:38400)\n");
  printf("     -h         ... show help message\n");
}

// env var buffers
static uint8_t env_rssn_speed[ 256 ];
static uint8_t env_rssn_timeout[ 256 ];

//
//  main
//
int32_t main(int32_t argc, uint8_t* argv[]) {

  // default return code
  int32_t rc = -1;

  // baud rate
  int32_t baud_rate = GETENV("RSSN_SPEED", NULL, env_rssn_speed) >= 0 ? atoi(env_rssn_speed) : 38400;

  // timeout
  int32_t timeout = GETENV("RSSN_TIMEOUT", NULL, env_rssn_timeout) >= 0 ? atoi(env_rssn_timeout) : 60;

  // rss url
  uint8_t* rss_url = NULL;

  // output file name
  uint8_t output_file_name[ 256 ];
  strcpy(output_file_name, "_R.D");

  // output file handle
  FILE* fo = NULL;

  // uart instance
  UART uart = { 0 };

  // rss instance
  RSS rss = { 0 };

  // original function key mode
  int32_t func_mode = C_FNKMOD(-1);

  // parse command lines
  for (int16_t i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && strlen(argv[i]) >= 2) {
      if (argv[i][1] == 's' && i+1 < argc) {
        baud_rate = atoi(argv[i+1]);
        i++;
      } else if (argv[i][1] == 'h') {
        show_help_message();
        goto exit;
      } else {
        printf("error: unknown option (%s).\n",argv[i]);
        goto exit;
      }
    } else {
      if (rss_url == NULL) {
        rss_url = argv[i];
      } else {
        strcpy(output_file_name, argv[i]);
      }
    }
  }

  if (baud_rate != 9600 && baud_rate != 19200 && baud_rate != 38400) {
    printf("error: unsupported baud rate. (%d)\n", baud_rate);
    goto exit;
  }

  if (rss_url == NULL) {
    show_help_message();
    goto exit;
  }

try:

  // cursor off
  C_CUROFF();

  // function key off
  C_FNKMOD(3);

  // open uart  
  if (uart_open(&uart, baud_rate, timeout) != 0) {
    goto catch;
  }

  // open rss
  if (rss_open(&rss) != 0) {
    goto catch;
  }

  // open output file in binary mode
  if ((fo = fopen(output_file_name, "wb")) == NULL) {
    goto catch;
  } 

  // message
  B_PUTMES(7, 0, 31, 32, "Now Loading... [ESC] to cancel ");

  // vsync interrupt for progress bar
  VDISPST((uint8_t*)vdisp_handler, 0, 55);

  // download channel items
  int32_t download_result = rss_download_channel_dshell(&rss, rss_url, fo, &uart);
    
  // check communication result
  if (download_result == UART_QUIT || download_result == UART_EXIT) {
    printf("error: canceled.\n");
  } else if (download_result == UART_TIMEOUT) { 
    printf("error: timeout.\n");
  } else {
    rc = 0;
  }

catch:

  // stop vsync interrupt handler
  VDISPST(0, 0, 0);

  // close output file handle
  if (fo != NULL) {
    fclose(fo);
    fo = NULL;
  }

  // close rss
  rss_close(&rss);

  // close uart
  uart_close(&uart);

  // remove file in error cases
  if (rc != 0) {
    DELETE(output_file_name);
  }

  // cursor on
  C_CURON();

  // resume function key mode
  C_FNKMOD(func_mode);

exit:

  return rc;
}