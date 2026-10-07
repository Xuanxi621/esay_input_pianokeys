#include <stdio.h>
#include <string.h>

#include "board_keys.h"
#include "board_power.h"
#include "piano_synth.h"
#include "star_song.h"
#include "led_piano.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "piano_main";

/*
 * 游戏核心状态：开机直接进入下落式钢琴小游戏
 */
static piano_play_mode_t s_mode = PIANO_MODE_GUIDE;
static uint8_t s_game_step = 0;
static uint32_t s_game_score = 0;
static uint16_t s_game_combo = 0;
static uint16_t s_max_combo = 0;

/* 长按延音判定状态 */
static bool s_holding_target = false;
static int64_t s_target_hold_start_us = 0;
static int64_t s_key_down_time[8] = {0};

static const char *get_mode_name(piano_play_mode_t mode)
{
    switch (mode) {
    case PIANO_MODE_GUIDE: return "沉浸式小星星钢琴小游戏 (Piano Game)";
    case PIANO_MODE_FREE:  return "自由演奏模式 (Free Play)";
    default: return "未知模式";
    }
}

static void send_game_step_event(void)
{
    const song_note_t *target = &g_star_song[s_game_step];
    uint8_t target_key = song_note_to_key(target->note_idx);

    led_piano_show_guide(target->note_idx);

    /* 输出机器可读的 JSON 事件，供网页端无缝驱动下落轨道与按键提示 */
    printf("{\"t\":\"game_step\",\"step\":%d,\"target_key\":%d,\"note\":%d,\"note_str\":\"%s\",\"lyric\":\"%s\",\"dur\":%d,\"total\":%d,\"score\":%lu,\"combo\":%u}\n",
           s_game_step, target_key, target->note_idx, target->note_str, target->lyric, target->duration_ms,
           STAR_SONG_NOTE_COUNT, (unsigned long)s_game_score, s_game_combo);

    /* 终端可读日志 */
    printf("🎯 [音符 %02d/%d] 请按下 【%s】键 (%s)%s · 歌词: \"%s\"\n",
           s_game_step + 1, STAR_SONG_NOTE_COUNT,
           song_key_name(target_key), target->note_str,
           (target->duration_ms >= 800) ? " [⏳长按延音]" : "",
           target->lyric);
    fflush(stdout);
}

static void handle_step_advance(void)
{
    s_game_step++;
    if (s_game_step >= STAR_SONG_NOTE_COUNT) {
        const char *rank = (s_max_combo == STAR_SONG_NOTE_COUNT) ? "SSS (神级演奏)" :
                           (s_max_combo >= 30) ? "SS (大师演奏)" : "S (优秀演奏)";

        printf("\n🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉\n");
        printf("   🏆 恭喜！《小星星》整首曲目演奏通关！🏆\n");
        printf("   • 最终得分: %lu\n", (unsigned long)s_game_score);
        printf("   • 最高连击: %u / %d\n", s_max_combo, STAR_SONG_NOTE_COUNT);
        printf("   • 演奏评级: %s\n", rank);
        printf("🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉\n\n");

        printf("{\"t\":\"game_victory\",\"score\":%lu,\"max_combo\":%u,\"total\":%d,\"rank\":\"%s\"}\n",
               (unsigned long)s_game_score, s_max_combo, STAR_SONG_NOTE_COUNT, rank);

        led_piano_show_victory();

        s_game_step = 0;
        s_game_score = 0;
        s_game_combo = 0;
        s_max_combo = 0;
    }
    send_game_step_event();
}

