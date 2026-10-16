#include "file.h"

#define __USE_XOPEN_EXTENDED 1
#define _XOPEN_SOURCE 500
#include <ftw.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ini.h"
#include "filter.h"
#include "settings.h"

ProgramSelection* g_ps = NULL;

#define MATCH_S(var, exp_section, exp_name, section, name, value)                       \
    do {                                                                                \
        if (strcmp((exp_section), (section)) == 0 && strcmp((exp_name), (name)) == 0) { \
            snprintf(var, sizeof(var), "%s", value);                                    \
        }                                                                               \
    } while (0);

#define MATCH_I(var, exp_section, exp_name, section, name, value)                       \
    do {                                                                                \
        if (strcmp((exp_section), (section)) == 0 && strcmp((exp_name), (name)) == 0) { \
            (var) = atoi(value);                                                        \
        }                                                                               \
    } while (0);

#define MATCH_F(var, exp_section, exp_name, section, name, value)                       \
    do {                                                                                \
        if (strcmp((exp_section), (section)) == 0 && strcmp((exp_name), (name)) == 0) { \
            (var) = atof(value);                                                        \
        }                                                                               \
    } while (0);

#define RAND_RANGE(min, max) ((rand() % ((max) - (min))) + (min))
#define RAND_RANGEF(min, max) (RAND_RANGE((int)((min) * 100), (int)((max) * 100)) / 100.0f)

static int add_program_entry(
        const char *filepath, const struct stat *info,
        const int typeflag, struct FTW *pathinfo) {
    const char* filename = filepath + pathinfo->base;
    const char* extension_substr = NULL;
    size_t program_len = 0;

    if (g_ps == NULL) {
        return EINVAL;
    }

    extension_substr = strstr(filename, PROGRAM_EXTENSION);
    if (extension_substr == NULL) {
        return 0;
    }

    program_len = MIN(extension_substr - filename, PROGRAM_NAME_CAPACITY);

    snprintf(g_ps->filepath_arr[g_ps->program_count], FILEPATH_CAPACITY, "%s", filepath);
    snprintf(g_ps->program_name_arr[g_ps->program_count], program_len + 1, "%s", filename);

    g_ps->program_count++;

    return 0;
}

int init_program_selection(const char* dirpath, ProgramSelection* ps) {
    int e = 0;

    g_ps = ps;
    g_ps->program_count = 0;
    g_ps->selected_program_idx = 0;

    e = nftw(dirpath, add_program_entry, 10, 0);
    if (e < 0) {
        e = errno;
    }

    g_ps = NULL;

    return e;
}

static void add_program(const SynthParams* s, ProgramSelection* ps) {
    char filepath[FILEPATH_CAPACITY] = {0};

    snprintf(
        filepath, FILEPATH_CAPACITY,
        "%s/%s%s", PROGRAM_PATH, s->gen.program_name, PROGRAM_EXTENSION);

    snprintf(
        ps->filepath_arr[ps->program_count],
        FILEPATH_CAPACITY, "%s", filepath);
    snprintf(
        ps->program_name_arr[ps->program_count],
        PROGRAM_NAME_CAPACITY, "%s", s->gen.program_name);

    ps->selected_program_idx = ps->program_count;
    ps->program_count++;
}

static int write_ini_value_s(FILE* file, const char* value, const char* section, const char* name) {
    int e = 0;

    e = fprintf(file, "[%s]\n%s = %s\n", section, name, value);
    if (e < 0) {
        return errno;
    }

    return 0;
}

static int write_ini_value_i(FILE* file, int value, const char* section, const char* name) {
    int e = 0;

    e = fprintf(file, "[%s]\n%s = %d\n", section, name, value);
    if (e < 0) {
        return errno;
    }

    return 0;
}

static int write_ini_value_f(FILE* file, float value, const char* section, const char* name) {
    int e = 0;

    e = fprintf(file, "[%s]\n%s = %f\n", section, name, value);
    if (e < 0) {
        return errno;
    }

    return 0;
}

