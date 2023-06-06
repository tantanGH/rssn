#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "himem.h"
#include "uart.h"
#include "rss.h"

//
//  helper: get parameter value
//
static inline char* rss_get_param_value(RSS* rss, char* line) {

  // 末尾のCR,LF,TAB,SPを削除しておく
  // シフトJIS全角の2バイト目は 0x40～0x7e, 0x80～0xfc なので後ろから辿ってもセーフ
  char* c = line + strlen(line) - 1;
  while (c >= line) {
    if (c[0] == '\r' || c[0] == '\n' || c[0] == '\t' || c[0] == ' ') {
      c[0] = '\0';
      c--;
    } else {
      break;
    }
  }

  // 値が入っているところまでスキップ
  c = line;
  while ((c[0] >= '0' && c[0] <= '9') || c[0] == '\t' || c[0] == ' ' || c[0] == '=') {
    c++;
  }

  return c;
}

//
//  find a rss channel instance
//
RSS_CHANNEL* rss_get_channel(RSS* rss, uint32_t channel_id) {

  RSS_CHANNEL* channel = NULL;

  for (int32_t i = 0; i < rss->num_channels; i++) {
    RSS_CHANNEL* ch = &(rss->channels[i]);
    if (ch->id == channel_id) {
      // 手持ちのリストの中に、指定された channel ID を持つものがあればそれを返す
      channel = ch;
      break;
    }
    if (ch->id == 0 && ch->title[0] == '\0' && ch->link[0] == '\0') {
      // 未定義領域まで入ってしまったらそれをこのID用として設定して返す
      ch->id = channel_id;
      channel = ch;
      break;
    }
  }

  return channel;
}

//
//  open rss
//
int32_t rss_open(RSS* rss, const char* channels_def_file) {

  // default return code
  int32_t rc = -1;

  // file handler
  FILE* fp = NULL;

  // baseline
  rss->num_channels = 0;
  rss->channels = NULL;

  // ハイメモリが使えるならDMA関係ないので積極的に使う
  rss->use_high_memory = himem_isavailable() ? 1 : 0;

  // phase 1: チャンネルの数を数えるだけ
  char linebuf[ MAX_LINE_LEN ];
  int32_t n = 0;
  fp = fopen(channels_def_file, "r");
  if (fp == NULL) {
    printf("error: def file open error. (%s)\n", channels_def_file);
    goto exit;
  }
  while (fgets(linebuf, MAX_LINE_LEN, fp) != NULL) {
    if (memcmp(linebuf, PARAM_TITLE, sizeof(PARAM_TITLE) - 1) == 0) n++;
  }
  fclose(fp);
  fp = NULL;

  // 必要なメモリを確保して初期化
  rss->num_channels = n;
  rss->channels = himem_malloc( rss->num_channels * sizeof(RSS_CHANNEL), rss->use_high_memory);
  memset(rss->channels, 0, rss->num_channels * sizeof(RSS_CHANNEL));

  // phase 2: タイトルとリンク(URL)の情報を読み込んで行く
  fp = fopen(channels_def_file, "r");
  if (fp == NULL) {
    printf("error: def file open error. (%s)\n", channels_def_file);
    goto exit;
  }
  while (fgets(linebuf, MAX_LINE_LEN, fp) != NULL) {
    if (memcmp(linebuf, PARAM_TITLE, sizeof(PARAM_TITLE) - 1) == 0) {
      int32_t channel_id = atoi(linebuf + sizeof(PARAM_TITLE) - 1);
      if (channel_id < 0 || channel_id > 65535) {
        printf("error: incorrect channel ID. (%d)\n", channel_id);
        goto exit;
      }
      RSS_CHANNEL* ch = rss_get_channel(rss, channel_id);
      if (ch != NULL) {
        char* value = rss_get_param_value(rss, linebuf + sizeof(PARAM_TITLE) - 1);
        if (value != NULL) {
          strcpy(ch->title, value);
        }
      }
    }
    if (memcmp(linebuf, PARAM_LINK, sizeof(PARAM_LINK) - 1) == 0) {
      int32_t channel_id = atoi(linebuf + sizeof(PARAM_LINK) - 1);
      if (channel_id < 0 || channel_id > 65535) {
        printf("error: incorrect channel ID. (%d)\n", channel_id);
        goto exit;
      }
      RSS_CHANNEL* ch = rss_get_channel(rss, channel_id);
      if (ch != NULL) {
        char* value = rss_get_param_value(rss, linebuf + sizeof(PARAM_LINK) - 1);
        if (value != NULL) {
          strcpy(ch->link, value);
        }
      }
    }
  }
  fclose(fp);
  fp = NULL;

  rc = 0;

exit:
  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }

  return rc;
}

//
//  close rss
//
void rss_close(RSS* rss) {
  if (rss->channels != NULL) {
    // channel が items を持っていれば開放
    for (int32_t i = 0; i < rss->num_channels; i++) {
      if (rss->channels[i].items != NULL) {
        himem_free(rss->channels[i].items, rss->use_high_memory);
        rss->channels[i].num_items = 0;
        rss->channels[i].items = NULL;
      }
    }
    // channel自体を開放
    himem_free(rss->channels, rss->use_high_memory);
    rss->num_channels = 0;
    rss->channels = NULL;
  }
}

// static buffers for RSSN APIs
static uint8_t request_buf[ 1024 ];
static uint8_t response_buf[ 1024 * 128 ];
static uint8_t body_size_buf[ 16 ];

// RSSN API - download channel items
int32_t rss_download_channel_items(RSS* rss, RSS_CHANNEL* channel, UART* uart) {

  // default return code
  int32_t rc = -1;

  // request
  strcpy(request_buf, ">|        /items?link=");
  strcat(request_buf, channel->link);
  size_t request_size = strlen(request_buf);
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
    himem_free(channel->items, rss->use_high_memory);
  }
  channel->items = himem_malloc(sizeof(RSS_ITEM) * channel->num_items, rss->use_high_memory);
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