#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <doslib.h>
#include <iocslib.h>
#include "himem.h"
#include "panel.h"
#include "rss.h"
#include "rssn.h"

// to preserve original function keys
uint8_t funckey_original_settings[ 712 ];

// to preserve original function mode
int32_t funckey_original_mode;

// to preserv original text palette codes
//int32_t tpalet_original_codes[ 4 ];

// show help message
static void show_help_message() {
  printf("usage: rssn [options] <rssn.def>\n");
  printf("options:\n");
  printf("     -s <speed> ... baud rate (9600/19200/38400/57600) (default:38400)\n");
  printf("     -b <color> ... information bar color (default:$7a7f)\n");
  printf("     -c <file>  ... background cut file name\n");
  printf("     -p <color> ... background cut file color (default:$741a)\n");
  printf("     -h         ... show help message\n");
}

// main loop
int32_t main(int32_t argc, char* argv[]) {

  // default return code
  int32_t rc = -1;

  // channels def file name
  char* channels_def_file_name = NULL;

  // background cut file name
  char* cut_file_name = NULL;

  // background cut file color
  int32_t cut_file_color = (0b01110 << 11) | (0b10000 << 6) | (0b01101 << 1) | 0;

  // information bar color
  int32_t info_bar_color = (0b01111 << 11) | (0b01001 << 6) | (0b11111) << 1 | 1;

  // baud rate
  int32_t baud_rate = 38400;

  // uart instance
  UART uart = { 0 };

  // rss instance
  RSS rss = { 0 };
  
  // panel instance
  PANEL panel = { 0 };

  // preserve original funckey settings
  FNCKEYGT(0, funckey_original_settings);

  // preserve original funckey mode
  funckey_original_mode = C_FNKMOD(-1);

  // preserve original text palettes
  //for (int16_t i = 0; i < 4; i++) {
  //  tpalet_original_codes[i] = TPALET(i, -1);
  //}

  // credit
  printf("RSSN.X - RSS News Reader for X680x0/Human68k version " PROGRAM_VERSION " by tantan\n");

  // parse command lines
  for (int16_t i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && strlen(argv[i]) >= 2) {
      if (argv[i][1] == 's' && i+1 < argc) {
        baud_rate = atoi(argv[i+1]);
        if (baud_rate != 9600 && baud_rate != 19200 && baud_rate != 38400 && baud_rate != 57600) {
          printf("error: unknown baud rate.\n");
          goto exit;
        }
        i++;
      } else if (argv[i][1] == 'b' && i+1 < argc) {
        if (argv[i+1][0] == 'x') {
          sscanf(argv[i+1]+1, "%x", &info_bar_color);
        } else {
          info_bar_color = atoi(argv[i+1]);
        }
        i++;
      } else if (argv[i][1] == 'c' && i+1 < argc) {
        cut_file_name = argv[i+1];
        i++;
      } else if (argv[i][1] == 'p' && i+1 < argc) {
        if (argv[i+1][0] == 'x') {
          sscanf(argv[i+1]+1, "%x", &cut_file_color);
        } else {
          cut_file_color = atoi(argv[i+1]);
        }
        i++;
      } else if (argv[i][1] == 'h') {
        show_help_message();
        goto exit;
      } else {
        printf("error: unknown option (%s).\n",argv[i]);
        goto exit;
      }
    } else {
      channels_def_file_name = argv[i];
    }
  }

  if (channels_def_file_name == NULL) {
    show_help_message();
    goto exit;
  }

  // open uart  
  if (uart_open(&uart, baud_rate, 60) != 0) {
    goto exit;
  }

  // open rss
  if (rss_open(&rss, channels_def_file_name) != 0) {
    goto exit;
  }

  // screen mode
  CRTMOD(16);
  G_CLR_ON();
  C_CUROFF();
  C_FNKMOD(3);

  // customize function keys
  uint8_t funckey_settings[ 712 ];
  memset(funckey_settings, 0, 712);
  funckey_settings[ 20 * 32 + 6 * 0 ] = '\x05';   // ROLLUP
  funckey_settings[ 20 * 32 + 6 * 1 ] = '\x15';   // ROLLDOWN
  funckey_settings[ 20 * 32 + 6 * 3 ] = '\x07';   // DEL
  funckey_settings[ 20 * 32 + 6 * 4 ] = '\x01';   // UP
  funckey_settings[ 20 * 32 + 6 * 5 ] = '\x13';   // LEFT
  funckey_settings[ 20 * 32 + 6 * 6 ] = '\x04';   // RIGHT
  funckey_settings[ 20 * 32 + 6 * 7 ] = '\x06';   // DOWN
  FNCKEYST(0, funckey_settings);

  // open panel
  panel_open(&panel, &rss, info_bar_color, cut_file_name, cut_file_color);

  int16_t abort = 0;

  RSS_CHANNEL* channel = NULL;

  while (!abort) {

    // list channels
    int32_t channel_select_result = panel_list_channels(&panel);
    if (channel_select_result == CHANNEL_SELECT_EXIT) {
      rc = 0;
      abort = 1;
      break;
    }

    channel = panel.channel;

reload:
    // set communication status
    panel_set_communication_status(&panel, 1);

    // download channel items
    int32_t download_result = rss_download_channel_items(&rss, channel, &uart);

    // reset communication status
    panel_set_communication_status(&panel, 0);
    
    // check communication result
    if (download_result == UART_QUIT || download_result == UART_TIMEOUT) { 
      continue;
    } else if (download_result == UART_EXIT) {
      rc = 1;
      abort = 1;
      break;
    }

    // list channel items
    int32_t item_select_result = panel_list_channel_items(&panel);
    if (item_select_result == ITEM_SELECT_EXIT) {
      rc = 0;
      abort = 1;
      break;
    } else if (item_select_result == ITEM_SELECT_RELOAD) {
      goto reload;
    } else if (item_select_result == ITEM_SELECT_PREV_CHANNEL) {
      channel = rss_get_prev_channel(&rss, channel);
      panel.channel = channel;
      goto reload;
    } else if (item_select_result == ITEM_SELECT_NEXT_CHANNEL) {
      channel = rss_get_next_channel(&rss, channel);
      panel.channel = channel;
      goto reload;
    }

  }

catch:

  // close panel
  panel_close(&panel);

  // close rss
  rss_close(&rss);

  // close uart
  uart_close(&uart);

  // resume screen
  CRTMOD(16);
  G_CLR_ON();
  C_CURON();

  // resume function key settings
  FNCKEYST(0, funckey_original_settings);

  // resume function key mode
  C_FNKMOD(funckey_original_mode);

exit:

  return rc;
}