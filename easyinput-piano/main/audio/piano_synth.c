#include "piano_synth.h"
#include "board_pins.h"
#include "board_power.h"

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>

static const char *TAG = "piano_synth";

#define AUDIO_SAMPLE_RATE    32000
#define AUDIO_BLOCK_FRAMES   128
#define AUDIO_STEREO_SAMPLES (AUDIO_BLOCK_FRAMES * 2)
#define SYNTH_VOICE_COUNT    8
#define EVENT_QUEUE_LEN      32

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Standard frequencies for C4 .. C5 */
const float g_piano_freqs[PIANO_TOTAL_NOTES] = {
    261.63f, /* 0: C4 (Do)  */
    293.66f, /* 1: D4 (Re)  */
    329.63f, /* 2: E4 (Mi)  */
    349.23f, /* 3: F4 (Fa)  */
    392.00f, /* 4: G4 (Sol) */
    440.00f, /* 5: A4 (La)  */
    493.88f, /* 6: B4 (Si)  */
    523.25f, /* 7: C5 (Do)  */
};

const char * const g_piano_note_names[PIANO_TOTAL_NOTES] = {
    "1 (Do/C4)",
    "2 (Re/D4)",
    "3 (Mi/E4)",
    "4 (Fa/F4)",
    "5 (Sol/G4)",
    "6 (La/A4)",
    "7 (Si/B4)",
    "i (Do/C5)",
};

typedef enum {
    EVT_NOTE_ON,
    EVT_NOTE_OFF,
    EVT_ALL_OFF,
} synth_event_type_t;

typedef struct {
    synth_event_type_t type;
    uint8_t note_idx;
    uint8_t velocity;
} synth_event_t;

typedef struct {
    bool active;
    bool releasing;
    uint8_t note_idx;
    float phase;
    float phase_step;
    float envelope;
    float decay_rate;
    float release_rate;
    float velocity_gain;
    uint32_t sample_count;
} synth_voice_t;

static i2s_chan_handle_t s_tx_chan;
static QueueHandle_t s_evt_queue;
static int16_t s_render_buf[AUDIO_STEREO_SAMPLES];
static uint8_t s_master_volume = 100;
static portMUX_TYPE s_vol_lock = portMUX_INITIALIZER_UNLOCKED;

static inline float fast_sin(float phase)
{
    /* Keep phase in [0, 2*PI) */
    while (phase >= (float)(2.0 * M_PI)) phase -= (float)(2.0 * M_PI);
    while (phase < 0.0f) phase += (float)(2.0 * M_PI);
    return sinf(phase);
}

static void voice_init(synth_voice_t *v, uint8_t note_idx, uint8_t velocity)
{
    v->active = true;
    v->releasing = false;
    v->note_idx = note_idx;
    v->phase = 0.0f;
    const float freq = g_piano_freqs[note_idx];
    v->phase_step = (2.0f * (float)M_PI * freq) / (float)AUDIO_SAMPLE_RATE;
    v->envelope = 1.0f;
    /* Natural sustain decay: exp decay, roughly 1.5s - 2.5s depending on frequency */
    v->decay_rate = 0.99988f - (freq * 0.0000002f);
    /* Damper release rate: stops within ~100ms once key released */
    v->release_rate = 0.9982f;
    v->velocity_gain = (float)velocity / 127.0f;
    v->sample_count = 0;
}

