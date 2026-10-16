#pragma once

#include <pthread.h>

#include "settings.h"
#include "utils.h"

#define FLT_TYPE_X(X)             \
    X(FT_LPF, "Low-pass filter")  \
    X(FT_HPF, "High-pass filter") \
    X(FT_BPF, "Band-pass filter") \
    X(FT_DISABLED, "Disabled")    \

typedef enum FilterType {
    FLT_TYPE_X(X_ENUM)
    FT_MAX,
} FilterType;

const char* ft2str(FilterType ft);

typedef struct FilterParams {
    FilterType type;
    float cutoff;
    float resonance;
    float env2_int;
    float gain;
} FilterParams;

typedef struct BiquadFilterState {
    double a[3]; // Poles
    double b[3]; // Zeros
} BiquadFilterState;

typedef struct FIRFilter {
    float coeffs[FLT_FIR_TAPS];
    float windows[FLT_FIR_TAPS];
    // TODO: Make this a circular buffer
    float history[FLT_FIR_TAPS];
} FIRFilter;

typedef struct FilterState {
    BiquadFilterState biquad;
    double x[3];
    double y[3];
    FIRFilter fir_up;
    FIRFilter fir_down;

    float oversampled_buffer[BUFFER_SIZE * FLT_OVERSAMPLING];
} FilterState;

void calc_biquad_filter_params(const FilterParams* flt, BiquadFilterState* o_biquad);
void prepare_filter_display(const FilterParams* flt, float disp_buffer[DISPLAY_BUFFER_SIZE]);

float update_fir_filter(FIRFilter *fir, float input);
void upsample_filter(FilterState* flt, float* buffer, size_t n);
void downsample_filter(FilterState* flt, float* buffer, size_t n);

void init_fir_filter(FIRFilter* fir, float sample_rate);
void init_filter_params(FilterParams *flt);
void init_filter_state(const FilterParams* params, FilterState *flt);
