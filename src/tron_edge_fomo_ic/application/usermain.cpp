// 修正ポイント1：C言語で書かれたOSやライブラリのヘッダファイルを extern "C" で囲む
extern "C" {
#ifdef _B
#undef _B
#endif
#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include "dave_driver.h"
#include "../ra/fsp/src/r_drw/r_drw_base.h" // D1ドライバの型定義をインクルード
extern d2_device * d2_handle;
void R_BSP_SdramInit(bool init_memory); // SDRAM初期化関数のプロトタイプ宣言

// FSPの空のキャッシュ操作関数をオーバーライドして、データキャッシュの同期を実行する
d1_int_t d1_cacheflush (d1_device * handle, d1_int_t memtype)
{
    (void)handle;
    (void)memtype;
    SCB_CleanDCache();
    return 1;
}

d1_int_t d1_cacheblockflush (d1_device * handle, d1_int_t memtype, const void * ptr, d1_uint_t size)
{
    (void)handle;
    (void)memtype;
    SCB_CleanDCache_by_Addr((void *)ptr, (int32_t)size);
    return 1;
}
}

#include "hal_data.h"

// 3つ目のフレームバッファを追加 (トリプルバッファ用。1.2MB分を外部SDRAMセクションに確保)
uint8_t fb_background_2[DISPLAY_BUFFER_STRIDE_BYTES_INPUT0 * DISPLAY_VSIZE_INPUT0] BSP_ALIGN_VARIABLE(64) BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");



// Vblank（垂直同期）検出用フラグとカウンタ
static volatile bool vblank_flag = false;
static volatile uint32_t vblank_interrupt_count = 0;

ID tskid_1;                          // Task ID number (global)
ID tskid_2;                          // Task ID number (global)
ID tskid_ai;                         // AI Task ID

#include "ai_inference.h"
#include "bg_font_18_full.h"
#include <stdio.h>
extern volatile bool g_ai_task_busy;

extern "C" {
extern uint8_t * p_camera_capture_buffer_stored;
}
#include "camera_layer.h"

// LCD GLCDC コールバック関数 (Vblank割り込み時に実行)
extern "C" void lcd_glcdc_callback(display_callback_args_t * p_args)
{
    if (p_args->event == DISPLAY_EVENT_LINE_DETECTION)
    {
        vblank_flag = true;
        vblank_interrupt_count++;
    }
}



LOCAL void task_1(INT stacd, void *exinf); // task execution function

LOCAL T_CTSK ctsk_1 = {
    .exinf   = NULL,               // 1. 拡張情報
    .tskatr  = TA_HLNG | TA_RNG3,  // 2. タスク属性
    .task    = (FP)task_1,         // 3. タスク関数
    .itskpri = 10,                 // 4. 優先度
    .stksz   = 32768,              // 5. スタックサイズ (32KBに拡張してスタックオーバーフローを防止)
    .bufptr  = NULL                // 6. スタックバッファポインタ
};

LOCAL void task_2(INT stacd, void *exinf); // task execution function

LOCAL T_CTSK ctsk_2 = {
    .exinf   = NULL,
    .tskatr  = TA_HLNG | TA_RNG3,
    .task    = (FP)task_2,
    .itskpri = 10,
    .stksz   = 1024,
    .bufptr  = NULL
};

LOCAL T_CTSK ctsk_ai = {
    .exinf   = NULL,
    .tskatr  = TA_HLNG | TA_RNG3,
    .task    = (FP)ai_inference_task,
    .itskpri = 11,                 // UI描画タスク(10)より低い優先度
    .stksz   = 32768,              // TFLite Micro用に32KBスタックを確保
    .bufptr  = NULL
};