static void print_banner(void)
{
    printf("\n");
    printf("====================================================================\n");
    printf("   ★ EasyInput 沉浸式钢琴小游戏 - 《小星星》演奏 (EasyInput Piano) ★\n");
    printf("====================================================================\n");
    printf("  [8键物理排布与音符映射]\n");
    printf("    上排: [S1: 5(Sol)]  [S2: 6(La)]  [S3: 7(Si)]  [S4: i(高音Do)]\n");
    printf("    下排: [S5: 1(Do)]   [S6: 2(Re)]  [S7: 3(Mi)]  [S8: 4(Fa)]\n");
    printf("--------------------------------------------------------------------\n");
    printf("  [玩法说明]:\n");
    printf("    • 支持短音单击与长音按住时间判定，感受真实钢琴击弦与长条延音！\n");
    printf("    • 网页前端提供下落式瀑布流轨道与拟真象牙钢琴琴键！\n");
    printf("    • 连击累加 COMBO 与得分，一曲弹完结算演奏评级！\n");
    printf("    • 短按旋钮可在【小游戏跟弹】与【自由琴键】之间切换；旋转调节音量。\n");
    printf("    • 长按旋钮(>0.7s)可一键彻底开启/关闭板载 D1~D5 柔和指示灯。\n");
    printf("====================================================================\n\n");
}

