#pragma once

#include <pthread.h>

#include "settings.h"
#include "utils.h"

#define FLT_SAFE_CUTOFF_COEF 0.45
#define FLT_FIR_TAPS         21
#define FLT_FIR_CUTOFF       (SAMPLE_RATE * FLT_SAFE_CUTOFF_COEF)

#define FLT_OVERSAMPLING     4
#define FLT_OVERSAMPLED_RATE (SAMPLE_RATE * FLT_OVERSAMPLING)

#define FLT_CUTOFF_MIN       100.0f
#define FLT_CUTOFF_MAX       (SAMPLE_RATE * FLT_SAFE_CUTOFF_COEF)
#define FLT_RESONANCE_MIN    0.5f
#define FLT_RESONANCE_MAX    20.0f
#define FLT_GAIN_MIN         -20.0f
#define FLT_GAIN_MAX         20.0f

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
    pthread_rwlock_t rw;

    FilterType type;
    float cutoff;
    float resonance;
    float gain;

    // Parameters calculated when any of the arguments are changed
    double norm;
    double a[3]; // Poles
    double b[3]; // Zeros
} FilterParams;

typedef struct FIRFilter {
    float coeffs[FLT_FIR_TAPS];
    float windows[FLT_FIR_TAPS];
    // TODO: Make this a circular buffer
    float history[FLT_FIR_TAPS];
} FIRFilter;

typedef struct Filter {
    // RW protected
    FilterParams params;

    // Filter state
    double x[3];
    double y[3];
    FIRFilter fir_up;
    FIRFilter fir_down;

    // Filter output
    float disp_buffer[DISPLAY_BUFFER_SIZE];
    float oversampled_buffer[BUFFER_SIZE * FLT_OVERSAMPLING];
} Filter;

void prepare_filter_params(Filter* flt);
void prepare_filter_display(Filter* flt);
void prepare_filter(Filter* flt);
void update_filter(Filter* flt, float* buffer, size_t n);

float update_fir_filter(FIRFilter *fir, float input);
void upsample_filter_u(Filter* flt, float* buffer, size_t n);
void downsample_filter_u(Filter* flt, float* buffer, size_t n);