static void audio_synth_task(void *arg)
{
    (void)arg;
    synth_voice_t voices[SYNTH_VOICE_COUNT] = {0};

    while (true) {
        /* Process all pending Note-On / Note-Off events */
        synth_event_t evt;
        while (xQueueReceive(s_evt_queue, &evt, 0) == pdTRUE) {
            if (evt.type == EVT_NOTE_ON) {
                if (evt.note_idx < PIANO_TOTAL_NOTES) {
                    /* Allocate or reuse the voice for this note */
                    voice_init(&voices[evt.note_idx], evt.note_idx, evt.velocity);
                }
            } else if (evt.type == EVT_NOTE_OFF) {
                if (evt.note_idx < PIANO_TOTAL_NOTES && voices[evt.note_idx].active) {
                    voices[evt.note_idx].releasing = true;
                }
            } else if (evt.type == EVT_ALL_OFF) {
                for (int i = 0; i < SYNTH_VOICE_COUNT; ++i) {
                    voices[i].active = false;
                }
            }
        }

        portENTER_CRITICAL(&s_vol_lock);
        const float vol_scale = ((float)s_master_volume / 100.0f) * 28500.0f;
        portEXIT_CRITICAL(&s_vol_lock);

        /* Render one audio block (AUDIO_BLOCK_FRAMES samples) */
        for (int frame = 0; frame < AUDIO_BLOCK_FRAMES; ++frame) {
            float mixed = 0.0f;

            for (int i = 0; i < SYNTH_VOICE_COUNT; ++i) {
                synth_voice_t *v = &voices[i];
                if (!v->active) {
                    continue;
                }

                /* Piano harmonic synthesis:
                 * Fundamental (f0) + 2nd harmonic (f1) + 3rd harmonic (f2) + 4th harmonic
                 */
                const float p = v->phase;
                const float s1 = 1.20f * fast_sin(p);
                const float s2 = 0.60f * fast_sin(p * 2.0f);
                const float s3 = 0.25f * fast_sin(p * 3.0f);
                const float s4 = 0.12f * fast_sin(p * 4.0f);

                /* Attack ramp for first 64 samples (2ms) to avoid clicks */
                float attack = 1.0f;
                if (v->sample_count < 64) {
                    attack = (float)v->sample_count / 64.0f;
                }

                /* Hammer strike transient: short brightness boost on initial strike */
                float strike = 0.0f;
                if (v->sample_count < 800) {
                    strike = 0.45f * expf(-((float)v->sample_count / 140.0f)) * fast_sin(p * 5.0f);
                }

                const float voice_out = (s1 + s2 + s3 + s4 + strike) * v->envelope * attack * v->velocity_gain;
                mixed += voice_out;

                /* Advance phase */
                v->phase += v->phase_step;
                if (v->phase >= (float)(2.0 * M_PI)) {
                    v->phase -= (float)(2.0 * M_PI);
                }

                /* Decay envelope */
                if (v->releasing) {
                    v->envelope *= v->release_rate;
                } else {
                    v->envelope *= v->decay_rate;
                }
                v->sample_count++;

                /* Voice cutoff threshold */
                if (v->envelope < 0.0015f) {
                    v->active = false;
                }
            }

            /* Soft limiter with gentle saturation to keep maximum acoustic volume */
            if (mixed > 1.05f) mixed = 1.05f;
            else if (mixed < -1.05f) mixed = -1.05f;

            int16_t out_sample = (int16_t)(mixed * vol_scale);
            s_render_buf[frame * 2]     = out_sample; /* Left channel */
            s_render_buf[frame * 2 + 1] = out_sample; /* Right channel */
        }

        /* Stream out through I2S DMA */
        size_t bytes_written = 0;
        i2s_channel_write(s_tx_chan, s_render_buf, sizeof(s_render_buf), &bytes_written, portMAX_DELAY);
    }
}

esp_err_t piano_synth_init(void)
{
    ESP_RETURN_ON_FALSE(board_power_peripherals_enabled(), ESP_ERR_INVALID_STATE, TAG,
                        "GPIO8 power rail must be enabled before audio DAC");

    s_evt_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(synth_event_t));
    ESP_RETURN_ON_FALSE(s_evt_queue != NULL, ESP_ERR_NO_MEM, TAG, "failed to create synth event queue");

    /* Setup I2S channel */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = AUDIO_BLOCK_FRAMES;
    chan_cfg.auto_clear = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx_chan, NULL), TAG, "i2s_new_channel failed");

    const i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BOARD_GPIO_SPK_BCLK,
            .ws = BOARD_GPIO_SPK_WS,
            .dout = BOARD_GPIO_SPK_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx_chan, &std_cfg), TAG, "i2s_channel_init_std_mode failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx_chan), TAG, "i2s_channel_enable failed");

    BaseType_t ret = xTaskCreatePinnedToCore(audio_synth_task, "piano_audio", 4096, NULL, 9, NULL, 0);
    ESP_RETURN_ON_FALSE(ret == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to spawn audio task");

    ESP_LOGI(TAG, "piano audio engine ready: %d Hz, 8 voices", AUDIO_SAMPLE_RATE);
    return ESP_OK;
}

void piano_synth_note_on(uint8_t note_idx, uint8_t velocity)
{
    if (!s_evt_queue) return;
    synth_event_t evt = {
        .type = EVT_NOTE_ON,
        .note_idx = note_idx,
        .velocity = velocity ? velocity : 127,
    };
    xQueueSend(s_evt_queue, &evt, 0);
}

void piano_synth_note_off(uint8_t note_idx)
{
    if (!s_evt_queue) return;
    synth_event_t evt = {
        .type = EVT_NOTE_OFF,
        .note_idx = note_idx,
        .velocity = 0,
    };
    xQueueSend(s_evt_queue, &evt, 0);
}

void piano_synth_set_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    portENTER_CRITICAL(&s_vol_lock);
    s_master_volume = volume;
    portEXIT_CRITICAL(&s_vol_lock);
    ESP_LOGI(TAG, "volume -> %d%%", volume);
}

uint8_t piano_synth_get_volume(void)
{
    portENTER_CRITICAL(&s_vol_lock);
    uint8_t v = s_master_volume;
    portEXIT_CRITICAL(&s_vol_lock);
    return v;
}

void piano_synth_all_notes_off(void)
{
    if (!s_evt_queue) return;
    synth_event_t evt = {
        .type = EVT_ALL_OFF,
    };
    xQueueSend(s_evt_queue, &evt, 0);
}