static void switch_mode(piano_play_mode_t next_mode)
{
    s_mode = next_mode;
    s_holding_target = false;
    piano_synth_all_notes_off();
    printf("\n>>> 切换至模式: 【%s】 <<<\n", get_mode_name(s_mode));

    if (s_mode == PIANO_MODE_GUIDE) {
        s_game_step = 0;
        s_game_score = 0;
        s_game_combo = 0;
        s_max_combo = 0;
        printf("{\"t\":\"game_start\",\"total\":%d}\n", STAR_SONG_NOTE_COUNT);
        send_game_step_event();
    } else {
        printf("{\"t\":\"mode_change\",\"mode\":\"free\"}\n");
        printf("【自由演奏】8个琴键随意弹奏，支持多键齐按和弦！\n");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Booting EasyInput Piano Game...");

    /* 1. 安全供电时序：GPIO8 延迟上电保护功放与 RGB 灯 */
    ESP_ERROR_CHECK(board_power_init());
    ESP_ERROR_CHECK(board_power_enable_peripherals(true));

    /* 2. 初始化按键、LED、高保真谐波钢琴合成器 */
    ESP_ERROR_CHECK(board_keys_init());
    ESP_ERROR_CHECK(led_piano_init());
    ESP_ERROR_CHECK(piano_synth_init());

    /* 默认音量调到 100% 满幅大声输出 */
    piano_synth_set_volume(100);

    print_banner();

    /* 开机直接进入小游戏模式，静待玩家落键 */
    switch_mode(PIANO_MODE_GUIDE);

    board_input_snapshot_t in;
    while (true) {
        const int64_t now_us = esp_timer_get_time();

        esp_err_t err = board_keys_poll(&in);
        if (err == ESP_OK) {
            /* 1. 旋钮调节音量 */
            if (in.enc_delta != 0) {
                int new_vol = (int)piano_synth_get_volume() + (in.enc_delta * 5);
                if (new_vol < 0) new_vol = 0;
                if (new_vol > 100) new_vol = 100;
                piano_synth_set_volume((uint8_t)new_vol);
                led_piano_show_volume((uint8_t)new_vol);
                printf("{\"t\":\"volume\",\"val\":%d}\n", new_vol);
                printf("[音量调节] 当前音量: %d%%\n", new_vol);
            }

            /* 2. 编码器按键：短按切换模式，长按(>0.7秒)彻底开/关 D1~D5 灯 */
            static int64_t s_enc_down_us = 0;
            static bool s_enc_long_triggered = false;

            if (in.enc_just_pressed) {
                s_enc_down_us = now_us;
                s_enc_long_triggered = false;
            }
            if (in.enc_press && !s_enc_long_triggered && s_enc_down_us > 0) {
                if (now_us - s_enc_down_us > 700000) {
                    s_enc_long_triggered = true;
                    led_piano_toggle_enable();
                    printf("{\"t\":\"led_toggle\",\"enabled\":%d}\n", led_piano_is_enabled());
                    printf("💡 [灯光控制] D1-D5 灯已%s！\n", led_piano_is_enabled() ? "开启 (柔和8%护眼)" : "彻底关闭 (熄灭)");
                }
            }
            if (!in.enc_press && s_enc_down_us > 0) {
                if (!s_enc_long_triggered) {
                    /* 短按：切换模式 */
                    piano_play_mode_t next = (s_mode == PIANO_MODE_GUIDE) ? PIANO_MODE_FREE : PIANO_MODE_GUIDE;
                    switch_mode(next);
                }
                s_enc_down_us = 0;
            }

            /* 3. 处理 8 个琴键输入 */
            if (s_mode == PIANO_MODE_GUIDE) {
                const song_note_t *target = &g_star_song[s_game_step];
                uint8_t target_note = target->note_idx;
                uint8_t target_key = song_note_to_key(target_note);
                bool is_hold_note = (target->duration_ms >= 800);

                /* 检查长按延音的实时保持状态 */
                if (s_holding_target) {
                    int held_ms = (int)((now_us - s_target_hold_start_us) / 1000);
                    if (held_ms >= target->duration_ms) {
                        /* ★ 完美长按按满！ */
                        s_holding_target = false;
                        s_game_combo++;
                        if (s_game_combo > s_max_combo) s_max_combo = s_game_combo;
                        uint32_t gain = 200 + (s_game_combo * 20);
                        s_game_score += gain;

                        printf("{\"t\":\"game_hit\",\"result\":\"perfect_hold\",\"key\":%d,\"note\":%d,\"held_ms\":%d,\"combo\":%u,\"score\":%lu,\"step\":%d}\n",
                               target_key, target_note, held_ms, s_game_combo, (unsigned long)s_game_score, s_game_step);
                        printf("  🌟 [PERFECT HOLD! 长按满分] %s 延音完成！(+%lu分) -> 总分: %lu\n",
                               song_key_name(target_key), (unsigned long)gain, (unsigned long)s_game_score);

                        handle_step_advance();
                    }
                }

                for (int i = 0; i < 8; ++i) {
                    uint8_t note = song_key_to_note((uint8_t)i);

                    /* 按下琴键（Note-On） */
                    if (in.pressed[i]) {
                        s_key_down_time[i] = now_us;
                        piano_synth_note_on(note, 127);
                        led_piano_on_note_hit(note);

                        printf("{\"t\":\"key_down\",\"key\":%d,\"note\":%d}\n", i, note);

                        if (i == target_key) {
                            if (is_hold_note) {
                                /* 长按音符：启动按住时间跟踪 */
                                s_holding_target = true;
                                s_target_hold_start_us = now_us;
                                printf("{\"t\":\"hold_start\",\"key\":%d,\"target_ms\":%d}\n", target_key, target->duration_ms);
                                printf("  ⏳ [长按延音] 请按住 【%s】 键保持 %d ms...\n", song_key_name(target_key), target->duration_ms);
                            } else {
                                /* 短音符：立即判定 PERFECT */
                                s_game_combo++;
                                if (s_game_combo > s_max_combo) s_max_combo = s_game_combo;
                                uint32_t gain = 100 + (s_game_combo * 15);
                                s_game_score += gain;

                                printf("{\"t\":\"game_hit\",\"result\":\"perfect\",\"key\":%d,\"note\":%d,\"lyric\":\"%s\",\"combo\":%u,\"score\":%lu,\"step\":%d}\n",
                                       i, note, target->lyric, s_game_combo, (unsigned long)s_game_score, s_game_step);
                                printf("  ✨ [PERFECT! 连击 x%u] 弹响 %s (%s) · \"%s\" (+%lu分) -> 总分: %lu\n",
                                       s_game_combo, song_key_name(i), target->note_str, target->lyric,
                                       (unsigned long)gain, (unsigned long)s_game_score);

                                handle_step_advance();
                            }
                        } else {
                            /* ✘ 按错琴键 */
                            s_game_combo = 0;
                            s_holding_target = false;
                            led_piano_on_miss();

                            printf("{\"t\":\"game_miss\",\"key\":%d,\"target_key\":%d,\"step\":%d}\n",
                                   i, target_key, s_game_step);
                            printf("  ✘ [按键偏差] 按下了 %s (音符 %s)，目标是 【%s】 (\"%s\")，请看提示灯再试一次！\n",
                                   song_key_name(i), g_piano_note_names[note],
                                   song_key_name(target_key), target->lyric);
                        }
                    }

                    /* 松开琴键（Note-Off） */
                    if (in.released[i]) {
                        int hold_ms = (int)((now_us - s_key_down_time[i]) / 1000);
                        piano_synth_note_off(note);

                        printf("{\"t\":\"key_up\",\"key\":%d,\"note\":%d,\"dur\":%d}\n", i, note, hold_ms);
                        printf("{\"t\":\"note_off\",\"key\":%d,\"note\":%d}\n", i, note);

                        /* 如果在长按目标时中途松开 */
                        if (i == target_key && s_holding_target) {
                            s_holding_target = false;
                            if (hold_ms >= target->duration_ms * 7 / 10) {
                                /* 大致按够时长（>=70%）给予良性判定 */
                                s_game_combo++;
                                if (s_game_combo > s_max_combo) s_max_combo = s_game_combo;
                                uint32_t gain = 140 + (s_game_combo * 15);
                                s_game_score += gain;

                                printf("{\"t\":\"game_hit\",\"result\":\"good_hold\",\"key\":%d,\"held_ms\":%d,\"combo\":%u,\"score\":%lu,\"step\":%d}\n",
                                       target_key, hold_ms, s_game_combo, (unsigned long)s_game_score, s_game_step);
                                printf("  👍 [GOOD HOLD!] 长按 %d ms (目标 %d ms) (+%lu分)\n",
                                       hold_ms, target->duration_ms, (unsigned long)gain);

                                handle_step_advance();
                            } else {
                                /* 提前松开，未按满时长 */
                                s_game_combo = 0;
                                printf("{\"t\":\"hold_early\",\"key\":%d,\"held_ms\":%d,\"target_ms\":%d}\n",
                                       target_key, hold_ms, target->duration_ms);
                                printf("  ⚠️ [提前松开] 仅按住 %d ms (需 %d ms)，长条中断！请重新按住！\n",
                                       hold_ms, target->duration_ms);
                            }
                        }
                    }
                }
            } else if (s_mode == PIANO_MODE_FREE) {
                /* --- 自由弹奏模式 --- */
                for (int i = 0; i < 8; ++i) {
                    uint8_t note = song_key_to_note((uint8_t)i);
                    if (in.pressed[i]) {
                        s_key_down_time[i] = now_us;
                        piano_synth_note_on(note, 127);
                        led_piano_on_note_hit(note);
                        printf("{\"t\":\"key_down\",\"key\":%d,\"note\":%d}\n", i, note);
                        printf("{\"t\":\"free_hit\",\"key\":%d,\"note\":%d}\n", i, note);
                        printf("[自由演奏] 按键 %s -> 音符 %s\n",
                               song_key_name(i), g_piano_note_names[note]);
                    }
                    if (in.released[i]) {
                        int hold_ms = (int)((now_us - s_key_down_time[i]) / 1000);
                        piano_synth_note_off(note);
                        printf("{\"t\":\"key_up\",\"key\":%d,\"note\":%d,\"dur\":%d}\n", i, note, hold_ms);
                        printf("{\"t\":\"note_off\",\"key\":%d,\"note\":%d}\n", i, note);
                    }
                }
            }

            /* 立即刷新串口缓冲区，确保按键事件零延迟送达前端网页 */
            fflush(stdout);
        }

        /* 4. 更新 LED 灯效与按键提示呼吸 */
        led_piano_update(now_us, s_mode);

        vTaskDelay(pdMS_TO_TICKS(15));
    }
}
