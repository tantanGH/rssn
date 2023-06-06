#ifndef __H_PANEL__
#define __H_PANEL__

#include <stdint.h>
#include "rss.h"

#define PANEL_MIN_CURSOR_POS (0)
#define PANEL_MAX_CURSOR_POS (29)

typedef struct {

  RSS_BOARD* board;
  RSS_CHANNEL* channel;

  char* cut_file_name;
  char* cut_file_color;

  uint16_t cursor_pos;
  uint16_t cursor_ofs;

  int16_t channel_list_len;
  int16_t item_list_len;

} PANEL;

int32_t panel_open(PANEL* panel, RSS_BOARD* board, char* cut_file_name, char* cut_file_color);
void panel_close(PANEL* panel);

void panel_clear(PANEL* panel);

RSS_CHANNEL* panel_select_channel(PANEL* panel);
RSS_ITEM* panel_select_channel_item(PANEL* panel, RSS_CHANNEL* channel);

#endif