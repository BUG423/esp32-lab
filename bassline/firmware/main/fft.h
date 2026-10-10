#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FFT_MAX_N 512

/** Initialise tables for an N-point real FFT (N must be power of two, 64..512). */
void fft_init(int n);

/**
 * Compute magnitude spectrum of n real samples.
 * Applies a Hann window internally.
 * magnitudes[] receives n/2 values (DC .. Nyquist-1).
 */
void fft_compute(const float *input, float *magnitudes);

/**
 * Fold an n/2 linear magnitude spectrum into n_bins log-spaced bands
 * between f_min and f_max (Hz). Output values are averaged magnitudes.
 */
void fft_log_bins(const float *mags, int n_half, float sample_rate,
                  float *out_bins, int n_bins, float f_min, float f_max);

#ifdef __cplusplus
}
#endif
