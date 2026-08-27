// 修正ポイント1：C言語で書かれたOSのヘッダファイルを extern "C" で囲む（これが undefined reference エラーの直接の原因です）
extern "C" {
#include <tk/tkernel.h>
#include <tm/tmonitor.h>
}

LOCAL void task_1(INT stacd, void *exinf); // task execution function
LOCAL ID tskid_1;                          // Task ID number

// 修正ポイント2：警告に出ていた bufptr も明示的に初期化する
LOCAL T_CTSK ctsk_1 = {
    .exinf   = NULL,               // 1. 拡張情報
    .tskatr  = TA_HLNG | TA_RNG3,  // 2. タスク属性
    .task    = (FP)task_1,         // 3. タスク関数
    .itskpri = 10,                 // 4. 優先度
    .stksz   = 1024,               // 5. スタックサイズ
    .bufptr  = NULL                // 6. スタックバッファポインタ（警告対策）
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
    // 修正ポイント3：未使用引数の警告対策（引数を void キャストして明示的に無視する）
    (void)stacd;
    (void)exinf;

    while (1) {
        tm_printf((UB *)"task 1\n");
        tk_dly_tsk(500);
    }
}

LOCAL void task_2(INT stacd, void *exinf)
{
    // 修正ポイント3：未使用引数の警告対策
    (void)stacd;
    (void)exinf;

    while (1) {
        tm_printf((UB *)"task 2\n");
        tk_dly_tsk(700);
    }
}

// OSからC言語の関数として正しく呼ばれるよう extern "C" を付与
extern "C" EXPORT INT usermain(void)
{
    tm_putstring((UB *)"Start User-main program.\n");

    /* Create & Start Tasks */
    tskid_1 = tk_cre_tsk(&ctsk_1);
    tk_sta_tsk(tskid_1, 0);

    tskid_2 = tk_cre_tsk(&ctsk_2);
    tk_sta_tsk(tskid_2, 0);

    tk_slp_tsk(TMO_FEVR);
    return 0;
}
