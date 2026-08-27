#ifndef AI_INFERENCE_H_
#define AI_INFERENCE_H_

#include "hal_data.h"

#ifdef __cplusplus
extern "C" {
#endif

// AI入力画像設定 (YOLO-Fastest は 192x192 1ch グレースケール)
#define AI_INPUT_IMAGE_WIDTH          (192)
#define AI_INPUT_IMAGE_HEIGHT         (192)
#define AI_INPUT_IMAGE_BYTE_PER_PIXEL (3)

// 同時に描画・検知する顔の最大数
#define AI_MAX_DETECTION_NUM          (20)

// 検出矩形の情報構造体
typedef struct ai_detection_point_t {
    signed short      m_x;
    signed short      m_y;
    signed short      m_w;
    signed short      m_h;
    int               m_val_percent; // 確信度パーセンテージ (0 ~ 100)
} st_ai_detection_point_t;

// 推論処理時間の計測構造体
typedef struct st_ai_processing_time_t {
    uint32_t ai_inference_pre_processing_time_ms;
    uint32_t ai_inference_time_ms;
} st_ai_processing_time_t;

// グローバル変数
extern st_ai_detection_point_t g_ai_detection[AI_MAX_DETECTION_NUM];
extern st_ai_processing_time_t g_ai_processing_time;
extern volatile uint32_t g_ai_inference_time_ms;
extern volatile bool g_ai_result_new;

// μT-Kernel 3.0 のタスクエントリー関数
void ai_inference_task(int stacd, void *exinf);

#ifdef __cplusplus
}
#endif

#endif /* AI_INFERENCE_H_ */
