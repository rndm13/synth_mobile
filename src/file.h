#pragma once

#include "synth.h"

typedef struct ProgramSelection {
    char   filepath_arr[PROGRAM_COUNT_MAX][FILEPATH_CAPACITY];
    char   program_name_arr[PROGRAM_COUNT_MAX][PROGRAM_NAME_CAPACITY];
    size_t program_count;
    int    selected_program_idx;
} ProgramSelection;

int save_program(Synth *s, ProgramSelection* ps);
int open_program(Synth *s, const ProgramSelection* ps);
int init_program_selection(const char* dirpath, ProgramSelection* ps);
void randomize_program(Synth *s);
