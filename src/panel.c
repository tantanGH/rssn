#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <doslib.h>
#include <iocslib.h>
#include "keyboard.h"
#include "rss.h"
#include "rssn.h"
#include "panel.h"

static inline void panel_hide_cursor(PANEL* panel, int16_t pos) {
  struct XLINEPTR txxptr = { 1, 0, 16*pos + 31, 768, 0x0000 };
  TXXLINE(&txxptr);
}

static inline void panel_show_cursor(PANEL* panel, int16_t pos) {
  struct XLINEPTR txxptr = { 1, 0, 16*pos + 31, 768, 0xffff };
  TXXLINE(&txxptr);
}

static inline void panel_scroll_down(PANEL* panel) {
  int16_t ras_src = ( 16 * 29 ) / 4 + 3;
  int16_t ras_dst = ( 16 * 30 ) / 4 + 3;
  TXRASCPY( ras_src * 256 + ras_dst, 16 * (PANEL_MAX_CURSOR_POS - PANEL_MIN_CURSOR_POS) / 4, 0x8001); 
}

static inline void panel_scroll_up(PANEL* panel) {
  int16_t ras_src = ( 16 * 2 ) / 4;
  int16_t ras_dst = ( 16 * 1 ) / 4;
  TXRASCPY( ras_src * 256 + ras_dst, 16 * (PANEL_MAX_CURSOR_POS - PANEL_MIN_CURSOR_POS) / 4, 0x0001); 
}

static inline void panel_item_scroll_down(PANEL* panel) {
  int16_t ras_src = ( 16 * ( panel->item_list_len - 1 )) / 4 + 3;
  int16_t ras_dst = ( 16 * ( panel->item_list_len - 0 )) / 4 + 3;
  TXRASCPY( ras_src * 256 + ras_dst, 16 * panel->item_list_len / 4, 0x8001); 
}

static inline void panel_item_scroll_up(PANEL* panel) {
  int16_t ras_src = ( 16 * 2 ) / 4;
  int16_t ras_dst = ( 16 * 1 ) / 4;
  TXRASCPY( ras_src * 256 + ras_dst, 16 * panel->item_list_len / 4, 0x0001); 
}

static inline void panel_show_channel_title(PANEL* panel, int16_t pos, RSS_CHANNEL* channel) {
  B_PUTMES(1,  0, 1 + pos, 40, channel->title);
  B_PUTMES(1, 41, 1 + pos, 55, channel->link);
}

static inline void panel_show_channel_item_summary(PANEL* panel, RSS_ITEM* item) {
  B_CONSOL(0, 16 * ( panel->item_list_len + 2 ), 95, 30 - ( panel->item_list_len + 2 ));
  B_COLOR(1);
  B_PRINT("\n");
  B_PRINT(item->updated);
  B_PRINT("\r\n\n");
  B_PRINT(item->summary);
  B_CLR_ED();
  B_CONSOL(0, 0, 95, 30);
}

static inline void panel_show_channel_item(PANEL* panel, int16_t pos, RSS_ITEM* item, int16_t summary) {
  B_PUTMES(1,  0, 1 + pos, 23, item->updated);
  B_PUTMES(1, 24, 1 + pos, 72, item->title);
  if (summary) {
    panel_show_channel_item_summary(panel, item);
  }
}

void panel_clear(PANEL* panel) {
  struct TXFILLPTR tfp = { 0, 0, 16, 768, 512 - 16, 0x0000 };
  TXFILL(&tfp);
}

static void panel_move_cursor_up(PANEL* panel) {
  if (panel->cursor_pos > PANEL_MIN_CURSOR_POS) {
    panel_hide_cursor(panel, panel->cursor_pos);
    panel->cursor_pos--;
    panel_show_cursor(panel, panel->cursor_pos);
  } else if (panel->cursor_ofs > 0) {
    panel_hide_cursor(panel, panel->cursor_pos);
    panel_scroll_down(panel);  
    panel->cursor_ofs--;
    panel_show_channel_title(panel, panel->cursor_pos, &(panel->board->channels[ panel->cursor_ofs + panel->cursor_pos ]));
    panel_show_cursor(panel, panel->cursor_pos);
  }
}

