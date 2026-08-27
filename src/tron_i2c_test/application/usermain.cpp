// 修正ポイント1：C言語で書かれたOSのヘッダファイルを extern "C" で囲む
extern "C" {
#include <tk/tkernel.h>
#include <tm/tmonitor.h>
}

#include "hal_data.h"

// I2C コールバックイベント受信用変数
static volatile i2c_master_event_t i2c_event = (i2c_master_event_t)0;

// C言語のリンケージを持つコールバック関数
extern "C" void g_cam_i2c_master_user_callback(i2c_master_callback_args_t * p_args)
{
    i2c_event = p_args->event;
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
LOCAL ID tskid_1;                          // Task ID number

LOCAL T_CTSK ctsk_1 = {
    .exinf   = NULL,               // 1. 拡張情報
    .tskatr  = TA_HLNG | TA_RNG3,  // 2. タスク属性
    .task    = (FP)task_1,         // 3. タスク関数
    .itskpri = 10,                 // 4. 優先度
    .stksz   = 2048,               // 5. スタックサイズ (I2C処理や出力のため少し拡張)
    .bufptr  = NULL                // 6. スタックバッファポインタ
};

LOCAL void task_2(INT stacd, void *exinf); // task execution function
LOCAL ID tskid_2;                          // Task ID number

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

    tm_printf((UB *)"\n=== Camera I2C Connection Test Start ===\n");

    // 1. カメラのリセット処理 (CAMERA_RESET -> P709)
    tm_printf((UB *)"Resetting Camera (CAMERA_RESET -> P709)...\n");
    R_IOPORT_PinWrite(&g_ioport_ctrl, CAMERA_RESET, BSP_IO_LEVEL_LOW);
    tk_dly_tsk(100); // 100msウェイト
    R_IOPORT_PinWrite(&g_ioport_ctrl, CAMERA_RESET, BSP_IO_LEVEL_HIGH);
    tk_dly_tsk(10);  // 10msウェイト

    // 2. GPTクロックのオープンと起動 (24MHz出力をP501で生成)
    tm_printf((UB *)"Starting GPT Clock for Camera XCLK (g_cam_clk)...\n");
    fsp_err_t err = R_GPT_Open(&g_cam_clk_ctrl, &g_cam_clk_cfg);
    if (FSP_SUCCESS != err)
    {
        tm_printf((UB *)"ERROR: Failed to open GPT (g_cam_clk), error code: 0x%x\n", err);
    }
    else
    {
        R_GPT_Start(&g_cam_clk_ctrl);
    }

    // 3. I2Cマスタドライバのオープン (IICチャネル1)
    tm_printf((UB *)"Opening I2C Master (g_cam_i2c_master)...\n");
    err = R_IIC_MASTER_Open(&g_cam_i2c_master_ctrl, &g_cam_i2c_master_cfg);
    if (FSP_SUCCESS != err)
    {
        tm_printf((UB *)"ERROR: Failed to open I2C master (g_cam_i2c_master), error code: 0x%x\n", err);
    }

    // スレーブアドレスをカメラモジュール(OV5640)の 0x3C に設定
    R_IIC_MASTER_SlaveAddressSet(&g_cam_i2c_master_ctrl, 0x3C, I2C_MASTER_ADDR_MODE_7BIT);

    // 4. カメラモジュールのProduct IDレジスタの読み出しによる接続確認
    tm_printf((UB *)"Reading OV5640 Product ID registers via I2C...\n");
    uint8_t pid_h = 0;
    uint8_t pid_l = 0;
    bool read_success = true;

    if (!rdSensorReg16_8(0x300a, &pid_h))
    {
        tm_printf((UB *)"ERROR: Failed to read register 0x300A (Product ID H)\n");
        read_success = false;
    }
    
    if (!rdSensorReg16_8(0x300b, &pid_l))
    {
        tm_printf((UB *)"ERROR: Failed to read register 0x300B (Product ID L)\n");
        read_success = false;
    }

    if (read_success)
    {
        tm_printf((UB *)"Product ID Read: H = 0x%02X, L = 0x%02X\n", pid_h, pid_l);
        if (pid_h == 0x56 && (pid_l == 0x40 || pid_l == 0x41 || pid_l == 0x4C))
        {
            tm_printf((UB *)"SUCCESS: Camera connection verified! (OV5640 detected)\n");
        }
        else
        {
            tm_printf((UB *)"ERROR: Product ID mismatch (Expected 0x56 and 0x40/0x41/0x4C)\n");
        }
    }
    else
    {
        tm_printf((UB *)"ERROR: Camera communication failed.\n");
    }

    while (1) {
        tm_printf((UB *)"task 1 heart beat...\n");
        tk_dly_tsk(5000);
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
    tm_putstring((UB *)"Start User-main program (Camera Connection Test).\n");

    /* Create & Start Tasks */
    tskid_1 = tk_cre_tsk(&ctsk_1);
    tk_sta_tsk(tskid_1, 0);

    tskid_2 = tk_cre_tsk(&ctsk_2);
    tk_sta_tsk(tskid_2, 0);

    tk_slp_tsk(TMO_FEVR);
    return 0;
}
