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
//  helper: erase channel line
//
static inline void panel_erase_channel_line(PANEL* panel, int16_t pos) {
//  B_PUTMES(1, 0, 1 + pos, 95, "");
  struct TXFILLPTR txf = { 0, 0, (1 + pos) * 16, 768, 16, 0x0000 };
  TXFILL(&txf);
}

//
//  helper: show item line
//
static inline void panel_show_item_line(PANEL* panel, int16_t pos, RSS_ITEM* item) {
  B_PUTMES(1,  0, 1 + pos, 23, item->updated);
  B_PUTMES(1, 24, 1 + pos, 72, item->title);
}

//
//  helper: erase item line
//
static inline void panel_erase_item_line(PANEL* panel, int16_t pos) {
//  B_PUTMES(1, 0, 1 + pos, 95, "");
  struct TXFILLPTR txf = { 0, 0, (1 + pos) * 16, 768, 16, 0x0000 };
  TXFILL(&txf);
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
//  B_PRINT("\r\n\n");
//  B_PRINT(item->author);
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
//  helper: move channel cursor top
//
static void panel_move_channel_cursor_top(PANEL* panel) {
  panel_hide_cursor(panel, panel->cursor_pos);
  panel->cursor_ofs = 0;
  panel->cursor_pos = 0;
  for (int16_t i = 0; i < panel->channel_list_len; i++) {
    if (i < panel->rss->num_channels) { 
      panel_show_channel_line(panel, i, &(panel->rss->channels[i]));
    } else {
      panel_erase_channel_line(panel, i);
    }
  }
  panel_show_cursor(panel, panel->cursor_pos);
}

//
//  helper: move channel cursor bottom
//
static void panel_move_channel_cursor_bottom(PANEL* panel) {
  panel_hide_cursor(panel, panel->cursor_pos);
  if (panel->rss->num_channels < panel->channel_list_len) {
    panel->cursor_ofs = 0;
    panel->cursor_pos = panel->rss->num_channels - 1;
  } else {
    panel->cursor_ofs = panel->rss->num_channels - panel->channel_list_len;
    panel->cursor_pos = panel->channel_list_len - 1;
  }
  for (int16_t i = 0; i < panel->channel_list_len; i++) {
    int16_t channel_index = panel->cursor_ofs + i;
    if (channel_index < panel->rss->num_channels) { 
      panel_show_channel_line(panel, i, &(panel->rss->channels[channel_index]));
    } else {
      panel_erase_channel_line(panel, i);
    }
  }
  panel_show_cursor(panel, panel->cursor_pos);
}

//
//  helper: move channel page up
//
static void panel_move_channel_page_up(PANEL* panel) {
  panel_hide_cursor(panel, panel->cursor_pos);
  int16_t refresh = 0;
  if (panel->rss->num_channels < panel->channel_list_len) {
    // そもそもchannel数が表示行数未満ならカーソルを一番下に移動
    panel->cursor_pos = panel->rss->num_channels - 1;
  } else if (panel->cursor_ofs == panel->rss->num_channels - panel->channel_list_len) {
    // 既に最終ページならばカーソルを一番下に移動
    panel->cursor_pos = panel->rss->num_channels - 1 - panel->cursor_ofs;
  } else if (panel->cursor_ofs + panel->channel_list_len >= panel->rss->num_channels - 1 - panel->channel_list_len) {
    // まるまる1ページ進むと範囲外になってしまう状況なら、ちょうどいい位置にオフセット
    int32_t new_idx = panel->cursor_ofs + panel->cursor_pos + panel->channel_list_len;
    panel->cursor_ofs = panel->rss->num_channels - panel->channel_list_len;
    panel->cursor_pos = new_idx - panel->cursor_ofs;
    if (panel->cursor_pos >= panel->channel_list_len) {
      panel->cursor_pos = panel->channel_list_len - 1;
    }
    refresh = 1;
  } else {
    // 素直に1ページ進む
    panel->cursor_ofs += panel->channel_list_len;
    refresh = 1;
  }
  if (refresh) {
    for (int16_t i = 0; i < panel->channel_list_len; i++) {
      int16_t channel_index = panel->cursor_ofs + i;
      if (channel_index < panel->rss->num_channels) { 
        panel_show_channel_line(panel, i, &(panel->rss->channels[channel_index]));
      } else {
        panel_erase_channel_line(panel, i);
      }
    }
  }
  panel_show_cursor(panel, panel->cursor_pos);
}

//
//  helper: move channel page down
//
static void panel_move_channel_page_down(PANEL* panel) {
  panel_hide_cursor(panel, panel->cursor_pos);
  int16_t refresh = 0;
  if (panel->rss->num_channels < panel->channel_list_len) {
    // そもそもchannel数が表示行数未満ならカーソルを一番上に移動
    panel->cursor_pos = 0;
  } else if (panel->cursor_ofs == 0) {
    // 既に最初のページならばカーソルを一番上に移動
    panel->cursor_pos = 0;
  } else if (panel->cursor_ofs - panel->channel_list_len < 0) {
    // まるまる1ページ戻ると範囲外になってしまう状況なら、ちょうどいい位置にオフセット
    panel->cursor_pos += panel->cursor_ofs - panel->channel_list_len;
    if (panel->cursor_pos < 0) panel->cursor_pos = 0;
    panel->cursor_ofs = 0;
    refresh = 1;
  } else {
    // 素直に1ページ戻る
    panel->cursor_ofs -= panel->channel_list_len;
    refresh = 1;
  }
  if (refresh) {
    for (int16_t i = 0; i < panel->channel_list_len; i++) {
      int16_t channel_index = panel->cursor_ofs + i;
      if (channel_index < panel->rss->num_channels) { 
        panel_show_channel_line(panel, i, &(panel->rss->channels[channel_index]));
      } else {
        panel_erase_channel_line(panel, i);
      }
    }
  }
  panel_show_cursor(panel, panel->cursor_pos);
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
//  helper: move item cursor top
//
static void panel_move_item_cursor_top(PANEL* panel) {
  panel_hide_cursor(panel, panel->cursor_pos);
  panel->cursor_ofs = 0;
  panel->cursor_pos = 0;
  for (int16_t i = 0; i < panel->item_list_len; i++) {
    if (i < panel->channel->num_items) { 
      RSS_ITEM* item = &(panel->channel->items[i]);
      panel_show_item_line(panel, i, item);
      if (i == 0) panel_show_item_summary(panel, item);
    } else {
      panel_erase_item_line(panel, i);
    }
  }
  panel_show_cursor(panel, panel->cursor_pos);
}

//
//  helper: move item cursor bottom
//
static void panel_move_item_cursor_bottom(PANEL* panel) {
  panel_hide_cursor(panel, panel->cursor_pos);
  if (panel->channel->num_items < panel->item_list_len) {
    panel->cursor_ofs = 0;
    panel->cursor_pos = panel->channel->num_items - 1;
  } else {
    panel->cursor_ofs = panel->channel->num_items - panel->item_list_len;
    panel->cursor_pos = panel->item_list_len - 1;
  }
  for (int16_t i = 0; i < panel->item_list_len; i++) {
    int16_t item_index = panel->cursor_ofs + i;
    if (item_index < panel->channel->num_items) { 
      RSS_ITEM* item = &(panel->channel->items[item_index]);
      panel_show_item_line(panel, i, item);
      if (i == 0) panel_show_item_summary(panel, item);
    } else {
      panel_erase_item_line(panel, i);
    }
  }
  panel_show_cursor(panel, panel->cursor_pos);
}

//
//  helper: move item page up
//
static void panel_move_item_page_up(PANEL* panel) {
  panel_hide_cursor(panel, panel->cursor_pos);
  int16_t refresh = 0;
  if (panel->channel->num_items < panel->item_list_len) {
    // そもそもitem数が表示行数未満ならカーソルを一番下に移動
    panel->cursor_pos = panel->channel->num_items - 1;
  } else if (panel->cursor_ofs == panel->channel->num_items - panel->item_list_len) {
    // 既に最終ページならばカーソルを一番下に移動
    panel->cursor_pos = panel->channel->num_items - 1 - panel->cursor_ofs;
  } else if (panel->cursor_ofs + panel->item_list_len >= panel->channel->num_items - 1 - panel->item_list_len) {
    // まるまる1ページ進むと範囲外になってしまう状況なら、ちょうどいい位置にオフセット
    panel->cursor_ofs = panel->channel->num_items - panel->item_list_len;
    refresh = 1;
  } else {
    // 素直に1ページ進む
    panel->cursor_ofs += panel->item_list_len;
    refresh = 1;
  }
  if (refresh) {
    for (int16_t i = 0; i < panel->item_list_len; i++) {
      int16_t item_index = panel->cursor_ofs + i;
      if (item_index < panel->channel->num_items) { 
        RSS_ITEM* item = &(panel->channel->items[item_index]);
        panel_show_item_line(panel, i, item);
        if (i == 0) panel_show_item_summary(panel, item);
      } else {
        panel_erase_item_line(panel, i);
      }
    }
  }
  panel_show_cursor(panel, panel->cursor_pos);
}

//
//  helper: move item page down
//
static void panel_move_item_page_down(PANEL* panel) {
  panel_hide_cursor(panel, panel->cursor_pos);
  int16_t refresh = 0;
  if (panel->channel->num_items < panel->item_list_len) {
    // そもそもitem数が表示行数未満ならカーソルを一番上に移動
    panel->cursor_pos = 0;
  } else if (panel->cursor_ofs == 0) {
    // 既に最初のページならばカーソルを一番上に移動
    panel->cursor_pos = 0;
  } else if (panel->cursor_ofs - panel->item_list_len < 0) {
    // まるまる1ページ戻ると範囲外になってしまう状況なら、ちょうどいい位置にオフセット
    panel->cursor_pos += panel->cursor_ofs - panel->item_list_len;
    if (panel->cursor_pos < 0) panel->cursor_pos = 0;
    panel->cursor_ofs = 0;
    refresh = 1;
  } else {
    // 素直に1ページ戻る
    panel->cursor_ofs -= panel->item_list_len;
    refresh = 1;
  }
  if (refresh) {
    for (int16_t i = 0; i < panel->item_list_len; i++) {
      int16_t item_index = panel->cursor_ofs + i;
      if (item_index < panel->channel->num_items) { 
        RSS_ITEM* item = &(panel->channel->items[item_index]);
        panel_show_item_line(panel, i, item);
        if (i == 0) panel_show_item_summary(panel, item);
      } else {
        panel_erase_item_line(panel, i);
      }
    }
  }
  panel_show_cursor(panel, panel->cursor_pos);
}

//
//  helper: 時計データの更新
//
static void panel_update_clock_info(PANEL* panel) {

  int32_t d = DATEBIN(BINDATEGET());
  int32_t t = TIMEBIN(TIMEGET());

  sprintf(panel->clock_info, "%04d-%02d-%02d %02d:%02d", 
          (d >> 16) & 0xfff,
          (d >> 8) & 0xff,
          (d >> 0) & 0xff,
          (t >> 16) & 0xff,
          (t >> 8) & 0xff);
}

//
//  helper: vdisp interrupt handler for clock display
//
static PANEL* g_panel;
static void __attribute__((interrupt)) panel_vdisp_handler() {

  g_panel->vdisp_counter++;
  g_panel->clock_info[13] = (g_panel->vdisp_counter & 0x01) ? ':' : ' ';
  B_PUTMES(15, 96 - 17, 0, 17, g_panel->clock_info);

  if (g_panel->communication_status) {
    int16_t c = ( g_panel->communication_counter + 1 ) % 32;
    g_panel->communication_counter = c;
    if (c < 16) {
      B_PUTMES(15, 12 + c, 31, 0, ">");
    } else {
      B_PUTMES(15, 12 + c - 16, 31, 0, "_");
    }
  }  
}

//
//  open panel
//
int32_t panel_open(PANEL* panel, RSS* rss, int32_t info_bar_color, char* cut_file_name, int32_t cut_file_color) {

  int32_t rc = 0;

  panel->rss = rss;
  panel->channel = NULL;

  panel->info_bar_color = info_bar_color;
  panel->cut_file_name = cut_file_name;
  panel->cut_file_color = cut_file_color;

  panel->cursor_pos = 0;
  panel->cursor_ofs = 0;

  panel->channel_list_len = 30;
  panel->item_list_len = 11;
  
  // カットファイルの指定があれば子プロセスで cut.r を呼び出す形で表示する
  if (panel->cut_file_name != NULL && panel->cut_file_color != 0) {
    uint8_t cmd[ 256 ];
    sprintf(cmd, "cut.r -gc -p$%x %s", panel->cut_file_color, panel->cut_file_name);
    system(cmd);
  }

  // customize text palette codes (cut.rの後に呼び出す必要がある)
  TPALET(0, (0b00000 << 11) | (0b00000 << 6) | (0b00000) << 1 | 0);   // 背景
  TPALET(1, (0b11111 << 11) | (0b11111 << 6) | (0b11111) << 1 | 1);   // 文字
  TPALET(2, (0b11111 << 11) | (0b11111 << 6) | (0b11111) << 1 | 1);   // カーソル
  TPALET(3, panel->info_bar_color);                                   // インフォメーションバー

  panel_update_clock_info(panel);

  // タイトルバーとステータスバー
  B_PUTMES(15, 0,  0, 95, " RSSN.X - RSS News Reader for X680x0 version " PROGRAM_VERSION);
  B_PUTMES(15, 0, 31, 95, "");

  // VDISP割り込み開始
  panel->vdisp_counter = 0;
  panel->communication_status = 0;
  g_panel = panel;
  if (VDISPST((uint8_t*)panel_vdisp_handler, 0, 55) != 0) {
    printf("error: VDISP interrupt is already used.\n");
    rc = -1;
  }

  return rc;
}

//
//  close panel
//
void panel_close(PANEL* panel) {
  VDISPST(0, 0, 0);
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
//  status bar: communication
//
void panel_show_status_communication(PANEL* panel) {
  B_PUTMES(15, 0, 31, 95, STATUS_BAR_COMMUNICATION);
}

//
//  status bar channel list
//
void panel_show_status_channel_list(PANEL* panel) {
  B_PUTMES(15, 0, 31, 95, STATUS_BAR_CHANNEL_LIST);
}

//
//  status bar item list
//
void panel_show_status_item_list(PANEL* panel) {
  B_PUTMES(15, 0, 31, 95, STATUS_BAR_ITEM_LIST);
}

//
//  list channels
//
int32_t panel_list_channels(PANEL* panel) {

  int32_t rc = CHANNEL_SELECT_OK;

  RSS_CHANNEL* channel = NULL;

  panel_clear(panel);
  panel_show_status_channel_list(panel);
  
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

        case KEY_SCAN_CODE_F10:
        case KEY_SCAN_CODE_ESC:
        case KEY_SCAN_CODE_Q: {
          abort = 1;
          rc = CHANNEL_SELECT_EXIT;
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

        case KEY_SCAN_CODE_ROLLUP:
        case KEY_SCAN_CODE_SPACE:
        case KEY_SCAN_CODE_CTRL_V: {
          panel_move_channel_page_up(panel);
          break;
        }

        case KEY_SCAN_CODE_ROLLDOWN:
        case KEY_SCAN_CODE_B:
        case KEY_SCAN_CODE_CTRL_B: {
          panel_move_channel_page_down(panel);
          break;
        }

        case KEY_SCAN_CODE_F1:
        case KEY_SCAN_CODE_HOME:
        case KEY_SCAN_CODE_SHIFT_COMMA: {
          panel_move_channel_cursor_top(panel);
          break;
        }

        case KEY_SCAN_CODE_F2:
        case KEY_SCAN_CODE_SHIFT_PERIOD: {
          panel_move_channel_cursor_bottom(panel);
          break;
        }

        case KEY_SCAN_CODE_ENTER:
        case KEY_SCAN_CODE_CR: {
          rc = CHANNEL_SELECT_OK;
          channel = &(panel->rss->channels[ panel->cursor_ofs + panel->cursor_pos ]);
          abort = 1;
          break;
        }
      }
    } else {

      // やることない時は時計データの更新でもやっておく
      panel_update_clock_info(panel);

    }
  }

  panel_hide_cursor(panel, panel->cursor_pos);

  panel->channel = channel;

  return rc;
}

//
//  list channel items
//
int32_t panel_list_channel_items(PANEL* panel) {

  int32_t rc = ITEM_SELECT_OK;

  RSS_CHANNEL* channel = panel->channel;

  panel_clear(panel);
  panel_show_status_item_list(panel);

  char channel_info_bar[ 256 ];
  sprintf(channel_info_bar, " %s [%d] - %s", channel->title, channel->num_items, channel->link);
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
          rc = ITEM_SELECT_OK;
          break;
        }

        case KEY_SCAN_CODE_F10: {
          abort = 1;
          rc = ITEM_SELECT_EXIT;
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

        case KEY_SCAN_CODE_ROLLUP:
        case KEY_SCAN_CODE_SPACE:
        case KEY_SCAN_CODE_CTRL_V: {
          panel_move_item_page_up(panel);
          break;
        }

        case KEY_SCAN_CODE_ROLLDOWN:
        case KEY_SCAN_CODE_B:
        case KEY_SCAN_CODE_CTRL_B: {
          panel_move_item_page_down(panel);
          break;
        }

        case KEY_SCAN_CODE_F1:
        case KEY_SCAN_CODE_HOME:
        case KEY_SCAN_CODE_SHIFT_COMMA: {
          panel_move_item_cursor_top(panel);
          break;
        }

        case KEY_SCAN_CODE_F2:
        case KEY_SCAN_CODE_SHIFT_PERIOD: {
          panel_move_item_cursor_bottom(panel);
          break;
        }

        case KEY_SCAN_CODE_F5: {
          abort = 1;
          rc = ITEM_SELECT_RELOAD;
          break;
        }

        case KEY_SCAN_CODE_LEFT: {
          abort = 1;
          rc = ITEM_SELECT_PREV_CHANNEL;
          break;
        }

        case KEY_SCAN_CODE_RIGHT: {
          abort = 1;
          rc = ITEM_SELECT_NEXT_CHANNEL;
          break;
        }

      }
    } else {

      // やることない時は時計データの更新でもやっておく
      panel_update_clock_info(panel);

    }
  }

  panel_hide_cursor(panel, panel->cursor_pos);

  return rc;
}

void panel_set_communication_status(PANEL* panel, int16_t status) {

  if (status) {
    panel_show_status_communication(panel);
  }

  panel->communication_counter = 0;
  panel->communication_status = status;

}