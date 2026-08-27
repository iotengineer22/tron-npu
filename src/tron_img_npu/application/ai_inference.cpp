#include "hal_data.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

extern "C" {
#include <tk/tkernel.h>
#include <tm/tmonitor.h>
void rm_ethosu_isr(void);
}

#include "camera_layer.h"
#include "camera_utils.h"
#include "time_counter.h"
#include "common_util.h"
#include "ai_application/image_classification/wrapper.h"
#include "ai_application/image_classification/Labels.h"

// FSPのコード生成スキップ対策：NPUドライバ・インスタンス構成構造体の手動定義
#include "ethosu_driver.h"
#include "rm_ethosu.h"
#include "rm_ethosu_api.h"

extern "C" {
struct ethosu_driver g_ethosu0;

rm_ethosu_extended_cfg_t g_rm_ethosu0_ext_cfg = {
    .p_dev = &g_ethosu0,
};

rm_ethosu_instance_ctrl_t g_rm_ethosu0_ctrl = {
    .p_ext_cfg = &g_rm_ethosu0_ext_cfg,
};

const rm_ethosu_cfg_t g_rm_ethosu0_cfg = {
    .p_callback      = NULL,
    .p_context       = NULL,
    .ipl             = (12),
#if defined(VECTOR_NUMBER_NPU_IRQ)
    .irq             = VECTOR_NUMBER_NPU_IRQ,
#else
    .irq             = ((IRQn_Type) 20), // 手動インジェクションしたベクタ番号20を使用
#endif
    .secure_enable   = 1,
    .privilege_enable = 1,
};

const rm_ethosu_instance_t g_rm_ethosu0 = {
    .p_ctrl = &g_rm_ethosu0_ctrl,
    .p_cfg = &g_rm_ethosu0_cfg,
    .p_api = &g_rm_ethosu_on_npu,
};
}

// グローバル推論結果および状態フラグ
volatile bool g_ai_result_new = false;
volatile uint32_t g_ai_inference_time_ms = 0;

// 推論結果の格納バッファ (Top-5)
st_ai_classification_point_t g_ai_classification[AI_MAX_DETECTION_NUM] = {};

// 推論入力用の 224x224 RGB888 テンソルバッファ (外部SDRAMに配置)
int8_t model_buffer_int8[AI_INPUT_IMAGE_WIDTH * AI_INPUT_IMAGE_HEIGHT * AI_INPUT_IMAGE_BYTE_PER_PIXEL] BSP_ALIGN_VARIABLE(8) BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
uint32_t model_buffer_int8_size = sizeof(model_buffer_int8);

// 処理時間計測用構造体の定義 (common_util.h で宣言されているものの定義実体)
processinf_time_info_t application_processing_time;

// NPUのログマクロ出力を T-Monitor コンソールにリダイレクト
extern "C" int e_printf(const char *format, ...)
{
    char buf[160];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    tm_putstring((UB *)buf);
    return 0;
}

// サンプルプロジェクトのコールバック関数 (結果をTop-5配列へ流し込む)
extern "C" void update_classification_result(unsigned index, unsigned short category, float probability)
{
    if (index < AI_MAX_DETECTION_NUM)
    {
        g_ai_classification[index].prob = probability;
        g_ai_classification[index].category = category;
    }
    if (index == (AI_MAX_DETECTION_NUM - 1)) {
        float sum = 1e-6f;
        for (uint32_t i = 0; i < AI_MAX_DETECTION_NUM; i++) {
            sum += g_ai_classification[i].prob;
        }
        for (uint32_t i = 0; i < AI_MAX_DETECTION_NUM; i++) {
            g_ai_classification[i].prob /= sum;
        }
    }
}

// 画像分類推論実行のプロトタイプ
extern "C" {
vision_ai_app_err_t image_classification(void);
}

// NPU推論タスクのエントリー関数
extern "C" void task_3(INT stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;

    // カメラ・LCDの安定化および初期化完了を少し待つ
    tk_dly_tsk(1500);

    tm_printf((UB *)"=== Starting Arm Ethos-U55 NPU Task (task 3)... ===\n");

    // --- Dynamic Interrupt Relocation for NPU (Vector 20) ---
    // Copy vector table from Flash to RAM to make it writable
    #define RAM_VECTOR_TABLE_SIZE (16 + 32)
    __attribute__((aligned(256))) static fsp_vector_t g_ram_vector_table[RAM_VECTOR_TABLE_SIZE];
    
    uint32_t current_vtor = SCB->VTOR;
    // Copy the original vector table (Cortex-M exceptions 16 + peripheral interrupts 13 = 29 entries)
    memcpy(g_ram_vector_table, (void*)current_vtor, 29 * sizeof(fsp_vector_t));
    
    // Register NPU ISR at Vector 20 (NVIC interrupt index 20, which is vector table index 16 + 20 = 36)
    g_ram_vector_table[16 + 20] = (fsp_vector_t)rm_ethosu_isr;
    
    // Set VTOR to point to our RAM vector table
    __disable_irq();
    SCB->VTOR = (uint32_t)g_ram_vector_table;
    __DSB();
    __ISB();
    __enable_irq();
    
    // Bind Vector 20 to ELC_EVENT_NPU_IRQ in ICU
    R_ICU->IELSR[20] = (uint32_t) ELC_EVENT_NPU_IRQ;

    // 1. Arm Ethos-U55 NPU ドライバを起動
    fsp_err_t err = RM_ETHOSU_Open(&g_rm_ethosu0_ctrl, &g_rm_ethosu0_cfg);
    if (FSP_SUCCESS != err)
    {
        tm_printf((UB *)"ERROR: Failed to open Arm Ethos-U55 NPU (0x%x)\n", err);
        while(1) { tk_dly_tsk(1000); }
    }
    tm_printf((UB *)"SUCCESS: Arm Ethos-U55 NPU driver initialized successfully!\n");

    // 2. DWT サイクルカウンタータイマーの初期化
    TimeCounter_Init();

    while (true)
    {
        // 描画タスク (task_1) からの推論リクエストがあるまでスリープ待機
        tk_slp_tsk(TMO_FEVR);
        tm_printf((UB *)"=== NPU Task: Woken up, starting inference... ===\n");

        // 推論の実行
        vision_ai_app_err_t status = image_classification();

        if (VISION_AI_APP_SUCCESS == status)
        {
            // 推論処理時間の取得
            g_ai_inference_time_ms = application_processing_time.ai_inference_time_ms;
            g_ai_result_new = true;
            tm_printf((UB *)"=== NPU Task: Inference Success! Time: %u ms ===\n", (unsigned int)g_ai_inference_time_ms);
        }
        else
        {
            tm_printf((UB *)"ERROR: NPU Inference execution failed (status: %d).\n", status);
        }
    }
}
