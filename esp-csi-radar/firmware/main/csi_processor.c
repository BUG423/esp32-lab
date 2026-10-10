/**
 * CSI signal processor
 *
 * Turns raw per-packet CSI (int8 I/Q interleaved) into:
 *   - per-subcarrier amplitude
 *   - temporal variance (motion signature)
 *   - aggregated "motion energy"
 *   - adaptive presence decision
 *   - crude breathing-rate estimate (bandpass on dominant subcarrier phase)
 */

#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "csi_processor.h"

static const char *TAG = "csi_proc";

/* ---- tunables (from Kconfig) ---- */
static const float PRESENCE_FACTOR  = CONFIG_CSI_RADAR_PRESENCE_THRESHOLD_X100 / 100.0f;
static const float BASELINE_ALPHA   = CONFIG_CSI_RADAR_BASELINE_ALPHA_X1000 / 1000.0f;

/* ---- internal state ---- */
static struct {
    /* raw ring of amplitudes: [window][subcarrier] */
    float amp_ring[CSI_WINDOW_LEN][CSI_MAX_SUBCARRIERS];
    int   ring_idx;
    int   ring_filled;

    /* running stats */
    float baseline;
    bool  present;
    float presence_score;
    float breath_bpm;
    int   sample_count;

    /* breathing: short phase history on strongest subcarrier */
    float phase_hist[128];
    int   phase_idx;
    int   phase_filled;
    int64_t last_ts_us;
    int   last_phase_peak_count;
    float last_peak_ts[8];
    int   last_peak_n;

    SemaphoreHandle_t lock;
} s;

/* ---- helpers ---- */

static inline float amp_from_iq(int8_t imag, int8_t real)
{
    return sqrtf((float)imag * imag + (float)real * real);
}

/**
 * One-pole bandpass-ish filter for breathing (0.1–0.5 Hz).
 * We just use a simple IIR low-pass + high-pass cascade so we don't
 * pull in a full DSP dependency. Good enough for a demo BPM.
 */
static float breath_filtered(float x)
{
    static float lp = 0.f, hp = 0.f;
    /* low-pass ~0.5 Hz @ ~10 Hz publish rate */
    lp += 0.25f * (x - lp);
    /* high-pass ~0.1 Hz */
    hp += 0.05f * (lp - hp);
    return lp - hp;
}

static void update_breathing(float dominant_phase)
{
    /* store filtered signal, count zero-crossing peaks over last ~10 s */
    float f = breath_filtered(dominant_phase);

    s.phase_hist[s.phase_idx] = f;
    s.phase_idx = (s.phase_idx + 1) % (int)(sizeof(s.phase_hist) / sizeof(s.phase_hist[0]));
    if (s.phase_filled < (int)(sizeof(s.phase_hist) / sizeof(s.phase_hist[0]))) {
        s.phase_filled++;
    }
    if (s.phase_filled < 32) {
        return;
    }

    /* count positive-going zero crossings in the history */
    int size = (int)(sizeof(s.phase_hist) / sizeof(s.phase_hist[0]));
    int n = s.phase_filled;
    int start = (s.phase_idx - n + size) % size;

    int crossings = 0;
    float prev = s.phase_hist[start];
    for (int i = 1; i < n; i++) {
        int idx = (start + i) % size;
        float cur = s.phase_hist[idx];
        if (prev <= 0.f && cur > 0.f) {
            crossings++;
        }
        prev = cur;
    }

    /* history covers CSI_PUBLISH_PERIOD_MS * n  => seconds */
    float window_sec = (n * CSI_PUBLISH_PERIOD_MS) / 1000.f;
    if (window_sec > 0.5f) {
        float bpm = (crossings / window_sec) * 60.f;
        /* clamp to physiological range 6–40 (resting breathing) */
        if (bpm >= 6.f && bpm <= 40.f) {
            s.breath_bpm = s.breath_bpm * 0.8f + bpm * 0.2f;
        }
    }
}

/* ---- public API ---- */

esp_err_t csi_processor_init(void)
{
    memset(&s, 0, sizeof(s));
    s.lock = xSemaphoreCreateMutex();
    if (!s.lock) {
        return ESP_ERR_NO_MEM;
    }
    s.baseline = 1.0f;
    ESP_LOGI(TAG, "processor init (window=%d, threshold=x%.1f, alpha=%.3f)",
             CSI_WINDOW_LEN, PRESENCE_FACTOR, BASELINE_ALPHA);
    return ESP_OK;
}