static void panel_move_cursor_down(PANEL* panel) {
  if (panel->cursor_pos < PANEL_MAX_CURSOR_POS) {
    if (panel->cursor_ofs + panel->cursor_pos < panel->board->num_channels) {
      panel_hide_cursor(panel, panel->cursor_pos);
      panel->cursor_pos++;
      panel_show_cursor(panel, panel->cursor_pos);
    }
  } else if (panel->cursor_pos == PANEL_MAX_CURSOR_POS) {
    if (panel->cursor_ofs + panel->cursor_pos < panel->board->num_channels - 1) {
      panel_hide_cursor(panel, panel->cursor_pos);
      panel_scroll_up(panel);  
      panel->cursor_ofs++;
      panel_show_channel_title(panel, panel->cursor_pos, &(panel->board->channels[ panel->cursor_ofs + panel->cursor_pos ]));
      panel_show_cursor(panel, panel->cursor_pos);
    }
  }
}

static void panel_move_item_cursor_up(PANEL* panel) {
  if (panel->cursor_pos > 0) {
    panel_hide_cursor(panel, panel->cursor_pos);
    panel->cursor_pos--;
    panel_show_cursor(panel, panel->cursor_pos);
    panel_show_channel_item_summary(panel, &(panel->channel->items[ panel->cursor_ofs + panel->cursor_pos ]));
  } else if (panel->cursor_ofs > 0) {
    panel_hide_cursor(panel, panel->cursor_pos);
    panel_item_scroll_down(panel);  
    panel->cursor_ofs--;
    panel_show_channel_item(panel, panel->cursor_pos, &(panel->channel->items[ panel->cursor_ofs + panel->cursor_pos ]), 1);
    panel_show_cursor(panel, panel->cursor_pos);
  }
}

static void panel_move_item_cursor_down(PANEL* panel) {
  if (panel->cursor_pos < panel->item_list_len - 1) {
    if (panel->cursor_ofs + panel->cursor_pos < panel->channel->num_items - 1) {
      panel_hide_cursor(panel, panel->cursor_pos);
      panel->cursor_pos++;
      panel_show_cursor(panel, panel->cursor_pos);
      panel_show_channel_item_summary(panel, &(panel->channel->items[ panel->cursor_ofs + panel->cursor_pos ]));
    }
  } else if (panel->cursor_pos == panel->item_list_len - 1) {
    if (panel->cursor_ofs + panel->cursor_pos < panel->channel->num_items - 1) {
      panel_hide_cursor(panel, panel->cursor_pos);
      panel_item_scroll_up(panel);  
      panel->cursor_ofs++;
      panel_show_channel_item(panel, panel->cursor_pos, &(panel->channel->items[ panel->cursor_ofs + panel->cursor_pos ]), 1);
      panel_show_cursor(panel, panel->cursor_pos);
    }
  }
}

int32_t panel_open(PANEL* panel, RSS_BOARD* board, char* cut_file_name, char* cut_file_color) {

  panel->board = board;
  panel->channel = NULL;

  panel->cut_file_name = cut_file_name;
  panel->cut_file_color = cut_file_color;

  panel->cursor_pos = 0;
  panel->cursor_ofs = 0;

  panel->channel_list_len = 30;
  panel->item_list_len = 12;
  
  if (panel->cut_file_name != NULL && panel->cut_file_color != NULL) {
    uint8_t cmd[ 256 ];
    sprintf(cmd, "cut.r -gc -p$%s %s", panel->cut_file_color, panel->cut_file_name);
    system(cmd);
  }

  // customize text palette codes
  TPALET(0, (0b00000 << 11) | (0b00000 << 6) | (0b00000) << 1 | 0);   // 背景
  TPALET(1, (0b11111 << 11) | (0b11111 << 6) | (0b11111) << 1 | 1);   // 文字
  TPALET(2, (0b11111 << 11) | (0b11111 << 6) | (0b11111) << 1 | 1);   // カーソル
  TPALET(3, (0b01111 << 11) | (0b01001 << 6) | (0b11111) << 1 | 1);   // インフォメーションバー

  B_PUTMES(15, 0, 0, 95, " RSSN.X - RSS News Reader for X680x0 version " PROGRAM_VERSION);
}

