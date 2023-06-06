#ifndef __H_PANEL__
#define __H_PANEL__

#include <stdint.h>
#include "rss.h"

typedef struct {

  RSS* rss;

  RSS_CHANNEL* channel;       // current channel
  RSS_ITEM* item;             // current item

  char* cut_file_name;
  uint16_t cut_file_color;

  uint16_t cursor_pos;
  uint16_t cursor_ofs;

  int16_t channel_list_len;
  int16_t item_list_len;

} PANEL;

int32_t panel_open(PANEL* panel, RSS* rss, char* cut_file_name, uint16_t cut_file_color);
void panel_close(PANEL* panel);
void panel_clear(PANEL* panel);

RSS_CHANNEL* panel_list_channels(PANEL* panel);
RSS_ITEM* panel_list_channel_items(PANEL* panel, RSS_CHANNEL* channel);

#endif