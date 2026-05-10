#pragma once

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef EPSILON
#define EPSILON 0.000001f
#endif

#define SAMPLE_RATE    48000

#define OSC_SEMI_RANGE 12
#define OSC_CENTS_RANGE 100

#define OSC_UNISON_MIN 1
#define OSC_UNISON_MAX 8

#define OSC_DETUNE_MIN 5
#define OSC_DETUNE_MAX 300

#define ENV_A_MIN 0.2f
#define ENV_A_MAX 5.0f
#define ENV_D_MIN 0.2f
#define ENV_D_MAX 5.0f
#define ENV_R_MIN 0.2f
#define ENV_R_MAX 5.0f

#define ENV_COUNT    2
#define OSC_COUNT    2

#define DISPLAY_BUFFER_SIZE 256
#define BUFFER_SIZE         4096
#define FFT_BUFFER_SIZE_MUL 2
#define FFT_BUFFER_SIZE     (BUFFER_SIZE * FFT_BUFFER_SIZE_MUL)

#define MAX_VELOCITY 80.0f

#define CENTS_IN_SEMI 100
#define CENTS_IN_OCTAVE (CENTS_IN_SEMI * KEY_OCTAVE)

#define PROGRAM_NAME_CAPACITY 16
#define PROGRAM_NAME_INIT     "INIT SYNTH"
#define PROGRAM_COUNT_MAX     128
#define FILEPATH_CAPACITY     512

#define PROGRAM_MAX_SIZE      20000
#define ANDROID_APP_DIR_PATH  "/storage/emulated/0/Android/data/com.raylib.synth/files"
#define PROGRAM_PATH          ANDROID_APP_DIR_PATH
#define PROGRAM_EXTENSION     ".ini"
#define INI_SECTION_CAPACITY  20

#define ERR_MSG_CAPACITY      256

typedef struct timespec timespec_t;
