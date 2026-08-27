// 修正ポイント1：C言語で書かれたOSやライブラリのヘッダファイルを extern "C" で囲む
extern "C" {
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

// I2C コールバックイベント受信用変数
static volatile i2c_master_event_t i2c_event = (i2c_master_event_t)0;

// 3つ目のフレームバッファを追加 (トリプルバッファ用。1.2MB分を外部SDRAMセクションに確保)
uint8_t fb_background_2[DISPLAY_BUFFER_STRIDE_BYTES_INPUT0 * DISPLAY_VSIZE_INPUT0] BSP_ALIGN_VARIABLE(64) BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");

// C言語のリンケージを持つコールバック関数
extern "C" void g_cam_i2c_master_user_callback(i2c_master_callback_args_t * p_args)
{
    i2c_event = p_args->event;
}

// Vblank（垂直同期）検出用フラグとカウンタ
static volatile bool vblank_flag = false;
static volatile uint32_t vblank_interrupt_count = 0;

LOCAL ID tskid_1;                          // Task ID number
LOCAL ID tskid_2;                          // Task ID number

// LCD GLCDC コールバック関数 (Vblank割り込み時に実行)
extern "C" void lcd_glcdc_callback(display_callback_args_t * p_args)
{
    if (p_args->event == DISPLAY_EVENT_LINE_DETECTION)
    {
        vblank_flag = true;
        vblank_interrupt_count++;
        // [重要] Vblank割り込みが入った瞬間に、超高速で描画タスク(task_1)を起こす
        tk_wup_tsk(tskid_1);
    }
}

// I2Cの完了イベントを待つヘルパー関数
static fsp_err_t wait_i2c_event(void)
{
    volatile uint32_t timeout = 1000000;
    while (i2c_event == (i2c_master_event_t)0 && timeout > 0)
    {
        timeout--;
    }
    
    if (timeout == 0)
    {
        return FSP_ERR_TIMEOUT;
    }
    
    if (i2c_event != I2C_MASTER_EVENT_ABORTED)
    {
        i2c_event = (i2c_master_event_t)0;
        return FSP_SUCCESS;
    }
    
    i2c_event = (i2c_master_event_t)0;
    return FSP_ERR_TRANSFER_ABORTED;
}

// 16ビットレジスタアドレスから8ビットデータを読み出す関数
static bool rdSensorReg16_8(uint16_t regID, uint8_t *regDat)
{
    fsp_err_t err;
    uint8_t data[2] = {(uint8_t)(regID >> 8), (uint8_t)regID};
    
    i2c_event = (i2c_master_event_t)0;
    err = R_IIC_MASTER_Write(&g_cam_i2c_master_ctrl, data, 2, true); // restart = true
    if (FSP_SUCCESS == err)
    {
        err = wait_i2c_event();
    }
    
    if (FSP_SUCCESS == err)
    {
        i2c_event = (i2c_master_event_t)0;
        err = R_IIC_MASTER_Read(&g_cam_i2c_master_ctrl, regDat, 1, false);
        if (FSP_SUCCESS == err)
        {
            err = wait_i2c_event();
        }
    }
    return (FSP_SUCCESS == err);
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

LOCAL void task_1(INT stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;

    // 電源ON直後のマイコンおよび液晶パネル周辺回路の電源電圧の安定化を待つため、
    // タスク起動の直後に500ms (2ms * 250) のウェイトを入れます (コールドスタート対策)
    tk_dly_tsk(250);

    // 外部SDRAMの初期化を実行
    R_BSP_SdramInit(true);

    // ----------------------------------------------------
    // [重要] キャッシュの完全クリーン＆無効化 (コールドブート時のキャッシュ汚染対策)
    // ----------------------------------------------------
    // これまでにCPUキャッシュに入り込んだ可能性のあるダーティラインをすべて物理メモリへ書き戻し、
    // キャッシュをクリーンな状態に初期化して、DMA（D/AVE 2D, GLCDC）との不整合を防ぎます。
    SCB_CleanInvalidateDCache();

    tm_printf((UB *)"\n=== Camera I2C & LCD D2D Connection Test Start ===\n");

    // ----------------------------------------------------
    // [1] I2C カメラ接続確認テスト (前のフェーズと同様)
    // ----------------------------------------------------
    tm_printf((UB *)"Resetting Camera (CAMERA_RESET -> P709)...\n");
    R_IOPORT_PinWrite(&g_ioport_ctrl, CAMERA_RESET, BSP_IO_LEVEL_LOW);
    tk_dly_tsk(100);
    R_IOPORT_PinWrite(&g_ioport_ctrl, CAMERA_RESET, BSP_IO_LEVEL_HIGH);
    tk_dly_tsk(10);

    tm_printf((UB *)"Starting GPT Clock for Camera XCLK (g_cam_clk)...\n");
    fsp_err_t err = R_GPT_Open(&g_cam_clk_ctrl, &g_cam_clk_cfg);
    if (FSP_SUCCESS == err)
    {
        R_GPT_Start(&g_cam_clk_ctrl);
    }

    tm_printf((UB *)"Opening I2C Master (g_cam_i2c_master)...\n");
    err = R_IIC_MASTER_Open(&g_cam_i2c_master_ctrl, &g_cam_i2c_master_cfg);
    if (FSP_SUCCESS == err)
    {
        R_IIC_MASTER_SlaveAddressSet(&g_cam_i2c_master_ctrl, 0x3C, I2C_MASTER_ADDR_MODE_7BIT);
        
        tm_printf((UB *)"Reading OV5640 Product ID registers via I2C...\n");
        uint8_t pid_h = 0;
        uint8_t pid_l = 0;
        bool read_success = rdSensorReg16_8(0x300a, &pid_h) && rdSensorReg16_8(0x300b, &pid_l);

        if (read_success)
        {
            tm_printf((UB *)"Product ID Read: H = 0x%02X, L = 0x%02X\n", pid_h, pid_l);
            if (pid_h == 0x56 && (pid_l == 0x40 || pid_l == 0x41 || pid_l == 0x4C))
            {
                tm_printf((UB *)"SUCCESS: Camera connection verified! (OV5640 detected)\n");
            }
            else
            {
                tm_printf((UB *)"ERROR: Product ID mismatch\n");
            }
        }
        else
        {
            tm_printf((UB *)"ERROR: Camera communication failed.\n");
        }
    }

    // ----------------------------------------------------
    // [1.5] SDRAM セルフテスト (物理メモリ検証)
    // ----------------------------------------------------
    tm_printf((UB *)"Testing physical SDRAM at address 0x%08X...\n", (uint32_t)&fb_background[0][0]);
    volatile uint32_t *p_sdram = (volatile uint32_t *)&fb_background[0][0];
    bool sdram_ok = true;
    for (uint32_t i = 0; i < 1024; i++)
    {
        p_sdram[i] = 0x55AA55AA ^ i;
    }
    
    // 書き込んだデータをキャッシュから物理メモリにフラッシュ & キャッシュ無効化
    SCB_CleanInvalidateDCache_by_Addr((void *)&fb_background[0][0], 1024 * sizeof(uint32_t));

    // 物理メモリから直接読み出して比較
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

    // テスト領域のキャッシュを再度クリーンにする
    SCB_CleanInvalidateDCache_by_Addr((void *)&fb_background[0][0], 1024 * sizeof(uint32_t));

    // ----------------------------------------------------
    // [重要] AXIバス調停（Arbitration）の設定 (GLCDC最優先・固定優先)
    // ----------------------------------------------------
    tm_printf((UB *)"=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===\n");
    for (int i = 0; i < 18; i++)
    {
        // ARBMET = 1 (Fixed Priority), ARBS = 3 (Group 3) -> 0x0013
        R_BUS->BUSS[i].CNT = 0x0013;
    }

    // ----------------------------------------------------
    // [重要] フレームバッファのゼロクリア (起動時の液晶同期ロスト防止対策)
    // ----------------------------------------------------
    tm_printf((UB *)"Clearing SDRAM framebuffers to black...\n");
    memset(fb_background, 0, sizeof(fb_background));
    memset(fb_background_2, 0, sizeof(fb_background_2));
    
    // クリアしたデータを確実に物理SDRAMへ書き戻す
    SCB_CleanInvalidateDCache();

    // ----------------------------------------------------
    // [2] LCD ディスプレイ & D/AVE 2D (D2D) テスト
    // ----------------------------------------------------
    tm_printf((UB *)"Initializing LCD (GLCDC)... \n");
    
    // [重要] 液晶パネルの堅牢なハードウェアリセットシーケンス
    // 1. まずリセットピンを High にして、電源供給レールと電位を合わせる
    R_IOPORT_PinWrite(&g_ioport_ctrl, DISP_RESET, BSP_IO_LEVEL_HIGH);
    tk_dly_tsk(500); // 500ms待機して、電源電圧(VCC/VDD)が100%安定するのを待つ
 
    // 2. リセットピンを Low に駆動して、明示的なリセット状態に入る
    R_IOPORT_PinWrite(&g_ioport_ctrl, DISP_RESET, BSP_IO_LEVEL_LOW);
    tk_dly_tsk(200); // 200ms間 Low を維持して、内部デジタルコアを完全にリセット
 
    // 3. リセットピンを High に戻して、液晶内部の起動シーケンスを開始させる
    R_IOPORT_PinWrite(&g_ioport_ctrl, DISP_RESET, BSP_IO_LEVEL_HIGH);
    tk_dly_tsk(500); // 500ms待機し、液晶パネル内部の発振器やPLLが完全にロックするのを待つ
 
    // GLCDC オープンとスタート
    err = R_GLCDC_Open(&g_lcd_glcdc_ctrl, &g_lcd_glcdc_cfg);
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
    
    // 最初のバッファとして設定
    uint8_t *const my_fb[3] = {
        (uint8_t *)&fb_background[0][0],
        (uint8_t *)&fb_background[1][0],
        (uint8_t *)&fb_background_2[0]
    };
    R_GLCDC_BufferChange(&g_lcd_glcdc_ctrl, my_fb[0], DISPLAY_FRAME_LAYER_1);
 
    // バックライトON (DISP_BLEN -> P514)
    R_IOPORT_PinWrite(&g_ioport_ctrl, DISP_BLEN, BSP_IO_LEVEL_HIGH);
    tm_printf((UB *)"LCD Backlight enabled.\n");
 
    // D/AVE 2D ドライバのオープンと初期化
    tm_printf((UB *)"Initializing D/AVE 2D Graphics Engine...\n");
    d2_handle = d2_opendevice(0);
    if (NULL == d2_handle)
    {
        tm_printf((UB *)"ERROR: Failed to open D/AVE 2D engine\n");
        while(1) { tk_dly_tsk(1000); }
    }
    d2_inithw(d2_handle, 0);
 
    // 2D描画パラメータ設定 (バス負荷軽減のためコピーモード・アンチエイリアス無効に設定)
    d2_setblendmode(d2_handle, d2_bm_one, d2_bm_zero);
    d2_setalphamode(d2_handle, d2_am_constant);
    d2_setalpha(d2_handle, 0xff);
    d2_setantialiasing(d2_handle, 0);
 
    // Bouncing Ball 描画ループ用変数
    int ball_x = 200;
    int ball_y = 200;
    int ball_dx = 10;
    int ball_dy = 8;
    int ball_r = 40;
 
    tm_printf((UB *)"Starting D/AVE 2D Rendering Loop (SDRAM Triple Buffer Bouncing Ball)...\n");
 
    uint8_t draw_buf = 0;
    uint8_t pending_buf = 1;
    uint8_t display_buf = 2;
    int loop_cnt = 0;
 
    while (1)
    {
        if (loop_cnt % 100 == 0)
        {
            tm_printf((UB *)"Loop %d: rendering bouncing ball... (Vblank IRQs: %d)\n", loop_cnt, vblank_interrupt_count);
        }
 
        // [重要] 新しいフレームの描画コマンドリストを開始
        d2_startframe(d2_handle);
 
        // 描画対象のフレームバッファを設定
        d2_framebuffer(d2_handle, my_fb[draw_buf], DISPLAY_HSIZE_INPUT0, DISPLAY_HSIZE_INPUT0, DISPLAY_VSIZE_INPUT0, d2_mode_rgb565);
 
        // 画面全体のクリア (黒に近い濃紺: ARGB値 0xFF10101C)
        d2_clear(d2_handle, 0xFF10101C);
 
        // 枠線の描画 (液晶パネルの額縁/オーバースキャンによる隠れを防ぐため、30ピクセル内側に描画)
        d2_setcolor(d2_handle, 0, 0xFF00FF00);
        d2_renderline(d2_handle, 30 << 4, 30 << 4, (DISPLAY_HSIZE_INPUT0 - 30) << 4, 30 << 4, 2 << 4, 0); // 上線
        d2_renderline(d2_handle, 30 << 4, (DISPLAY_VSIZE_INPUT0 - 30) << 4, (DISPLAY_HSIZE_INPUT0 - 30) << 4, (DISPLAY_VSIZE_INPUT0 - 30) << 4, 2 << 4, 0); // 下線
        d2_renderline(d2_handle, 30 << 4, 30 << 4, 30 << 4, (DISPLAY_VSIZE_INPUT0 - 30) << 4, 2 << 4, 0); // 左線
        d2_renderline(d2_handle, (DISPLAY_HSIZE_INPUT0 - 30) << 4, 30 << 4, (DISPLAY_HSIZE_INPUT0 - 30) << 4, (DISPLAY_VSIZE_INPUT0 - 30) << 4, 2 << 4, 0); // 右線
 
        // 物理計算 (跳ね返り)
        ball_x += ball_dx;
        ball_y += ball_dy;
 
        // 壁とのコリジョンチェック (新枠線 30px 境界に合わせる、ボール半径40pxのため衝突判定は 30 + 2 = 32px に設定)
        if (ball_x - ball_r < 32 || ball_x + ball_r > (int)DISPLAY_HSIZE_INPUT0 - 32)
        {
            ball_dx = -ball_dx;
            ball_x += ball_dx;
        }
        if (ball_y - ball_r < 32 || ball_y + ball_r > (int)DISPLAY_VSIZE_INPUT0 - 32)
        {
            ball_dy = -ball_dy;
            ball_y += ball_dy;
        }
 
        // 跳ね返る円を描画 (黄色: ARGB値 0xFFFFFF00)
        d2_setcolor(d2_handle, 0, 0xFFFFFF00);
        d2_rendercircle(d2_handle, ball_x << 4, ball_y << 4, ball_r << 4, 0); // 0 は塗りつぶし
 
        // 静的な他の図形もいくつか描画
        // 赤い円 (ARGB値 0xFFFF0000)
        d2_setcolor(d2_handle, 0, 0xFFFF0000);
        d2_rendercircle(d2_handle, 300 << 4, 300 << 4, 60 << 4, 0);
 
        // 青い矩形 (ARGB値 0xFF0000FF)
        d2_setcolor(d2_handle, 0, 0xFF0000FF);
        d2_renderbox(d2_handle, 700 << 4, 200 << 4, 150 << 4, 150 << 4);
 
        // [重要] フレームの描画終了（描画コマンドの録画を完了）
        d2_endframe(d2_handle);
 
        // [重要] 描画コマンドリストを確実に物理SRAMへ書き出す (DMA読み出し対策)
        SCB_CleanInvalidateDCache();
 
        // [重要] D/AVE 2D ハードウェアの描画実行が完全に完了するまでCPUをブロック待機
        d2_flushframe(d2_handle);
 
        // [重要] GPUが描き込んだ物理メモリ(SDRAM)上のピクセルとキャッシュを完全に同期
        SCB_CleanInvalidateDCache();
 
        // 各フレームでバッファ切り替え要求を送信
        fsp_err_t glcdc_err = R_GLCDC_BufferChange(&g_lcd_glcdc_ctrl, my_fb[draw_buf], DISPLAY_FRAME_LAYER_1);
        if (FSP_SUCCESS != glcdc_err)
        {
            tm_printf((UB *)"GLCDC BufferChange Error: %d\n", glcdc_err);
        }
 
        // [重要] Vblank割り込みが入るまで、タスクをスリープさせて待つ (CPU負荷0、完全同期)
        vblank_flag = false;
        tk_can_wup(TSK_SELF); // 過去にキューに溜まった古い起床要求をクリアして、確実に次のVblankを待つようにする
        tk_slp_tsk(100); // 割り込みコールバック内の tk_wup_tsk で即座に起床します
 
        // 描画バッファとディスプレイバッファのキャッシュコヒーレンシの追加保護
        SCB_CleanInvalidateDCache();

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

    tk_slp_tsk(TMO_FEVR);
    return 0;
}
