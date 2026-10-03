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

static piano_play_mode_t s_mode = PIANO_MODE_MAGIC; /* 默认启动一键魔法自动演奏 */
static uint8_t s_guide_step = 0;
static uint8_t s_magic_step = 0;
static int64_t s_next_auto_note_us = 0;

static const char *get_mode_name(piano_play_mode_t mode)
{
    switch (mode) {
    case PIANO_MODE_FREE:  return "自由演奏模式 (Free Play)";
    case PIANO_MODE_GUIDE: return "小星星跟弹教学模式 (Guide Practice)";
    case PIANO_MODE_MAGIC: return "一键魔法自动演奏模式 (Magic Auto Play)";
    case PIANO_MODE_AUTO:  return "小星星自动演示模式 (Auto Demo)";
    default: return "未知模式";
    }
}

static void print_banner(void)
{
    printf("\n");
    printf("====================================================================\n");
    printf("   ★ EasyInput 8键电子琴 - 《小星星》演奏项目 (EasyInput Piano) ★\n");
    printf("====================================================================\n");
    printf("  [按键物理排布与音符映射]\n");
    printf("    上排: [S1: 5(Sol)]  [S2: 6(La)]  [S3: 7(Si)]  [S4: i(高音Do)]\n");
    printf("    下排: [S5: 1(Do)]   [S6: 2(Re)]  [S7: 3(Mi)]  [S8: 4(Fa)]\n");
    printf("--------------------------------------------------------------------\n");
    printf("  [当前音量]: 100%% (大声满幅输出)\n");
    printf("  [模式指南]:\n");
    printf("    - 短按编码器: 循环切换模式\n");
    printf("        1. 魔法自动演奏模式 (板子自己自动弹奏小星星，随心互动)\n");
    printf("        2. 小星星跟弹教学模式 (跟随提示灯与网页指引练习)\n");
    printf("        3. 自由琴键演奏模式 (8个键自由弹奏与和弦)\n");
    printf("    - 旋转编码器: 调节主音量 (0%% ~ 100%%)\n");
    printf("====================================================================\n\n");
}