void panel_close(PANEL* panel) {

}

RSS_CHANNEL* panel_select_channel(PANEL* panel) {

  RSS_CHANNEL* channel = NULL;

  panel_clear(panel);

  for (int16_t i = 0; i < panel->board->num_channels && i < panel->channel_list_len; i++) {
    panel_show_channel_title(panel, i, &(panel->board->channels[i]));
  }

  panel->cursor_ofs = 0;
  panel->cursor_pos = 0;
  panel_show_cursor(panel, panel->cursor_pos);

  int16_t abort = 0;

  while (!abort) {

    if (B_KEYSNS() != 0) {
      
      int16_t scan_code = B_KEYINP() >> 8;
      int16_t shift_sense = B_SFTSNS();
      
      if (shift_sense & 0x01) {
        scan_code += 0x100;
      }
      if (shift_sense & 0x02) {
        scan_code += 0x200;
      }

      switch (scan_code) {

        case KEY_SCAN_CODE_ESC:
        case KEY_SCAN_CODE_Q: {
          abort = 1;
          break;
        }

        case KEY_SCAN_CODE_UP:
        case KEY_SCAN_CODE_K:
        case KEY_SCAN_CODE_CTRL_P: {
          panel_move_cursor_up(panel);
          break;
        }

        case KEY_SCAN_CODE_DOWN:
        case KEY_SCAN_CODE_J:
        case KEY_SCAN_CODE_CTRL_N: {
          panel_move_cursor_down(panel);
          break;
        }

        case KEY_SCAN_CODE_ENTER:
        case KEY_SCAN_CODE_CR: {
          channel = &(panel->board->channels[ panel->cursor_ofs + panel->cursor_pos ]);
          abort = 1;
          break;
        }
      }
    }
  }

  panel_hide_cursor(panel, panel->cursor_pos);

  return channel;
}

RSS_ITEM* panel_select_channel_item(PANEL* panel, RSS_CHANNEL* channel) {

  panel->channel = channel;

  RSS_ITEM* item = NULL;

  panel_clear(panel);

  char channel_info[ 256 ];
  sprintf(channel_info, " %s %s", channel->title, channel->link);
  B_PUTMES(15, 0, panel->item_list_len + 1, 95, channel_info);

  for (int16_t i = 0; i < channel->num_items && i < panel->item_list_len; i++) {
    panel_show_channel_item(panel, i, &(channel->items[i]), i == 0 ? 1 : 0);
  }

  panel->cursor_ofs = 0;
  panel->cursor_pos = 0;
  panel_show_cursor(panel, panel->cursor_pos);

  int16_t abort = 0;

  while (!abort) {

    if (B_KEYSNS() != 0) {

      int16_t scan_code = B_KEYINP() >> 8;
      int16_t shift_sense = B_SFTSNS();

      if (shift_sense & 0x01) {
        scan_code += 0x100;
      }
      if (shift_sense & 0x02) {
        scan_code += 0x200;
      }

      switch (scan_code) {

        case KEY_SCAN_CODE_ESC:
        case KEY_SCAN_CODE_Q: {
          abort = 1;
          break;
        }

        case KEY_SCAN_CODE_UP:
        case KEY_SCAN_CODE_K:
        case KEY_SCAN_CODE_CTRL_P: {
          panel_move_item_cursor_up(panel);
          break;
        }

        case KEY_SCAN_CODE_DOWN:
        case KEY_SCAN_CODE_J:
        case KEY_SCAN_CODE_CTRL_N: {
          panel_move_item_cursor_down(panel);
          break;
        }
      }
    }
  }

  panel_hide_cursor(panel, panel->cursor_pos);

  return item;
}