LOCAL void task_1(INT stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;

    // タスク起動の直後に1000ms (1〜2秒) のウェイトを入れます (コールドスタート時の電源安定化対策)
    tk_dly_tsk(1000);

    // 外部SDRAMの初期化を実行
    R_BSP_SdramInit(true);

    // 液晶用追加ピン構成(g_bsp_pin_cfg_glcd)を適用して、液晶用周辺機能ピンを有効化
    R_IOPORT_PinsCfg(&g_ioport_ctrl, &g_bsp_pin_cfg_glcd);

    // [デバッグ] SDRAM 物理メモリアクセステスト
    tm_printf((UB *)"Testing physical SDRAM at address 0x%08X...\n", (uint32_t)&fb_background[0][0]);
    volatile uint32_t *p_sdram = (volatile uint32_t *)&fb_background[0][0];
    bool sdram_ok = true;
    for (uint32_t i = 0; i < 1024; i++)
    {
        p_sdram[i] = 0x55AA55AA ^ i;
    }
    SCB_CleanInvalidateDCache_by_Addr((void *)&fb_background[0][0], 1024 * sizeof(uint32_t));
    for (uint32_t i = 0; i < 1024; i++)
    {
        if (p_sdram[i] != (0x55AA55AA ^ i))
        {
            tm_printf((UB *)"SDRAM verification failed at index %d! Read: 0x%08X, Expected: 0x%08X\n", i, p_sdram[i], 0x55AA55AA ^ i);
            sdram_ok = false;
            break;
        }
    }
    if (sdram_ok)
    {
        tm_printf((UB *)"SDRAM verification SUCCESS!\n");
    }
    else
    {
        tm_printf((UB *)"ERROR: SDRAM physical memory write/read failed!\n");
    }
    SCB_CleanInvalidateDCache_by_Addr((void *)&fb_background[0][0], 1024 * sizeof(uint32_t));

    // [重要] キャッシュの完全クリーン＆無効化 (コールドブート時のキャッシュ汚染対策)
    // これまでにCPUキャッシュに入り込んだ可能性のあるダーティラインをすべて物理メモリへ書き戻し、
    // キャッシュをクリーンな状態に初期化して、DMA（D/AVE 2D, GLCDC）との不整合を防ぎます。
    SCB_CleanInvalidateDCache();

    tm_printf((UB *)"\n=== Camera MIPI-CSI2 & LCD Display D2D Start ===\n");

    // [重要] AXIバス調停（Arbitration）の設定 (GLCDC最優先・固定優先)
    tm_printf((UB *)"=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===\n");
    for (int i = 0; i < 18; i++)
    {
        // ARBMET = 1 (Fixed Priority), ARBS = 3 (Group 3) -> 0x0013
        R_BUS->BUSS[i].CNT = 0x0013;
    }

    // [重要] フレームバッファのゼロクリア (起動時の液晶同期ロスト防止対策)
    tm_printf((UB *)"Clearing SDRAM framebuffers to black...\n");
    memset(fb_background, 0, sizeof(fb_background));
    memset(fb_background_2, 0, sizeof(fb_background_2));
    
    // クリアしたデータを確実に物理SDRAMへ書き戻す
    SCB_CleanInvalidateDCache();

    // ----------------------------------------------------
    // [1] LCD ディスプレイ (GLCDC) の初期化とバックライトON
    // ----------------------------------------------------
    tm_printf((UB *)"Initializing LCD (GLCDC)... \n");
    
    // MIPIインターフェーススイッチを有効化 (ボード上のマルチプレクサをMIPI側に接続する)
    R_IOPORT_PinWrite(&g_ioport_ctrl, MIPI_IF_EN, BSP_IO_LEVEL_LOW);

    // 液晶パネルのリセットシーケンス
    R_IOPORT_PinWrite(&g_ioport_ctrl, LCD_RST, BSP_IO_LEVEL_HIGH);
    tk_dly_tsk(500);
    R_IOPORT_PinWrite(&g_ioport_ctrl, LCD_RST, BSP_IO_LEVEL_LOW);
    tk_dly_tsk(200);
    R_IOPORT_PinWrite(&g_ioport_ctrl, LCD_RST, BSP_IO_LEVEL_HIGH);
    tk_dly_tsk(500);

    fsp_err_t err = R_GLCDC_Open(&g_lcd_glcdc_ctrl, &g_lcd_glcdc_cfg);
    if (FSP_SUCCESS != err)
    {
        tm_printf((UB *)"ERROR: Failed to open GLCDC (0x%x)\n", err);
        while(1) { tk_dly_tsk(1000); }
    }
    
    err = R_GLCDC_Start(&g_lcd_glcdc_ctrl);
    if (FSP_SUCCESS != err)
    {
        tm_printf((UB *)"ERROR: Failed to start GLCDC (0x%x)\n", err);
        while(1) { tk_dly_tsk(1000); }
    }
    
    uint8_t *const my_fb[3] = {
        (uint8_t *)&fb_background[0][0],
        (uint8_t *)&fb_background[1][0],
        (uint8_t *)&fb_background_2[0]
    };
    R_GLCDC_BufferChange(&g_lcd_glcdc_ctrl, my_fb[0], DISPLAY_FRAME_LAYER_1);

    R_IOPORT_PinWrite(&g_ioport_ctrl, LCD_BLEN, BSP_IO_LEVEL_HIGH);
    tm_printf((UB *)"LCD Backlight enabled.\n");

    // ----------------------------------------------------
    // [2] D/AVE 2D (D2D) グラフィックスエンジンの初期化
    // ----------------------------------------------------
    tm_printf((UB *)"Initializing D/AVE 2D Graphics Engine...\n");
    d2_handle = d2_opendevice(0);
    if (NULL == d2_handle)
    {
        tm_printf((UB *)"ERROR: Failed to open D/AVE 2D engine\n");
        while(1) { tk_dly_tsk(1000); }
    }
    d2_inithw(d2_handle, 0);
    
    d2_setblendmode(d2_handle, d2_bm_one, d2_bm_zero);
    d2_setalphamode(d2_handle, d2_am_constant);
    d2_setalpha(d2_handle, 0xff);
    d2_setantialiasing(d2_handle, 0);

    // ----------------------------------------------------
    // [3] MIPI-CSI2 カメラ (OV5640) の初期化
    // ----------------------------------------------------
    tm_printf((UB *)"Initializing MIPI-CSI2 Camera (OV5640)... \n");
    err = camera_init(false); // false: 実画像キャプチャモード (trueならカラーバーテスト)
    if (FSP_SUCCESS != err)
    {
        tm_printf((UB *)"ERROR: Failed to initialize camera (0x%x)\n", err);
        while(1) { tk_dly_tsk(1000); }
    }
    
    camera_image_buffer_initialize();
    camera_capture_start();
    tm_printf((UB *)"SUCCESS: Camera initialized and capture started.\n");

    uint8_t draw_buf = 0;
    uint8_t pending_buf = 1;
    uint8_t display_buf = 2;
    int loop_cnt = 0;

    tm_printf((UB *)"Entering Real-time Camera Display Loop...\n");

    while (1)
    {
        // [重要] GLCDCのVblank（垂直同期）割り込み（lcd_glcdc_callback内のtk_wup_tsk）が入るまでスリープ待機
        // これにより、ビジーウェイトや無駄なポーリングを行わず、CPU負荷0%で液晶リフレッシュに完全同期します。
        tk_can_wup(TSK_SELF);
        tk_slp_tsk(100);

        if (loop_cnt % 100 == 0)
        {
            tm_printf((UB *)"Loop %d: buffer = 0x%08X, vsync_cnt = %u\n", loop_cnt, (uint32_t)p_camera_capture_buffer_stored, (unsigned int)vblank_interrupt_count);
            if (p_camera_capture_buffer_stored != NULL)
            {
                // デバッグ用に先頭16バイトのキャッシュを無効化して読み出し
                SCB_InvalidateDCache_by_Addr((void *)p_camera_capture_buffer_stored, 32);
                uint16_t *p_words = (uint16_t *)p_camera_capture_buffer_stored;
                tm_printf((UB *)"  Buf Data: 0x%04X 0x%04X 0x%04X 0x%04X 0x%04X 0x%04X 0x%04X 0x%04X\n",
                          p_words[0], p_words[1], p_words[2], p_words[3],
                          p_words[4], p_words[5], p_words[6], p_words[7]);
            }
        }

        if (p_camera_capture_buffer_stored != NULL)
        {

            // 新しいフレームの D/AVE 2D 描画開始
            d2_startframe(d2_handle);

            // 描画対象のフレームバッファを設定
            d2_framebuffer(d2_handle, my_fb[draw_buf], DISPLAY_HSIZE_INPUT0, DISPLAY_HSIZE_INPUT0, DISPLAY_VSIZE_INPUT0, d2_mode_rgb565);

            // 画面全体のクリアは重くバス帯域を圧迫してノイズの原因になるため廃止し、
            // カメラ画像が上書きしない左端の余白エリア（X:0〜111、Y:0〜480）だけを部分的に黒で塗りつぶしクリアする
            d2_setcolor(d2_handle, 0, 0xFF000000); // 黒色
            d2_renderbox(d2_handle, (d2_point)(0 << 4), (d2_point)(0 << 4), (d2_width)(112 << 4), (d2_width)(480 << 4));

            // 320x240のカメラ画像を、D/AVE 2Dで800x600に拡大して画面中央に描画 (X=112)
            d2_setblitsrc(d2_handle, (void *)p_camera_capture_buffer_stored, 320, 320, 240, d2_mode_rgb565);
            d2_blitcopy(d2_handle,
                        320, 240,
                        0, 0,
                        800 << 4, 600 << 4,  // 拡大後幅・高 (16.4固定少数)
                        112 << 4, 0 << 4,    // 中央寄せX・Y座標 (16.4固定少数)
                        d2_tm_filter);       // バイリニアフィルタを有効化して美しく拡大

            // --- 顔検出枠（Bounding Box）の重ね描き ---
            d2_setcolor(d2_handle, 0, 0xFF00FF00); // 明るい緑色
            for (int i = 0; i < AI_MAX_DETECTION_NUM; i++)
            {
                if (g_ai_detection[i].m_w > 0 && g_ai_detection[i].m_h > 0)
                {
                    // 192x192 座標系から LCD 画面上の 800x600 エリア（X=112オフセット）への変換
                    float fx = (float)g_ai_detection[i].m_x * 3.125f + 212.0f;
                    float fy = (float)g_ai_detection[i].m_y * 3.125f;
                    float fw = (float)g_ai_detection[i].m_w * 3.125f;
                    float fh = (float)g_ai_detection[i].m_h * 3.125f;
                    
                    d2_point x1 = (d2_point)(fx * 16.0f);
                    d2_point y1 = (d2_point)(fy * 16.0f);
                    d2_point x2 = (d2_point)((fx + fw) * 16.0f);
                    d2_point y2 = (d2_point)((fy + fh) * 16.0f);
                    
                    d2_width border_width = 2 * 16; // 太さ 2px
                    
                    d2_renderline(d2_handle, x1, y1, x2, y1, border_width, 0); // 上
                    d2_renderline(d2_handle, x2, y1, x2, y2, border_width, 0); // 右
                    d2_renderline(d2_handle, x2, y2, x1, y2, border_width, 0); // 下
                    d2_renderline(d2_handle, x1, y2, x1, y1, border_width, 0); // 左

                    // 確信度(値)の描画
                    char val_str[16];
                    sprintf(val_str, "%d%%", g_ai_detection[i].m_val_percent);
                    int text_y = (int)fy - 20;
                    if (text_y < 10) text_y = (int)(fy + 10);
                    print_bg_font_18(d2_handle, (d2_point)fx, (d2_point)text_y, 1.0f, val_str);
                }
            }

            // AIトータル処理時間（検出時間）と検出されたICの数の描画
            int ic_count = 0;
            for (int i = 0; i < AI_MAX_DETECTION_NUM; i++)
            {
                if (g_ai_detection[i].m_w > 0 && g_ai_detection[i].m_h > 0)
                {
                    ic_count++;
                }
            }

            char time_str[32];
            sprintf(time_str, "AI Time: %lu ms", g_ai_inference_time_ms);
            
            char count_str[32];
            sprintf(count_str, "IC     : %d", ic_count);

            d2_setcolor(d2_handle, 0, 0xFF00FF00); // 明るい緑色
            print_bg_font_18(d2_handle, (d2_point)120, (d2_point)10, 1.0f, time_str);
            print_bg_font_18(d2_handle, (d2_point)120, (d2_point)35, 1.0f, count_str);

            // 描画コマンド終了
            d2_endframe(d2_handle);

            // [重要] 描画コマンドリストを確実に物理メモリへ書き出す
            SCB_CleanInvalidateDCache();

            // GPUによる描画処理が物理メモリ上で完了するまでブロック待機
            d2_flushframe(d2_handle);

            // [重要] GPUが更新したステータスやデータをキャッシュと同期する
            SCB_CleanInvalidateDCache();

            // GLCDCに対して、新しく描き終わったバッファへの切り替えを要求
            fsp_err_t glcdc_err = R_GLCDC_BufferChange(&g_lcd_glcdc_ctrl, my_fb[draw_buf], DISPLAY_FRAME_LAYER_1);
            if (FSP_SUCCESS != glcdc_err)
            {
                tm_printf((UB *)"GLCDC BufferChange Error: %d\n", glcdc_err);
            }

            // AI推論タスクへの起床通知 (ビジーでない場合のみ通知してフレームスキップ)
            if (!g_ai_task_busy)
            {
                tk_wup_tsk(tskid_ai);
            }
        }

        // [不要] カメラは連続キャプチャかつトリプルバッファモードで動作しているため、
        // 毎フレーム再始動する必要はありません（再始動処理がDMAと衝突してフリーズの原因になります）。
        // camera_capture_start();

        // バッファインデックスのローテーション (トリプルバッファリング)
        uint8_t next_draw = display_buf;
        uint8_t next_pending = draw_buf;
        uint8_t next_display = pending_buf;

        draw_buf = next_draw;
        pending_buf = next_pending;
        display_buf = next_display;

        loop_cnt++;
    }
}

LOCAL void task_2(INT stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;

    while (1) {
        tm_printf((UB *)"task 2 running...\n");
        tk_dly_tsk(7000);
    }
}

extern "C" EXPORT INT usermain(void)
{
    tm_putstring((UB *)"Start User-main program (Camera & LCD D2D Test).\n");

    /* Create & Start Tasks */
    tskid_1 = tk_cre_tsk(&ctsk_1);
    tk_sta_tsk(tskid_1, 0);

    tskid_2 = tk_cre_tsk(&ctsk_2);
    tk_sta_tsk(tskid_2, 0);

    tskid_ai = tk_cre_tsk(&ctsk_ai);
    tk_sta_tsk(tskid_ai, 0);

    tk_slp_tsk(TMO_FEVR);
    return 0;
}
