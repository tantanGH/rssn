#ifndef __H_RSS__
#define __H_RSS__

#include <stdint.h>
#include "uart.h"

#define MAX_RSS_LEN      (256)
#define MAX_RSS_LEN_DESC (512)

#define PROPERTY_PREFIX_TITLE   "rss.channel.title."
#define PROPERTY_PREFIX_LINK    "rss.channel.link."

typedef struct {

  uint32_t id;

  uint8_t title[ MAX_RSS_LEN ];
  uint8_t updated[ MAX_RSS_LEN ];
  uint8_t summary[ MAX_RSS_LEN_DESC ];

} RSS_ITEM;

typedef struct {

  uint32_t id;

  uint8_t title[ MAX_RSS_LEN ];
  uint8_t link[ MAX_RSS_LEN ];

  uint16_t num_items;
  RSS_ITEM* items;

} RSS_CHANNEL;

typedef struct {

  uint16_t num_channels;
  RSS_CHANNEL* channels;

} RSS_BOARD;

int32_t rss_board_open(RSS_BOARD* board, const char* board_file);
void rss_board_close(RSS_BOARD* board);

int32_t rss_channel_download_items(RSS_CHANNEL* channel, UART* uart);

#endif