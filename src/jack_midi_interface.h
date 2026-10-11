#pragma once

/* TODO: Wrap this yourself and dynamically link to JACK */
#include <jack/jack.h>
#include <jack/midiport.h>

#include <assert.h>

#include "ring_buffer.h"
#include "voice.h"

typedef enum JACKMidiState {
    JMS_STOPPED = 0,
    JMS_RUNNING = 1,
} JACKMidiState;

typedef struct VoiceMsg {
    float timestamp;
    bool released;
    Voice v;
} VoiceMsg;

typedef struct JACKMidi {
    jack_client_t *jack;
    jack_port_t *jack_port_in;
    jack_port_t *jack_port_out;

    JACKMidiState state;

    RingBuffer rb_recv;
    RingBuffer rb_send;
    RingBufferElem rbe_send_buffer;

    RingBuffer rb_voice_msg;
} JACKMidi;

void jack_midi_init(JACKMidi* jack);
void jack_midi_deinit(JACKMidi* jack);
void jack_midi_process_events(JACKMidi *jack);
