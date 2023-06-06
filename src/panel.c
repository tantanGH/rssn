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

//
//  helper: hide cursor bar
//
static inline void panel_hide_cursor(PANEL* panel, int16_t pos) {
  struct XLINEPTR txx = { 1, 0, 16*pos + 31, 768, 0x0000 };
  TXXLINE(&txx);
}

//
//  helper: show cursor bar
//
static inline void panel_show_cursor(PANEL* panel, int16_t pos) {
  struct XLINEPTR txx = { 1, 0, 16*pos + 31, 768, 0xffff };
  TXXLINE(&txx);
}

//
//  helper: channel list scroll down
//
static inline void panel_channel_scroll_down(PANEL* panel) {
  int16_t ras_src = ( 16 * ( panel->channel_list_len - 1 )) / 4 + 3;
  int16_t ras_dst = ( 16 * ( panel->channel_list_len - 0 )) / 4 + 3;
  TXRASCPY( ras_src * 256 + ras_dst, 16 * panel->channel_list_len / 4, 0x8001); 
}

//
//  helper: channel list scroll up
//
static inline void panel_channel_scroll_up(PANEL* panel) {
  int16_t ras_src = ( 16 * 2 ) / 4;
  int16_t ras_dst = ( 16 * 1 ) / 4;
  TXRASCPY( ras_src * 256 + ras_dst, 16 * panel->channel_list_len / 4, 0x0001); 
}

//
//  helper: item list scroll down
//
static inline void panel_item_scroll_down(PANEL* panel) {
  int16_t ras_src = ( 16 * ( panel->item_list_len - 1 )) / 4 + 3;
  int16_t ras_dst = ( 16 * ( panel->item_list_len - 0 )) / 4 + 3;
  TXRASCPY( ras_src * 256 + ras_dst, 16 * panel->item_list_len / 4, 0x8001); 
}

//
//  helper: item list scroll up
//
static inline void panel_item_scroll_up(PANEL* panel) {
  int16_t ras_src = ( 16 * 2 ) / 4;
  int16_t ras_dst = ( 16 * 1 ) / 4;
  TXRASCPY( ras_src * 256 + ras_dst, 16 * panel->item_list_len / 4, 0x0001); 
}

//
//  helper: show channel line
//
static inline void panel_show_channel_line(PANEL* panel, int16_t pos, RSS_CHANNEL* channel) {
  B_PUTMES(1,  0, 1 + pos, 40, channel->title);
  B_PUTMES(1, 41, 1 + pos, 55, channel->link);
}

//
//  helper: show item line
//
static inline void panel_show_item_line(PANEL* panel, int16_t pos, RSS_ITEM* item) {
  B_PUTMES(1,  0, 1 + pos, 23, item->updated);
  B_PUTMES(1, 24, 1 + pos, 72, item->title);
}

//
//  helper: show item summary
//
static inline void panel_show_item_summary(PANEL* panel, RSS_ITEM* item) {
  B_CONSOL(0, 16 * ( panel->item_list_len + 2 ), 95, 30 - ( panel->item_list_len + 2 ));
  B_COLOR(1);
  B_PRINT("\n");
  B_PRINT(item->updated);
  B_PRINT("\r\n\n");
  B_PRINT(item->summary);
  B_CLR_ED();
  B_CONSOL(0, 0, 95, 30);
}

//
//  helper: move channel cursor up
//
static void panel_move_channel_cursor_up(PANEL* panel) {
  if (panel->cursor_pos > 0) {
    panel_hide_cursor(panel, panel->cursor_pos);
    panel->cursor_pos--;
    panel_show_cursor(panel, panel->cursor_pos);
  } else if (panel->cursor_ofs > 0) {
    panel_hide_cursor(panel, panel->cursor_pos);
    panel_channel_scroll_down(panel);  
    panel->cursor_ofs--;
    panel_show_channel_line(panel, panel->cursor_pos, &(panel->rss->channels[ panel->cursor_ofs + panel->cursor_pos ]));
    panel_show_cursor(panel, panel->cursor_pos);
  }
}

//
//  helper: move channel cursor down
//
static void panel_move_channel_cursor_down(PANEL* panel) {
  if (panel->cursor_pos < panel->channel_list_len - 1) {
    if (panel->cursor_ofs + panel->cursor_pos < panel->rss->num_channels - 1) {
      panel_hide_cursor(panel, panel->cursor_pos);
      panel->cursor_pos++;
      panel_show_cursor(panel, panel->cursor_pos);
    }
  } else if (panel->cursor_pos == panel->channel_list_len - 1) {
    if (panel->cursor_ofs + panel->cursor_pos < panel->rss->num_channels - 1) {
      panel_hide_cursor(panel, panel->cursor_pos);
      panel_channel_scroll_up(panel);  
      panel->cursor_ofs++;
      panel_show_channel_line(panel, panel->cursor_pos, &(panel->rss->channels[ panel->cursor_ofs + panel->cursor_pos ]));
      panel_show_cursor(panel, panel->cursor_pos);
    }
  }
}