static void switch_mode(piano_play_mode_t next_mode)
{
    s_mode = next_mode;
    piano_synth_all_notes_off();
    printf("\n>>> 切换至模式: 【%s】 <<<\n", get_mode_name(s_mode));

    if (s_mode == PIANO_MODE_GUIDE) {
        s_guide_step = 0;
        uint8_t target_note = g_star_song[0].note_idx;
        uint8_t target_key = song_note_to_key(target_note);
        led_piano_show_guide(target_note);
        printf("【跟弹指引】第 1/42 音: 请按键 [%s] -> 音符: %s, 歌词: \"%s\"\n",
               song_key_name(target_key), g_star_song[0].note_str, g_star_song[0].lyric);
    } else if (s_mode == PIANO_MODE_MAGIC) {
        s_magic_step = 0;
        s_next_auto_note_us = esp_timer_get_time() + 200000; /* 立即开始自动演奏 */
        printf("【一键魔法】开发板已开始自己自动弹奏《小星星》！按任意按键亦可互动伴奏！\n");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Booting EasyInput Piano...");

    /* 1. Safe boot sequence: Power rails */
    ESP_ERROR_CHECK(board_power_init());
    ESP_ERROR_CHECK(board_power_enable_peripherals(true));

    /* 2. Hardware peripherals */
    ESP_ERROR_CHECK(board_keys_init());
    ESP_ERROR_CHECK(led_piano_init());
    ESP_ERROR_CHECK(piano_synth_init());

    /* 默认音量调到最大 100% */
    piano_synth_set_volume(100);

    print_banner();
    switch_mode(PIANO_MODE_MAGIC); /* 开机直接进入一键魔法自动演奏 */

    board_input_snapshot_t in;
    while (true) {
        const int64_t now_us = esp_timer_get_time();

        /* Poll key inputs and encoder */
        esp_err_t err = board_keys_poll(&in);
        if (err == ESP_OK) {
            /* 1. Volume adjustment via rotary encoder */
            if (in.enc_delta != 0) {
                int new_vol = (int)piano_synth_get_volume() + (in.enc_delta * 5);
                if (new_vol < 0) new_vol = 0;
                if (new_vol > 100) new_vol = 100;
                piano_synth_set_volume((uint8_t)new_vol);
                led_piano_show_volume((uint8_t)new_vol);
                printf("[音量调节] 当前音量: %d%%\n", new_vol);
            }

            /* 2. Switch mode via encoder button */
            if (in.enc_just_pressed) {
                piano_play_mode_t next = (s_mode + 1) % 3; /* 在 魔法(0) / 跟弹(1) / 自由(2) 循环 */
                switch_mode(next);
            }

            /* 3. 一键魔法自动演奏模式：开发板自己自动按节拍弹奏 */
            if (s_mode == PIANO_MODE_MAGIC) {
                /* 自动推进时间轴 */
                if (now_us >= s_next_auto_note_us) {
                    const song_note_t *cur = &g_star_song[s_magic_step];
                    uint8_t key = song_note_to_key(cur->note_idx);

                    piano_synth_note_on(cur->note_idx, 127);
                    led_piano_on_note_hit(cur->note_idx);

                    /* 输出给终端及前端页面 */
                    printf("[魔法弹奏] 按键 %s -> 音符 %s (\"%s\") [%d/%d]\n",
                           song_key_name(key), cur->note_str, cur->lyric,
                           s_magic_step + 1, STAR_SONG_NOTE_COUNT);

                    /* 计算下一个音的时值：四分音符约 480ms，二分音符约 960ms */
                    int duration_ms = cur->duration_ms > 0 ? cur->duration_ms : 480;
                    s_next_auto_note_us = now_us + ((int64_t)duration_ms * 1000);

                    s_magic_step = (s_magic_step + 1) % STAR_SONG_NOTE_COUNT;
                    if (s_magic_step == 0) {
                        printf("\n🌟 《小星星》自动演奏完成一整轮！🌟\n");
                        led_piano_show_victory();
                        s_next_auto_note_us += 1200000; /* 庆祝停顿 1.2 秒后继续下一轮循环 */
                    }
                }

                /* 如果用户在此期间按了任意按键，也可以互动发声伴奏 */
                for (int i = 0; i < 8; ++i) {
                    if (in.pressed[i]) {
                        uint8_t note = song_key_to_note((uint8_t)i);
                        piano_synth_note_on(note, 127);
                        led_piano_on_note_hit(note);
                        printf("[魔法互动] 手动按下按键 %s 伴奏!\n", song_key_name(i));
                    }
                    if (in.released[i]) {
                        uint8_t note = song_key_to_note((uint8_t)i);
                        piano_synth_note_off(note);
                    }
                }
            } else if (s_mode == PIANO_MODE_FREE) {
                /* 自由琴键演奏模式 */
                for (int i = 0; i < 8; ++i) {
                    uint8_t note = song_key_to_note((uint8_t)i);
                    if (in.pressed[i]) {
                        piano_synth_note_on(note, 127);
                        led_piano_on_note_hit(note);
                        printf("[自由演奏] 按键 %s -> 音符 %s\n",
                               song_key_name(i), g_piano_note_names[note]);
                    }
                    if (in.released[i]) {
                        piano_synth_note_off(note);
                    }
                }
            } else if (s_mode == PIANO_MODE_GUIDE) {
                /* 跟弹教学模式 */
                const song_note_t *target = &g_star_song[s_guide_step];
                uint8_t target_note = target->note_idx;
                uint8_t target_key = song_note_to_key(target_note);
                led_piano_show_guide(target_note);

                for (int i = 0; i < 8; ++i) {
                    uint8_t note = song_key_to_note((uint8_t)i);
                    if (in.pressed[i]) {
                        piano_synth_note_on(note, 127);
                        led_piano_on_note_hit(note);

                        if (i == target_key) {
                            /* Hit the correct note! */
                            printf("  ✔ [弹奏正确!] %s (%s) -> 歌词: \"%s\" [%d/%d]\n",
                                   song_key_name(i), target->note_str, target->lyric,
                                   s_guide_step + 1, STAR_SONG_NOTE_COUNT);
                            s_guide_step++;

                            if (s_guide_step >= STAR_SONG_NOTE_COUNT) {
                                printf("\n🎉🎉 太棒了！您已完整弹奏完《小星星》整首曲子！🎉🎉\n\n");
                                led_piano_show_victory();
                                s_guide_step = 0;
                            }

                            /* Update guide for next note */
                            const song_note_t *next_target = &g_star_song[s_guide_step];
                            uint8_t next_key = song_note_to_key(next_target->note_idx);
                            printf("  >> 下一个音 [%d/%d]: 请按 [%s] -> 音符: %s, 歌词: \"%s\"\n",
                                   s_guide_step + 1, STAR_SONG_NOTE_COUNT,
                                   song_key_name(next_key), next_target->note_str, next_target->lyric);
                        } else {
                            /* Hit incorrect note */
                            printf("  ✘ [按键偏差] 按下了 %s (音符 %s)，目标是 [%s] (音符 %s, \"%s\")，再试一次！\n",
                                   song_key_name(i), g_piano_note_names[note],
                                   song_key_name(target_key), target->note_str, target->lyric);
                        }
                    }
                    if (in.released[i]) {
                        piano_synth_note_off(note);
                    }
                }
            }
        }

        /* 5. Update LED animations */
        led_piano_update(now_us, s_mode);

        vTaskDelay(pdMS_TO_TICKS(15));
    }
}
