#include "hal_data.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

extern "C" {
#ifdef _B
#undef _B
#endif
#include <tk/tkernel.h>
#include <tm/tmonitor.h>
void rm_ethosu_isr(void);
}

#include "ai_inference.h"
#include "camera_layer.h"
#include "camera_utils.h"
#include "time_counter.h"

// FSPコード生成をスキップして手動で定義するNPU構造体
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
    .irq             = ((IRQn_Type) 20), // ベクタ20を使用
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

// 共有する検出結果の座標データ
st_ai_detection_point_t g_ai_detection[AI_MAX_DETECTION_NUM] = {0};
st_ai_processing_time_t g_ai_processing_time = {0};
volatile uint32_t g_ai_inference_time_ms = 0;
volatile bool g_ai_result_new = false;
volatile bool g_ai_task_busy = false;

// 描画ループ脱出時に自動で busy フラグを下げるための RAII 構造体
struct AIBusyScope {
    AIBusyScope() { g_ai_task_busy = true; }
    ~AIBusyScope() { g_ai_task_busy = false; }
};

// NPU入力用の 192x192 グレースケール テンソルバッファ (外部SDRAMに配置)
// ラッパーである MainLoop_obj.cc 側から extern 参照されるバッファ名 "model_buffer_int8" に合わせます。
int8_t model_buffer_int8[AI_INPUT_IMAGE_WIDTH * AI_INPUT_IMAGE_HEIGHT * AI_INPUT_IMAGE_BYTE_PER_PIXEL] BSP_ALIGN_VARIABLE(8) BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
uint32_t model_buffer_int8_size = sizeof(model_buffer_int8);

// 処理時間計測構造体の定義実体 (ラッパー MainLoop_obj.cc 側で参照されるため定義)
typedef struct st_processing_time_info_t
{
    uint32_t camera_image_capture_time_ms;
    uint32_t camera_post_processing_time_ms;
    uint32_t lcd_display_update_refresh_ms;
    uint32_t ai_inference_pre_processing_time_ms;
    uint32_t ai_inference_time_ms;
} processinf_time_info_t;

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

extern "C" {
// 液晶側から渡される最新のカメラ画像バッファ
extern void *p_camera_capture_buffer_stored;
// ラッパー側のAI推論関数
vision_ai_app_err_t face_detection(void);
// TFLiteのログコールバック登録関数 (CPUモデルでは使用しない)
// extern void RegisterDebugLogCallback(void (*cb)(const char* s));
}

// C++側の後処理ライブラリから呼び出される、検出された顔座標の格納コールバック
extern "C" void update_detection_result(uint16_t index, signed short x, signed short y, signed short w, signed short h, int val_percent, int class_idx)
{
    if (index < AI_MAX_DETECTION_NUM)
    {
        if (w > 0 && h > 0)
        {
            tm_printf((UB *)"[update_detection_result] idx=%d, x=%d, y=%d, w=%d, h=%d, prob=%d%%, class=%d\n", index, x, y, w, h, val_percent, class_idx);
        }
        g_ai_detection[index].m_x = x;
        g_ai_detection[index].m_y = y;
        g_ai_detection[index].m_w = w;
        g_ai_detection[index].m_h = h;
        g_ai_detection[index].m_val_percent = val_percent;
        g_ai_detection[index].m_class = class_idx;
    }
}

// AIタスクのエントリー関数 (μT-Kernel 3.0)
extern "C" void ai_inference_task(int stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;

    tm_printf((UB *)"=== Starting AI Inference Task (CPU-based FOMO) ===\n");

    // 外部SDRAMの初期化が完了するのを待機 (2000ms)
    tk_dly_tsk(2000);

    // DWT サイクルカウンター（時間計測）の初期化
    TimeCounter_Init();

    while (true)
    {
        // 描画タスク (task_1) からの通知が来るまでスリープ待機
        tk_slp_tsk(TMO_FEVR);

        // 起床したらビジー状態にする
        AIBusyScope busy_scope;

        if (p_camera_capture_buffer_stored == nullptr)
        {
            continue;
        }

        // --- 前処理の実行 ---
        // 最新のカメラキャプチャ画像 (320x240 RGB565) を 192x192 グレースケール (int8) に変換
        SCB_InvalidateDCache_by_Addr(p_camera_capture_buffer_stored, 320 * 240 * 2);

        vision_ai_app_err_t pre_err = image_rgb565_to_int8(p_camera_capture_buffer_stored,
                                                 model_buffer_int8,
                                                 320, 240,
                                                 AI_INPUT_IMAGE_WIDTH, AI_INPUT_IMAGE_HEIGHT);
        
        if (pre_err != VISION_AI_APP_SUCCESS)
        {
            tm_printf((UB *)"ERROR: Image pre-processing failed (0x%x)\n", pre_err);
            continue;
        }

        // Diagnostics for camera buffer
        uint16_t *cam_pixels = (uint16_t *)p_camera_capture_buffer_stored;
        uint32_t cam_pixels_count = 320 * 240;
        uint32_t cam_sum = 0;
        uint16_t cam_min = 0xFFFF;
        uint16_t cam_max = 0x0000;
        for (uint32_t i = 0; i < cam_pixels_count; ++i) {
            uint16_t val = cam_pixels[i];
            cam_sum += val;
            if (val < cam_min) cam_min = val;
            if (val > cam_max) cam_max = val;
        }
        uint32_t cam_avg = cam_sum / cam_pixels_count;
        
        // Diagnostics for NPU input buffer
        int32_t npu_sum = 0;
        int8_t npu_min = 127;
        int8_t npu_max = -128;
        for (uint32_t i = 0; i < model_buffer_int8_size; ++i) {
            int8_t val = model_buffer_int8[i];
            npu_sum += val;
            if (val < npu_min) npu_min = val;
            if (val > npu_max) npu_max = val;
        }
        int32_t npu_avg = npu_sum / (int32_t)model_buffer_int8_size;

        char diag_buf[160];
        sprintf(diag_buf, "[Diag] Cam buffer: min=0x%04X, max=0x%04X, avg=0x%04X | NPU Input: min=%d, max=%d, avg=%d\n",
                (unsigned int)cam_min, (unsigned int)cam_max, (unsigned int)cam_avg, (int)npu_min, (int)npu_max, (int)npu_avg);
        tm_printf((UB *)diag_buf);

        // --- NPU推論の実行 (ラッパー経由) ---
        vision_ai_app_err_t status = face_detection();

        if (VISION_AI_APP_SUCCESS == status)
        {
            // 推論処理時間の取得 (MainLoop_obj.cc 側で直接 g_ai_inference_time_ms に書き込まれるため、ここでの 0 による上書きを回避)
            // g_ai_inference_time_ms = application_processing_time.ai_inference_time_ms;
            g_ai_result_new = true;

            // シリアルログに推論詳細データを出力 (引数制限回避のため sprintf 経由)
            char log_buf[128];
            sprintf(log_buf, "AI Inference (NPU): IC detection complete in %lu ms\n", g_ai_inference_time_ms);
            tm_printf((UB *)"%s", (UB *)log_buf);
        }
        else
        {
            char err_buf[64];
            sprintf(err_buf, "ERROR: NPU Inference execution failed (status: %d).\n", (int)status);
            tm_printf((UB *)"%s", (UB *)err_buf);
        }
    }
}

// GCC/Newlib-nano の printf() 出力を T-Monitor コンソールにリダイレクトするシステムコール
extern "C" int _write(int file, char *ptr, int len)
{
    (void)file;
    for (int i = 0; i < len; i++) {
        tm_putchar(ptr[i]);
    }
    return len;
}
