#include "ai_inference.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "yolo_fastest_model_data.h"
#include "DetectorPostProcessing.hpp"
#include "DetectionResult.hpp"
#include "camera_utils.h"
#include "time_counter.h"
#include <vector>
#include <string.h>

extern "C" {
#ifdef _B
#undef _B
#endif
#include <tk/tkernel.h>
#include <tm/tmonitor.h>

// 液晶側から渡される最新のカメラ画像バッファ
extern void *p_camera_capture_buffer_stored;

// TFLite Micro のログコールバック登録関数
extern void RegisterDebugLogCallback(void (*cb)(const char* s));
}

// TFLite Micro からのエラー出力を μT-Kernel のシリアルコンソールに転送するコールバック
static void ai_debug_log_callback(const char* s) {
    tm_printf((UB *)"%s", (const UB *)s);
}

// テンソルアリーナを外部SDRAMに十分なサイズ (2MB) で配置
#define TENSOR_ARENA_SIZE (2048 * 1024)
uint8_t tensor_arena[TENSOR_ARENA_SIZE] BSP_PLACE_IN_SECTION(".sdram") BSP_ALIGN_VARIABLE(16);

// TFLite Micro 関連のポインタ
static const tflite::Model* g_model = nullptr;
static tflite::MicroInterpreter* g_interpreter = nullptr;
static tflite::MicroMutableOpResolver<15>* g_resolver = nullptr;

// YOLO-Fastest のアンカー定義 (ルネサスの顔検出サンプルと同一)
const float anchor1[] = {38.f, 77.f, 47.f, 97.f, 61.f, 126.f};
const float anchor2[] = {14.f, 26.f, 19.f, 37.f, 28.f, 55.f};

// 共有する検出結果の座標データ
st_ai_detection_point_t g_ai_detection[AI_MAX_DETECTION_NUM] = {0};
st_ai_processing_time_t g_ai_processing_time = {0};
volatile uint32_t g_ai_inference_time_ms = 0;
volatile bool g_ai_result_new = false;
volatile bool g_ai_task_busy = false;

// ループ脱出時に自動で busy フラグを下げるための RAII 構造体
struct AIBusyScope {
    AIBusyScope() { g_ai_task_busy = true; }
    ~AIBusyScope() { g_ai_task_busy = false; }
};

// 192x192 の入力用 int8 画像テンソルバッファ (外部SDRAM)
static int8_t model_input_buffer[AI_INPUT_IMAGE_WIDTH * AI_INPUT_IMAGE_HEIGHT * AI_INPUT_IMAGE_BYTE_PER_PIXEL] BSP_PLACE_IN_SECTION(".sdram") BSP_ALIGN_VARIABLE(8);

// C++側の後処理ライブラリから呼び出される、検出された顔座標の格納コールバック
extern "C" void update_detection_result(uint16_t index, signed short x, signed short y, signed short w, signed short h, int val_percent)
{
    if (index < AI_MAX_DETECTION_NUM)
    {
        g_ai_detection[index].m_x = x;
        g_ai_detection[index].m_y = y;
        g_ai_detection[index].m_w = w;
        g_ai_detection[index].m_h = h;
        g_ai_detection[index].m_val_percent = val_percent;
    }
}