static int write_ini_file(SynthParams *s, const char* filepath) {
    char cur_section[INI_SECTION_CAPACITY] = {};
    int e = 0;
    FILE* file = NULL;
    OscParams *oparams = NULL;
    Env *eparams = NULL;
    FilterParams *fparams = NULL;
    SynthGeneralParams *sparams = NULL;
    DistortionParams *distparams = NULL;
    DelayParams *delparams = NULL;

    file = fopen(filepath, "w");

    if (NULL == file) {
        return errno;
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%zu", "osc", i);

        oparams = &s->osc_arr[i];

        e = write_ini_value_i(file, oparams->cents, cur_section, "cents");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_i(file, oparams->detune, cur_section, "detune");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_i(file, oparams->unison, cur_section, "unison");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_i(file, oparams->semi, cur_section, "semi");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, oparams->volume, cur_section, "volume");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_i(file, oparams->type, cur_section, "type");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_i(file, oparams->rand_phase, cur_section, "rand_phase");
        if (e != 0) {
            goto close_file;
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->env_arr); i++) {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%zu", "env", i);

        eparams = &s->env_arr[i];

        e = write_ini_value_f(file, eparams->attack, cur_section, "attack");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, eparams->decay, cur_section, "decay");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, eparams->sustain, cur_section, "sustain");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, eparams->release, cur_section, "release");
        if (e != 0) {
            goto close_file;
        }
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%d", "flt", 0);

        fparams = &s->flt;

        e = write_ini_value_i(file, fparams->type, cur_section, "type");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, fparams->cutoff, cur_section, "cutoff");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, fparams->resonance, cur_section, "resonance");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, fparams->gain, cur_section, "gain");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, fparams->env2_int, cur_section, "env2_int");
        if (e != 0) {
            goto close_file;
        }
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s", "synth");

        sparams = &s->gen;

        e = write_ini_value_f(file, sparams->amp, cur_section, "amp");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, sparams->pan, cur_section, "pan");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_s(file, sparams->program_name, cur_section, "name");
        if (e != 0) {
            goto close_file;
        }
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s", "distortion");

        distparams = &s->distortion;

        e = write_ini_value_f(file, distparams->wet_dry_ratio, cur_section, "wet_dry_ratio");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, distparams->gain, cur_section, "gain");
        if (e != 0) {
            goto close_file;
        }
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s", "delay");

        delparams = &s->delay;

        e = write_ini_value_f(file, delparams->wet_dry_ratio, cur_section, "wet_dry_ratio");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, delparams->feedback, cur_section, "feedback");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, delparams->delay_s, cur_section, "delay_s");
        if (e != 0) {
            goto close_file;
        }
    }

close_file:
    fclose(file);
    return e;
}

int save_program(Synth *s, ProgramSelection* ps) {
    const char* current_program = NULL;
    int e = 0;
    bool found = false;

    for (size_t i = 0; i < ps->program_count; i++) {
        current_program = ps->program_name_arr[i];
        if (0 == strncmp(current_program, s->gui_params.gen.program_name, PROGRAM_NAME_CAPACITY)) {
            ps->selected_program_idx = i;
            found = true;
            break;
        }
    }

    if (!found) {
        add_program(&s->gui_params, ps);
    }

    e = write_ini_file(&s->gui_params, ps->filepath_arr[ps->selected_program_idx]);

    return e;
}

