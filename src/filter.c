#include "filter.h"

#include <math.h>
#include <raymath.h>
#include <complex.h>
#include <pthread.h>

const char* ft2str(FilterType ft) {
    switch (ft) {
        FLT_TYPE_X(X_STR_CASE)
        default:
            break;
    }
    return "Unknown";
}

void prepare_filter_params(Filter* flt) {
    FilterParams* params = &flt->params;

    int e = pthread_rwlock_wrlock(&params->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    // double V = powf(10, fabs(params->gain) / 20);
    double K = tan(PI * params->cutoff / (double)FLT_OVERSAMPLED_RATE);

    params->a[0] = 1;
    switch (params->type) {
        case FT_LPF:
            params->norm = 1 / (1 + K / params->resonance + K * K);
            params->b[0] = K * K * params->norm;
            params->b[1] = 2 * params->b[0];
            params->b[2] = params->b[0];
            params->a[1] = 2 * (K * K - 1) * params->norm;
            params->a[2] = (1 - K / params->resonance + K * K) * params->norm;
            break;

        case FT_HPF:
            params->norm = 1 / (1 + K / params->resonance + K * K);
            params->b[0] = 1 * params->norm;
            params->b[1] = -2 * params->b[0];
            params->b[2] = params->b[0];
            params->a[1] = 2 * (K * K - 1) * params->norm;
            params->a[2] = (1 - K / params->resonance + K * K) * params->norm;
            break;

        case FT_BPF:
            params->norm = 1 / (1 + K / params->resonance + K * K);
            params->b[0] = K / params->resonance * params->norm;
            params->b[1] = 0;
            params->b[2] = -params->b[0];
            params->a[1] = 2 * (K * K - 1) * params->norm;
            params->a[2] = (1 - K / params->resonance + K * K) * params->norm;
            break;

        case FT_DISABLED:
            params->norm = 0;
            params->b[0] = 0;
            params->b[1] = 0;
            params->b[2] = 0;
            params->a[1] = 0;
            params->a[2] = 0;
            break;

        default:
            break;
    }

    e = pthread_rwlock_unlock(&params->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void prepare_filter_display(Filter* flt) {
    FilterParams* params = &flt->params;

    int e = pthread_rwlock_rdlock(&params->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    for (size_t i = 0; i < DISPLAY_BUFFER_SIZE; i++) {
        if (params->type == FT_DISABLED) {
            flt->disp_buffer[i] = 0.0f;
            continue;
        }

        double freq = FLT_CUTOFF_MIN * powf(FLT_CUTOFF_MAX / FLT_CUTOFF_MIN, (double)(i) / (DISPLAY_BUFFER_SIZE - 1));

        // Current frequency in radians (0 to PI)
        double w = 2.0 * PI * freq / FLT_OVERSAMPLED_RATE;

        double complex z1 = cexp(-I * w);
        double complex z2 = cexp(-I * 2.0 * w);

        // H(z) = (b0 + b1*z^-1 + b2*z^-2) / (a0 + a1*z^-1 + a2*z^-2)
        double complex num = params->a[0] + params->a[1] * z1 + params->a[2] * z2;
        double complex den = params->b[0] + params->b[1] * z1 + params->b[2] * z2;

        double nMag = cabs(num);
        double dMag = cabs(den);

        if (dMag < EPSILON) {
            flt->disp_buffer[i] = -1.0f;
        } else {
            double magnitude = nMag / dMag;

            flt->disp_buffer[i] = Clamp(-log10f(magnitude), -1, 1);
        }
    }

    e = pthread_rwlock_unlock(&params->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void prepare_filter(Filter* flt) {
    prepare_filter_params(flt);
    prepare_filter_display(flt);
}

void update_filter(Filter* flt, float* buffer, size_t n) {
    // int e = pthread_rwlock_rdlock(&flt->params.rw);
    // if (e != 0) {
    //     // TODO: Log
    //     return;
    // }

    if (flt->params.type == FT_DISABLED) {
        goto unlock;
    }

    upsample_filter_u(flt, buffer, n);

    // Converts the buffer data before using it
    for (size_t i = 0; i < n * FLT_OVERSAMPLING; i++) {
        // Move old state
        flt->y[2] = flt->y[1];
        flt->y[1] = flt->y[0];
        flt->x[2] = flt->x[1];
        flt->x[1] = flt->x[0];

        flt->x[0] = flt->oversampled_buffer[i];
        flt->y[0] =
            flt->params.b[0] * flt->x[0] +
            flt->params.b[1] * flt->x[1] +
            flt->params.b[2] * flt->x[2] -
            flt->params.a[1] * flt->y[1] -
            flt->params.a[2] * flt->y[2] +
            EPSILON; // Anti-denormal offset

        flt->oversampled_buffer[i] = flt->y[0];
    }

    downsample_filter_u(flt, buffer, n);

unlock:
    // e = pthread_rwlock_unlock(&flt->params.rw);
    // if (e != 0) {
    //     // TODO: Log
    //     return;
    // }
    return;
}

float update_fir_filter(FIRFilter *fir, float input) {
    for (int i = FLT_FIR_TAPS - 1; i > 0; i--) {
        fir->history[i] = fir->history[i - 1];
    }
    fir->history[0] = input;

    float output = 0;
    for (int i = 0; i < FLT_FIR_TAPS; i++) {
        output += fir->history[i] * fir->coeffs[i];
    }

    return output;
}

void upsample_filter_u(Filter* flt, float* buffer, size_t n) {
    for (size_t i = 0; i < n * FLT_OVERSAMPLING; i++) {
        flt->oversampled_buffer[i] = 0.0f;
    }

    for (size_t i = 0; i < n; i++) {
        flt->oversampled_buffer[i * FLT_OVERSAMPLING] += buffer[i] * FLT_OVERSAMPLING;
    }

    for (size_t i = 0; i < n * FLT_OVERSAMPLING; i++) {
        flt->oversampled_buffer[i] = update_fir_filter(&flt->fir_up, flt->oversampled_buffer[i]);
    }
}

void downsample_filter_u(Filter* flt, float* buffer, size_t n) {
    for (size_t i = 0; i < n * FLT_OVERSAMPLING; i++) {
        flt->oversampled_buffer[i] = update_fir_filter(&flt->fir_down, flt->oversampled_buffer[i]);
    }

    for (size_t i = 0; i < n; i++) {
        buffer[i] = flt->oversampled_buffer[i * FLT_OVERSAMPLING];
    }
}

void init_fir_filter(FIRFilter* fir, float sample_rate) {
    double ft = FLT_FIR_CUTOFF / sample_rate;
    double sum = 0;

    for (int i = 0; i < FLT_FIR_TAPS; i++) {
        int n = i - (FLT_FIR_TAPS - 1) / 2;

        // The Sinc function
        if (n == 0) {
            fir->coeffs[i] = 2.0f * ft;
        } else {
            fir->coeffs[i] = sinf(2.0f * PI * ft * n) / (PI * n);
        }

        // Apply Hamming Window
        float window = 0.54f - 0.46f * cosf(2.0f * PI * i / (FLT_FIR_TAPS - 1));
        fir->coeffs[i] *= window;

        sum += fir->coeffs[i];
    }

    // Normalize coefficients so the gain is 1.0 (0dB)
    for (int i = 0; i < FLT_FIR_TAPS; i++) {
        fir->coeffs[i] /= sum;
    }
}

void init_filter(Filter *flt) {
    int e = pthread_rwlock_init(&flt->params.rw, NULL);
    if (e != 0) {
        return;
    }
    flt->params.type = FT_LPF;
    flt->params.cutoff = FLT_CUTOFF_MAX;
    flt->params.resonance = FLT_RESONANCE_MIN;
    flt->params.gain = 0.0f;
    init_fir_filter(&flt->fir_up, FLT_OVERSAMPLED_RATE);
    init_fir_filter(&flt->fir_down, FLT_OVERSAMPLED_RATE);

    prepare_filter(flt);
}

void deinit_filter(Filter *flt) {
    int e = 0;
    e = pthread_rwlock_destroy(&flt->params.rw);
}
