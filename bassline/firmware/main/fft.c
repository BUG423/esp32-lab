#include "fft.h"

#include <math.h>
#include <string.h>

static int s_n;
static int s_bits;
static float s_twiddle_re[FFT_MAX_N / 2];
static float s_twiddle_im[FFT_MAX_N / 2];
static float s_hann[FFT_MAX_N];
static uint16_t s_bitrev[FFT_MAX_N];

static float s_re[FFT_MAX_N];
static float s_im[FFT_MAX_N];

static int s_ilog2(int n)
{
    int r = 0;
    while ((1 << r) < n) {
        r++;
    }
    return r;
}

void fft_init(int n)
{
    if (n < 64) {
        n = 64;
    }
    if (n > FFT_MAX_N) {
        n = FFT_MAX_N;
    }
    /* snap down to power of two */
    while (n & (n - 1)) {
        n &= (n - 1);
    }
    s_n = n;
    s_bits = s_ilog2(n);

    for (int i = 0; i < n; i++) {
        uint16_t rev = 0;
        for (int b = 0; b < s_bits; b++) {
            if (i & (1 << b)) {
                rev |= (uint16_t)(1 << (s_bits - 1 - b));
            }
        }
        s_bitrev[i] = rev;
    }

    for (int k = 0; k < n / 2; k++) {
        float ang = -2.0f * (float)M_PI * (float)k / (float)n;
        s_twiddle_re[k] = cosf(ang);
        s_twiddle_im[k] = sinf(ang);
    }

    /* Hann window (periodic-ish, endpoints handled) */
    if (n > 1) {
        for (int i = 0; i < n; i++) {
            s_hann[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i / (float)(n - 1)));
        }
    } else {
        s_hann[0] = 1.0f;
    }
}

void fft_compute(const float *input, float *magnitudes)
{
    int n = s_n;
    if (n <= 0) {
        return;
    }

    for (int i = 0; i < n; i++) {
        int j = s_bitrev[i];
        s_re[j] = input[i] * s_hann[i];
        s_im[j] = 0.0f;
    }

    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1;
        int step = n / len;
        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; j++) {
                float wr = s_twiddle_re[j * step];
                float wi = s_twiddle_im[j * step];
                float tr = s_re[i + j + half] * wr - s_im[i + j + half] * wi;
                float ti = s_re[i + j + half] * wi + s_im[i + j + half] * wr;
                s_re[i + j + half] = s_re[i + j] - tr;
                s_im[i + j + half] = s_im[i + j] - ti;
                s_re[i + j] += tr;
                s_im[i + j] += ti;
            }
        }
    }

    int half_n = n / 2;
    for (int i = 0; i < half_n; i++) {
        magnitudes[i] = sqrtf(s_re[i] * s_re[i] + s_im[i] * s_im[i]);
    }
}

void fft_log_bins(const float *mags, int n_half, float sample_rate,
                  float *out_bins, int n_bins, float f_min, float f_max)
{
    if (n_half < 2 || n_bins < 1) {
        return;
    }
    if (f_min < 1.0f) {
        f_min = 1.0f;
    }
    if (f_max <= f_min) {
        f_max = sample_rate * 0.5f;
    }

    float bin_hz = sample_rate / (float)(n_half * 2);
    float ratio = f_max / f_min;

    for (int i = 0; i < n_bins; i++) {
        float f0 = f_min * powf(ratio, (float)i / (float)n_bins);
        float f1 = f_min * powf(ratio, (float)(i + 1) / (float)n_bins);
        int b0 = (int)(f0 / bin_hz);
        int b1 = (int)(f1 / bin_hz);
        if (b0 < 1) {
            b0 = 1; /* skip DC */
        }
        if (b1 <= b0) {
            b1 = b0 + 1;
        }
        if (b1 > n_half) {
            b1 = n_half;
        }
        float sum = 0.0f;
        for (int b = b0; b < b1; b++) {
            sum += mags[b];
        }
        out_bins[i] = sum / (float)(b1 - b0);
    }
}