static int read_ini_value(void* user, const char* section, const char* name, const char* value) {
    SynthParams *s = (SynthParams*)user;
    char cur_section[INI_SECTION_CAPACITY] = {};

    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%zu", "osc", i);

        OscParams *params = &s->osc_arr[i];

        MATCH_I(params->cents, cur_section, "cents", section, name, value);
        MATCH_I(params->detune, cur_section, "detune", section, name, value);
        MATCH_I(params->unison, cur_section, "unison", section, name, value);
        MATCH_I(params->semi, cur_section, "semi", section, name, value);
        MATCH_F(params->volume, cur_section, "volume", section, name, value);
        MATCH_I(params->type, cur_section, "type", section, name, value);
        MATCH_I(params->rand_phase, cur_section, "rand_phase", section, name, value);
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->env_arr); i++) {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%zu", "env", i);

        Env *params = &s->env_arr[i];

        MATCH_F(params->attack, cur_section, "attack", section, name, value);
        MATCH_F(params->decay, cur_section, "decay", section, name, value);
        MATCH_F(params->sustain, cur_section, "sustain", section, name, value);
        MATCH_F(params->release, cur_section, "release", section, name, value);
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%d", "flt", 0);

        FilterParams *params = &s->flt;

        MATCH_I(params->type, cur_section, "type", section, name, value);
        MATCH_F(params->cutoff, cur_section, "cutoff", section, name, value);
        MATCH_F(params->resonance, cur_section, "resonance", section, name, value);
        MATCH_F(params->gain, cur_section, "gain", section, name, value);
        MATCH_F(params->env2_int, cur_section, "env2_int", section, name, value);
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s", "synth");

        SynthGeneralParams *params = &s->gen;

        MATCH_F(params->amp, cur_section, "amp", section, name, value);
        MATCH_F(params->pan, cur_section, "pan", section, name, value);
        MATCH_S(params->program_name, cur_section, "name", section, name, value);
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s", "distortion");

        DistortionParams *params = &s->distortion;

        MATCH_F(params->wet_dry_ratio, cur_section, "wet_dry_ratio", section, name, value);
        MATCH_F(params->gain, cur_section, "gain", section, name, value);
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s", "delay");

        DelayParams *params = &s->delay;

        MATCH_F(params->wet_dry_ratio, cur_section, "wet_dry_ratio", section, name, value);
        MATCH_F(params->feedback, cur_section, "feedback", section, name, value);
        MATCH_F(params->delay_s, cur_section, "delay_s", section, name, value);
    }

    return 1;
}

int open_program(Synth *s, const ProgramSelection* ps) {
    int e = 0;
    e = ini_parse(ps->filepath_arr[ps->selected_program_idx], read_ini_value, &s->gui_params);

    prepare_osc_display(&s->gui_params.osc_arr[0], s->osc_disp_buffer_arr[0]);
    prepare_osc_display(&s->gui_params.osc_arr[1], s->osc_disp_buffer_arr[1]);
    prepare_filter_display(&s->gui_params.flt, s->flt_disp_buffer);

    return e;
}

void randomize_program(Synth *s) {
    srand(time(NULL));
    SynthParams *sp = &s->gui_params;

    for (size_t i = 0; i < ARRAY_SIZE(sp->osc_arr); i++) {
        OscParams *params = &sp->osc_arr[i];

        params->cents = RAND_RANGE(-OSC_CENTS_RANGE, OSC_CENTS_RANGE);
        params->detune = RAND_RANGE(OSC_DETUNE_MIN, OSC_DETUNE_MAX);
        params->unison = RAND_RANGE(OSC_UNISON_MIN, OSC_UNISON_MAX);
        params->semi = RAND_RANGE(-OSC_SEMI_RANGE, OSC_SEMI_RANGE);
        params->volume = RAND_RANGEF(0, 1);
        params->type = RAND_RANGE(0, OT_MAX);

        prepare_osc_display(&sp->osc_arr[i], s->osc_disp_buffer_arr[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(sp->env_arr); i++) {
        Env *params = &sp->env_arr[i];

        params->attack = RAND_RANGEF(ENV_A_MIN, ENV_A_MAX);
        params->decay = RAND_RANGEF(ENV_D_MIN, ENV_D_MAX);
        params->sustain = RAND_RANGEF(0, 1);
        params->release = RAND_RANGEF(ENV_R_MIN, ENV_R_MAX);
    }

    {
        FilterParams *params = &sp->flt;

        params->type = RAND_RANGE(0, FT_MAX);
        params->cutoff = RAND_RANGEF(FLT_CUTOFF_MIN, FLT_CUTOFF_MAX);
        params->resonance = RAND_RANGEF(FLT_RESONANCE_MIN, FLT_RESONANCE_MAX);
        params->gain = RAND_RANGEF(FLT_GAIN_MIN, FLT_GAIN_MAX);

        prepare_filter_display(&sp->flt, s->flt_disp_buffer);
    }

    {
        DistortionParams *params = &sp->distortion;

        params->wet_dry_ratio = RAND_RANGEF(0, 1);
        params->gain = RAND_RANGEF(DISTORTION_GAIN_MIN, DISTORTION_GAIN_MAX);
    }

    {
        DelayParams *params = &sp->delay;

        params->wet_dry_ratio = RAND_RANGEF(0, 1);
        params->feedback = RAND_RANGEF(0.0f, 0.45f);
        params->delay_s = RAND_RANGEF(DELAY_S_MIN, DELAY_S_MAX);
    }
}
