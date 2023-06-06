#ifndef __H_RSS__
#define __H_RSS__

#include <stdint.h>
#include "uart.h"

#define MAX_LINE_LEN      (256)
#define MAX_TITLE_LEN     (256)
#define MAX_LINK_LEN      (256)
#define MAX_TIMESTAMP_LEN (256)
#define MAX_SUMMARY_LEN   (512)

#define PARAM_TITLE    "rss.channel.title."
#define PARAM_LINK     "rss.channel.link."

typedef struct {

  uint32_t id;

  uint8_t title[ MAX_TITLE_LEN ];
  uint8_t updated[ MAX_TIMESTAMP_LEN ];
  uint8_t summary[ MAX_SUMMARY_LEN ];

} RSS_ITEM;

typedef struct {

  uint32_t id;

  uint8_t title[ MAX_TITLE_LEN ];
  uint8_t link[ MAX_LINK_LEN ];

  uint16_t num_items;
  RSS_ITEM* items;

} RSS_CHANNEL;

typedef struct {

  int32_t num_channels;
  RSS_CHANNEL* channels;

  int16_t use_high_memory;

} RSS;

int32_t rss_open(RSS* board, const char* board_file);
void rss_close(RSS* board);

int32_t rss_download_channel_items(RSS* rss, RSS_CHANNEL* channel, UART* uart);

#endif