//
//  helper: move item cursor up
//
static void panel_move_item_cursor_up(PANEL* panel) {
  if (panel->cursor_pos > 0) {
    panel_hide_cursor(panel, panel->cursor_pos);
    panel->cursor_pos--;
    panel_show_cursor(panel, panel->cursor_pos);
    panel_show_item_summary(panel, &(panel->channel->items[ panel->cursor_ofs + panel->cursor_pos ]));
  } else if (panel->cursor_ofs > 0) {
    panel_hide_cursor(panel, panel->cursor_pos);
    panel_item_scroll_down(panel);  
    panel->cursor_ofs--;
    RSS_ITEM* item = &(panel->channel->items[ panel->cursor_ofs + panel->cursor_pos ]);
    panel_show_item_line(panel, panel->cursor_pos, item);
    panel_show_item_summary(panel, item);
    panel_show_cursor(panel, panel->cursor_pos);
  }
}

//
//  helper: move item cursor down
//
static void panel_move_item_cursor_down(PANEL* panel) {
  if (panel->cursor_pos < panel->item_list_len - 1) {
    if (panel->cursor_ofs + panel->cursor_pos < panel->channel->num_items - 1) {
      panel_hide_cursor(panel, panel->cursor_pos);
      panel->cursor_pos++;
      panel_show_cursor(panel, panel->cursor_pos);
      panel_show_item_summary(panel, &(panel->channel->items[ panel->cursor_ofs + panel->cursor_pos ]));
    }
  } else if (panel->cursor_pos == panel->item_list_len - 1) {
    if (panel->cursor_ofs + panel->cursor_pos < panel->channel->num_items - 1) {
      panel_hide_cursor(panel, panel->cursor_pos);
      panel_item_scroll_up(panel);  
      panel->cursor_ofs++;
      RSS_ITEM* item = &(panel->channel->items[ panel->cursor_ofs + panel->cursor_pos ]);
      panel_show_item_line(panel, panel->cursor_pos, item);
      panel_show_item_summary(panel, item);
      panel_show_cursor(panel, panel->cursor_pos);
    }
  }
}

//
//  open panel
//
int32_t panel_open(PANEL* panel, RSS* rss, char* cut_file_name, uint16_t cut_file_color) {

  panel->rss = rss;
  panel->channel = NULL;
  panel->item = NULL;

  panel->cut_file_name = cut_file_name;
  panel->cut_file_color = cut_file_color;

  panel->cursor_pos = 0;
  panel->cursor_ofs = 0;

  panel->channel_list_len = 29;
  panel->item_list_len = 11;
  
  // カットファイルの指定があれば子プロセスで cut.r を呼び出す形で表示する
  if (panel->cut_file_name != NULL && panel->cut_file_color != 0) {
    uint8_t cmd[ 256 ];
    sprintf(cmd, "cut.r -gc -p$%x %s", panel->cut_file_color, panel->cut_file_name);
    system(cmd);
  }

  B_PUTMES(15, 0,  0, 95, " RSSN.X - RSS News Reader for X680x0 version " PROGRAM_VERSION);
  B_PUTMES(15, 0, 31, 95, "");
}

//
//  close panel
//
void panel_close(PANEL* panel) {

}

//
//  clear panel
//
void panel_clear(PANEL* panel) {
  // 最上段と最下段を残してプレーン0,1を消す
  struct TXFILLPTR txf0 = { 0, 0, 16, 768, 512 - 32, 0x0000 };
  TXFILL(&txf0);
  struct TXFILLPTR txf1 = { 1, 0, 16, 768, 512 - 32, 0x0000 };
  TXFILL(&txf1);
}

//
//  list channels
//
RSS_CHANNEL* panel_list_channels(PANEL* panel) {

  RSS_CHANNEL* channel = NULL;

  panel_clear(panel);

  for (int16_t i = 0; i < panel->rss->num_channels && i < panel->channel_list_len; i++) {
    panel_show_channel_line(panel, i, &(panel->rss->channels[i]));
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
          panel_move_channel_cursor_up(panel);
          break;
        }

        case KEY_SCAN_CODE_DOWN:
        case KEY_SCAN_CODE_J:
        case KEY_SCAN_CODE_CTRL_N: {
          panel_move_channel_cursor_down(panel);
          break;
        }

        case KEY_SCAN_CODE_ENTER:
        case KEY_SCAN_CODE_CR: {
          channel = &(panel->rss->channels[ panel->cursor_ofs + panel->cursor_pos ]);
          abort = 1;
          break;
        }
      }
    }
  }

  panel_hide_cursor(panel, panel->cursor_pos);

  panel->channel = channel;

  return channel;
}

//
//  list channel items
//
RSS_ITEM* panel_list_channel_items(PANEL* panel, RSS_CHANNEL* channel) {

  RSS_ITEM* item = NULL;

  panel_clear(panel);

  char channel_info_bar[ 256 ];
  sprintf(channel_info_bar, " %s - %s", channel->title, channel->link);
  B_PUTMES(15, 0, panel->item_list_len + 1, 95, channel_info_bar);

  for (int16_t i = 0; i < channel->num_items && i < panel->item_list_len; i++) {
    panel_show_item_line(panel, i, &(channel->items[i]));
    if (i == 0) panel_show_item_summary(panel, &(channel->items[i]));
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

  panel->item = item;

  return item;
}