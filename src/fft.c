#include "fft.h"
#include "math.h"
#include "settings.h"

#include <stdint.h>

void fft(float data_re[], float data_im[], const uint32_t n) {
    rearrange(data_re, data_im, n);
    compute(data_re, data_im, n);
}

void rearrange(float data_re[], float data_im[], const uint32_t n) {
    uint32_t target = 0;
    for(uint32_t position = 0; position < n; position++) {
        if(target > position) {
            const float temp_re = data_re[target];
            const float temp_im = data_im[target];
            data_re[target] = data_re[position];
            data_im[target] = data_im[position];
            data_re[position] = temp_re;
            data_im[position] = temp_im;
        }
        uint32_t mask = n;
        while(target & (mask >>=1)) {
            target &= ~mask;
        }

        target |= mask;
    }
}

void compute(float data_re[], float data_im[], const uint32_t n) {
    for(uint32_t step = 1; step < n; step <<= 1) {
        const uint32_t jump = step << 1;
        const float step_d = (float) step;
        float twiddle_re = 1.0;
        float twiddle_im = 0.0;

        for(uint32_t group = 0; group < step; group++) {
            for(uint32_t pair = group; pair < n; pair += jump) {
                const uint32_t match = pair + step;
                const float product_re = twiddle_re * data_re[match] - twiddle_im * data_im[match];
                const float product_im = twiddle_im * data_re[match] + twiddle_re * data_im[match];
                data_re[match] = data_re[pair] - product_re;
                data_im[match] = data_im[pair] - product_im;
                data_re[pair] += product_re;
                data_im[pair] += product_im;
            }

            // we need the factors below for the next iteration
            // if we don't iterate then don't compute
            if (group + 1 == step) {
                continue;
            }

            float angle = -PI * ((float) group + 1) / step_d;
            twiddle_re = cos(angle);
            twiddle_im = sin(angle);
        }
    }
}
