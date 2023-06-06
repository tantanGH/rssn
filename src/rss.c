#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "himem.h"
#include "uart.h"
#include "rss.h"

// ファイル読み込み用バッファ (関数内で確保しすぎると000機でアドレスエラーが発生することがあるので敢えて外出ししておく)
static char linebuf[ MAX_RSS_LEN ];

// find channel instance
RSS_CHANNEL* rss_board_get_channel(RSS_BOARD* board, uint32_t channel_id) {
  RSS_CHANNEL* channel = NULL;
  for (int32_t i = 0; i < board->num_channels; i++) {
    RSS_CHANNEL* ch = &(board->channels[i]);
    if (ch->id == channel_id) {
      channel = ch;
      break;
    }
    if (ch->id == 0) {
      ch->id = channel_id;
      channel = ch;
      break;
    }
  }
  if (channel == NULL) printf("not found!\n");
  return channel;
}

// get property value
static inline char* rss_board_get_property_value(RSS_BOARD* board, char* str) {

  // 末尾のCRLFを削除しておく
  char* c = str + strlen(str) - 1;
  if (c >= str) {
    if (c[0] == '\r' || c[0] == '\n') c[0] = '\0';
    c--;
    if (c >= str) {
      if (c[0] == '\r' || c[0] == '\n') c[0] = '\0';
    }
  }

  // 値が入っているところまでスキップ
  c = str;
  while ((c[0] >= '0' && c[0] <= '9') || c[0] == ' ' || c[0] == '\t' || c[0] == '=') {
    c++;
  }

  return c;
}

// open rss board
int32_t rss_board_open(RSS_BOARD* board, const char* board_file) {

  int32_t rc = -1;

  FILE* fp = NULL;

  board->num_channels = 0;
  board->channels = NULL;

  // phase 1: チャンネルの数を数えるだけ
  fp = fopen(board_file, "r");
  if (fp == NULL) {
    printf("error: board file open error. (%s)\n", board_file);
    goto exit;
  }
  while (fgets(linebuf, MAX_RSS_LEN, fp) != NULL) {
    if (memcmp(linebuf, PROPERTY_PREFIX_TITLE, sizeof(PROPERTY_PREFIX_TITLE) - 1) == 0) board->num_channels++;
  }
  fclose(fp);
  fp = NULL;

  // 必要なメモリをヒープから確保して初期化
  board->channels = himem_malloc( board->num_channels * sizeof(RSS_CHANNEL), 0);
  memset(board->channels, 0, board->num_channels * sizeof(RSS_CHANNEL));
  //printf("channels=%d\n", board->num_channels);

  // phase 2: タイトルとURLの情報を読み込んで行く
  fp = fopen(board_file, "r");
  if (fp == NULL) {
    printf("error: board file open error. (%s)\n", board_file);
    goto exit;
  }
  while (fgets(linebuf, MAX_RSS_LEN, fp) != NULL) {
    if (memcmp(linebuf, PROPERTY_PREFIX_TITLE, sizeof(PROPERTY_PREFIX_TITLE) - 1) == 0) {
      int32_t channel_id = atoi(linebuf + sizeof(PROPERTY_PREFIX_TITLE) - 1);
      if (channel_id < 0 || channel_id > 65535) {
        printf("error: incorrect channel ID. (%d)\n", channel_id);
        goto exit;
      }
      RSS_CHANNEL* ch = rss_board_get_channel(board, channel_id);
      if (ch != NULL) {
        char* value = rss_board_get_property_value(board, linebuf + sizeof(PROPERTY_PREFIX_TITLE) - 1);
        if (value != NULL) {
          strcpy(ch->title, value);
        }
      }
    }
    if (memcmp(linebuf, PROPERTY_PREFIX_LINK, sizeof(PROPERTY_PREFIX_LINK) - 1) == 0) {
      int32_t channel_id = atoi(linebuf + sizeof(PROPERTY_PREFIX_LINK) - 1);
      if (channel_id < 0 || channel_id > 65535) {
        printf("error: incorrect channel ID. (%d)\n", channel_id);
        goto exit;
      }
      RSS_CHANNEL* ch = rss_board_get_channel(board, channel_id);
      if (ch != NULL) {
        char* value = rss_board_get_property_value(board, linebuf + sizeof(PROPERTY_PREFIX_LINK) - 1);
        if (value != NULL) {
          strcpy(ch->link, value);
        }
      }
    }
  }

  rc = 0;

exit:
  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }

  return rc;
}

// close rss board
void rss_board_close(RSS_BOARD* board) {
  if (board->channels != NULL) {
    himem_free(board->channels, 0);
    board->channels = NULL;
    board->num_channels = 0;
  }
}

static uint8_t request_buf[ 1024 ];
static uint8_t response_buf[ 1024 * 128 ];
static uint8_t body_size_buf[ 16 ];

int32_t rss_channel_download_items(RSS_CHANNEL* channel, UART* uart) {

  int32_t rc = -1;
  size_t request_size = 0;
  
  // request
  strcpy(request_buf, ">|        /items?link=");
  strcat(request_buf, channel->link);
  request_size = strlen(request_buf);
  sprintf(body_size_buf, "%08x", request_size - 10);
  memcpy(request_buf + 2, body_size_buf, 8);
  if (uart_write(uart, request_buf, request_size) != 0) {
    printf("error: request write error.\n");
    goto exit;
  }

  // response
  if (uart_read(uart, response_buf, 14) != 0) {
    printf("error: response read error.\n");
    goto exit;
  }
  if (memcmp(response_buf, "<|0200", 6) != 0) {
    printf("error: unexpected error code.\n");
    goto exit;
  }

  size_t response_size;
  sscanf(response_buf + 6, "%08x", &response_size);
  if (response_size > 1024 * 128 - 16) {
    printf("error: too large response.\n");
    goto exit;
  }
  if (uart_read(uart, response_buf + 14, response_size) != 0) {
    printf("error: response body read error.\n");
    goto exit;
  }
  char* c = strchr(response_buf + 14, '\n');
  if (c == NULL) {
    printf("error: unexpected response body.\n");
    goto exit;
  }
  *c = '\0';
  size_t num_items = atoi(response_buf + 14);
  if (num_items > 65535) {
    printf("error: too may items.\n");
    goto exit;
  }
  channel->num_items = num_items;
  if (channel->items != NULL) {
    himem_free(channel->items, 0);
  }
  channel->items = himem_malloc(sizeof(RSS_ITEM) * channel->num_items, 0);
  for (size_t i = 0; i < channel->num_items; i++) {
    char* t1 = strchr(c+1, '\t');
    char* t2 = t1 != NULL ? strchr(t1 + 1, '\t') : NULL;
    char* t3 = t2 != NULL ? strchr(t2 + 1, '\n') : NULL;
    if (t3 == NULL) break;
    *t1 = '\0';
    *t2 = '\0';
    *t3 = '\0';
    strcpy(channel->items[i].title, c+1);
    strcpy(channel->items[i].updated, t1+1);
    strcpy(channel->items[i].summary, t2+1);
    c = t3;
  }

  rc = 0;

exit:

  return rc;
}