void csi_processor_feed(const int8_t *buf, uint32_t len, int8_t rssi, int64_t ts_us)
{
    (void)rssi;
    if (!buf || len < 2) {
        return;
    }

    /* ESP32 CSI: interleaved [I0,Q0,I1,Q1,...] as int8; first 64 subcarriers
     * of the HT-LTF are the ones we use for HT20. */
    int n_sc = CSI_MAX_SUBCARRIERS;
    if ((int)len / 2 < n_sc) {
        n_sc = (int)len / 2;
    }

    /* compute amplitude vector */
    float amp[CSI_MAX_SUBCARRIERS];
    for (int i = 0; i < n_sc; i++) {
        /* some stacks deliver [real, imag], some [imag, real] — amplitude
         * is symmetric so it does not matter. */
        amp[i] = amp_from_iq(buf[i * 2], buf[i * 2 + 1]);
    }
    for (int i = n_sc; i < CSI_MAX_SUBCARRIERS; i++) {
        amp[i] = 0.f;
    }

    xSemaphoreTake(s.lock, portMAX_DELAY);

    /* push into ring */
    memcpy(s.amp_ring[s.ring_idx], amp, sizeof(amp));
    s.ring_idx = (s.ring_idx + 1) % CSI_WINDOW_LEN;
    if (s.ring_filled < CSI_WINDOW_LEN) {
        s.ring_filled++;
    }
    s.sample_count++;

    /* if we don't have enough history yet, just store */
    if (s.ring_filled < 4) {
        xSemaphoreGive(s.lock);
        return;
    }

    /* per-subcarrier temporal mean & variance */
    float var_sum = 0.f;
    float best_amp = -1.f;
    int   best_i = 0;

    for (int i = 0; i < CSI_MAX_SUBCARRIERS; i++) {
        float mean = 0.f;
        for (int k = 0; k < s.ring_filled; k++) {
            mean += s.amp_ring[k][i];
        }
        mean /= s.ring_filled;

        float v = 0.f;
        for (int k = 0; k < s.ring_filled; k++) {
            float d = s.amp_ring[k][i] - mean;
            v += d * d;
        }
        v /= s.ring_filled;

        var_sum += v;
        if (mean > best_amp) {
            best_amp = mean;
            best_i = i;
        }
    }

    float motion_energy = var_sum / CSI_MAX_SUBCARRIERS;

    /* adaptive baseline: track empty-room energy with slow EMA */
    if (motion_energy < s.baseline * 1.5f) {
        s.baseline += BASELINE_ALPHA * (motion_energy - s.baseline);
    }
    if (s.baseline < 0.01f) {
        s.baseline = 0.01f;
    }

    /* presence score: how far above baseline */
    float ratio = motion_energy / s.baseline;
    float score = 0.f;
    if (ratio > 1.f) {
        score = (ratio - 1.f) / (PRESENCE_FACTOR * 2.f);
        if (score > 1.f) score = 1.f;
    }
    s.presence_score = score;
    s.present = (ratio >= PRESENCE_FACTOR);

    /* crude breathing from dominant subcarrier phase */
    if (best_i * 2 + 1 < (int)len) {
        float imag = (float)buf[best_i * 2];
        float real = (float)buf[best_i * 2 + 1];
        float phase = atan2f(imag, real);
        update_breathing(phase);
    }

    xSemaphoreGive(s.lock);

    /* (frame snapshot is pulled by web layer on its own cadence) */
}

void csi_processor_get_frame(csi_frame_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));

    xSemaphoreTake(s.lock, portMAX_DELAY);

    /* recompute mean/var per subcarrier for the snapshot */
    float var_sum = 0.f;
    for (int i = 0; i < CSI_MAX_SUBCARRIERS; i++) {
        float mean = 0.f;
        for (int k = 0; k < s.ring_filled; k++) {
            mean += s.amp_ring[k][i];
        }
        if (s.ring_filled) mean /= s.ring_filled;
        float v = 0.f;
        for (int k = 0; k < s.ring_filled; k++) {
            float d = s.amp_ring[k][i] - mean;
            v += d * d;
        }
        if (s.ring_filled) v /= s.ring_filled;
        out->subcarrier_amp[i] = mean;
        out->subcarrier_var[i] = v;
        var_sum += v;
    }
    out->motion_energy  = var_sum / CSI_MAX_SUBCARRIERS;
    out->baseline       = s.baseline;
    out->presence_score = s.presence_score;
    out->present        = s.present;
    out->breath_bpm     = s.breath_bpm;
    out->sample_count   = s.sample_count;
    out->ts_us          = esp_timer_get_time();

    xSemaphoreGive(s.lock);
}

void csi_processor_recalibrate(void)
{
    xSemaphoreTake(s.lock, portMAX_DELAY);
    s.baseline = 0.01f;
    s.present = false;
    s.presence_score = 0.f;
    s.breath_bpm = 0.f;
    s.ring_filled = 0;
    s.ring_idx = 0;
    xSemaphoreGive(s.lock);
    ESP_LOGI(TAG, "baseline recalibrated");
}