// AIタスクのエントリー関数 (μT-Kernel 3.0)
extern "C" void ai_inference_task(int stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;

    tm_printf((UB *)"=== Starting AI Inference Task (CPU-based YOLO-Fastest) ===\n");

    // 外部SDRAMの初期化(task_1側で行われる)が確実に完了するのを待機 (2000ms)
    tk_dly_tsk(2000);

    // TFLite Micro のデバッグログ出力先をシリアルモニタへ結合する
    RegisterDebugLogCallback(ai_debug_log_callback);

    // アリーナ全体を確実にゼロクリア (未初期化のゴミデータ対策)
    memset(tensor_arena, 0, TENSOR_ARENA_SIZE);

    // 1. TFLite Micro の初期化
    g_model = tflite::GetModel(yolo_fastest_192_face_v4_tflite);
    if (g_model->version() != TFLITE_SCHEMA_VERSION)
    {
        tm_printf((UB *)"ERROR: Model schema version %d does not match runtime %d.\n",
                  (int)g_model->version(), (int)TFLITE_SCHEMA_VERSION);
        while (1) { tk_dly_tsk(1000); }
    }

    static tflite::MicroMutableOpResolver<15> resolver;
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddLeakyRelu();
    resolver.AddAdd();
    resolver.AddReshape();
    resolver.AddConcatenation();
    resolver.AddPad();
    resolver.AddLogistic(); // Sigmoid (tflite micro uses AddLogistic for sigmoid)
    resolver.AddMaxPool2D(); // Max pooling (YOLO-Fastest requires MAX_POOL_2D)
    resolver.AddResizeNearestNeighbor(); // Resize nearest neighbor (YOLO-Fastest requires RESIZE_NEAREST_NEIGHBOR)
    g_resolver = &resolver;

    static tflite::MicroInterpreter interpreter(g_model, *g_resolver, tensor_arena, TENSOR_ARENA_SIZE);
    g_interpreter = &interpreter;

    TfLiteStatus allocate_status = g_interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        tm_printf((UB *)"ERROR: AllocateTensors() failed with status %d.\n", (int)allocate_status);
        while (1) { tk_dly_tsk(1000); }
    }

    TfLiteTensor* input_tensor = g_interpreter->input(0);
    TfLiteTensor* output_tensor0 = g_interpreter->output(0);
    TfLiteTensor* output_tensor1 = g_interpreter->output(1);

    tm_printf((UB *)"SUCCESS: TensorFlow Lite Micro initialized successfully!\n");
    char init_log_buf[128];
    sprintf(init_log_buf, "Input tensor size: %d bytes, shape: %d x %d x %d\n",
            (int)input_tensor->bytes,
            (int)input_tensor->dims->data[1],
            (int)input_tensor->dims->data[2],
            (int)input_tensor->dims->data[3]);
    tm_printf((UB *)"%s", (UB *)init_log_buf);

    // DWT サイクルカウンター（時間計測）の初期化
    TimeCounter_Init();

    while (true)
    {
        // 描画タスク (task_1) からの通知が来るまでスリープ待機
        tk_slp_tsk(TMO_FEVR);

        // 起床したらビジー状態にする (関数スコープを抜ける/continue時に自動解除)
        AIBusyScope busy_scope;

        if (p_camera_capture_buffer_stored == nullptr)
        {
            continue;
        }

        // --- 前処理の実行 ---
        uint32_t start_time = TimeCounter_CurrentCountGet();

        // 最新のカメラキャプチャ画像 (320x240 RGB565) を 192x192 グレースケール (int8) に変換
        // カメラ画像をキャッシュフラッシュして、直前のDMA書き込みデータを確実に読み込めるようにする
        SCB_InvalidateDCache_by_Addr(p_camera_capture_buffer_stored, 320 * 240 * 2);

        // image_rgb565_to_int8 は camera_utils.cpp で実装されている
        vision_ai_app_err_t pre_err = image_rgb565_to_int8(p_camera_capture_buffer_stored,
                                                 model_input_buffer,
                                                 320, 240,
                                                 AI_INPUT_IMAGE_WIDTH, AI_INPUT_IMAGE_HEIGHT);
        
        if (pre_err != VISION_AI_APP_SUCCESS)
        {
            tm_printf((UB *)"ERROR: Image pre-processing failed (0x%x)\n", pre_err);
            continue;
        }

        // 前処理済みのデータをモデルの入力テンソル領域へコピー
        memcpy(input_tensor->data.int8, model_input_buffer, input_tensor->bytes);

        uint32_t middle_time = TimeCounter_CurrentCountGet();
        g_ai_processing_time.ai_inference_pre_processing_time_ms = 
            TimeCounter_CountValueConvertToMs(start_time, middle_time);

        // --- 推論の実行 (CPU上でのロード) ---
        TfLiteStatus invoke_status = g_interpreter->Invoke();

        uint32_t end_time = TimeCounter_CurrentCountGet();
        g_ai_processing_time.ai_inference_time_ms = 
            TimeCounter_CountValueConvertToMs(middle_time, end_time);
        g_ai_inference_time_ms = g_ai_processing_time.ai_inference_time_ms;

        if (invoke_status != kTfLiteOk)
        {
            tm_printf((UB *)"ERROR: Inference Invoke failed.\n");
            continue;
        }

        // --- 後処理の実行 (YOLO-Fastest デコード & NMS) ---
        uint32_t post_start_time = TimeCounter_CurrentCountGet();

        // 前回の検出結果座標配列をクリア
        for (int i = 0; i < AI_MAX_DETECTION_NUM; i++)
        {
            g_ai_detection[i].m_x = 0;
            g_ai_detection[i].m_y = 0;
            g_ai_detection[i].m_w = 0;
            g_ai_detection[i].m_h = 0;
            g_ai_detection[i].m_val_percent = 0;
        }

        std::vector<arm::app::object_detection::DetectionResult> results;
        arm::app::object_detection::PostProcessParams postProcessParams {
            AI_INPUT_IMAGE_HEIGHT, AI_INPUT_IMAGE_WIDTH, AI_INPUT_IMAGE_WIDTH, anchor1, anchor2
        };

        // DetectorPostProcess クラス（C++）の実行
        arm::app::DetectorPostProcess postProcess(output_tensor0, output_tensor1, results, postProcessParams);
        if (!postProcess.DoPostProcess())
        {
            tm_printf((UB *)"ERROR: YOLO-Fastest post-processing failed.\n");
            continue;
        }

        // 検出されたオブジェクト結果を配列へ反映
        for (uint16_t i = 0; i < results.size() && i < AI_MAX_DETECTION_NUM; ++i)
        {
            update_detection_result(i, 
                                    (signed short)results[i].m_x0, 
                                    (signed short)results[i].m_y0, 
                                    (signed short)results[i].m_w, 
                                    (signed short)results[i].m_h,
                                    (int)(results[i].m_normalisedVal * 100));
        }

        uint32_t total_end_time = TimeCounter_CurrentCountGet();
        uint32_t total_time_ms = TimeCounter_CountValueConvertToMs(start_time, total_end_time);
        uint32_t post_time_ms = TimeCounter_CountValueConvertToMs(post_start_time, total_end_time);

        // トータル処理時間をグローバル変数に反映 (描画タスクで参照するため)
        g_ai_inference_time_ms = total_time_ms;

        // シリアルログに詳細データを出力 (引数制限を避けるため一度 sprintf で整形する)
        char log_buf[128];
        sprintf(log_buf, "AI Inference: Found %d face(s) in %d ms (Pre: %d ms, Inv: %d ms, Post: %d ms)\n",
                (int)results.size(), (int)total_time_ms,
                (int)g_ai_processing_time.ai_inference_pre_processing_time_ms,
                (int)g_ai_processing_time.ai_inference_time_ms,
                (int)post_time_ms);
        tm_printf((UB *)"%s", (UB *)log_buf);
        
        for (uint16_t i = 0; i < results.size() && i < AI_MAX_DETECTION_NUM; ++i)
        {
            char detail_buf[128];
            sprintf(detail_buf, "  Face %d: Box[%d, %d, %d, %d], Conf: %d%%\n",
                    (int)i,
                    (int)results[i].m_x0, (int)results[i].m_y0,
                    (int)results[i].m_w, (int)results[i].m_h,
                    (int)(results[i].m_normalisedVal * 100));
            tm_printf((UB *)"%s", (UB *)detail_buf);
        }

        // 描画タスクへ最新の検出座標が更新されたことをフラグで通知
        g_ai_result_new = true;
    }
}
