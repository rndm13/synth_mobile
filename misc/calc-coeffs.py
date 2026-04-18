import scipy.signal as signal

SAMPLE_RATE = 48000
FLT_OVERSAMPLING = 2
FLT_OVERSAMPLED_RATE = SAMPLE_RATE * FLT_OVERSAMPLING
FLT_SAFE_CUTOFF_COEF = 0.45
FLT_CUTOFF = SAMPLE_RATE * FLT_SAFE_CUTOFF_COEF
FLT_IIR_STAGE_COUNT = 2
FLT_IIR_ORDER = FLT_IIR_STAGE_COUNT * 2

# 4th order elliptic filter.
# Max passband ripple: 0.1 dB. Min stopband attenuation: 100 dB
sos = signal.ellip(FLT_IIR_ORDER, 1, 30, FLT_CUTOFF, fs=FLT_OVERSAMPLED_RATE, output='sos')

print("Biquad Stages (b0, b1, b2, a0, a1, a2):")
for stage in sos:
    print(stage)
