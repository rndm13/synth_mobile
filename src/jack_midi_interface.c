#include "jack_midi_interface.h"

#include <jack/thread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "log.h"

#define JACK_CLIENT_NAME                      "Synth"

#define CC_VALUE_SIGN_BIT_OFFSET              13

#define CC_HEADER                             0xB0
#define NOTE_OFF_HEADER                       0x80
#define NOTE_ON_HEADER                        0x90
#define CC_CHANNEL_MASK                       0x0F

void push_bytes(JACKMidi* jack, const uint8_t *bytes, size_t size);
void push_event(JACKMidi* jack);
#define PUSH_BYTES(X) push_bytes(jack, (uint8_t*)&(X), sizeof(X));

static uint16_t encode_i16(uint16_t val);
static void debug_dump_bytes(const uint8_t *bytes, size_t size);
static RingBufferElem jack_event_to_rbe(jack_midi_event_t* e);

static int process(jack_nframes_t nframes, void *arg) {
    static bool first_run = true;
    assert(arg != NULL);
    JACKMidi *jack_midi = (JACKMidi *)arg;

    if (first_run) {
        first_run = false;

        int exp_rt_prio = jack_client_max_real_time_priority(jack_midi->jack);
        int act_rt_prio = jack_client_real_time_priority(jack_midi->jack);

        if (act_rt_prio < 0) {
            log_error("JACK Client is not running with real time priority");
        } else if (act_rt_prio < exp_rt_prio) {
            // int err = jack_acquire_real_time_scheduling(0, exp_rt_prio);
            int err = 0;
            if (err < 0) {
                log_error("JACK Client failed to set real-time priority to %d", exp_rt_prio);
            } else {
                log_info("JACK Client successfully set real-time priority to %d", exp_rt_prio);
            }
        }
    }

    // ------------------
    //       INPUT
    // ------------------

    void *in_buf = jack_port_get_buffer(jack_midi->jack_port_in, nframes);
    if (in_buf == NULL) {
        log_error("Failed to get in port buffer");
        return 1;
    }

    size_t event_cnt = jack_midi_get_event_count(in_buf);
    for (size_t i = 0; i < event_cnt; i++) {
        jack_midi_event_t e;

        int result = jack_midi_event_get(&e, in_buf, i);
        if (result != 0) {
            log_error("Failed to read MIDI event %d", i);
            return 1;
        }

        if (e.size == 1 && (e.buffer[0] == 0xf8 || e.buffer[0] == 0xfe)) {
            // Ignore status messages

            continue;
        }

        RingBufferElem rbe = jack_event_to_rbe(&e);
        rb_push(&jack_midi->rb_recv, &rbe);
    }

    // // Do I need this?
    // jack_midi_clear_buffer(in_buf);

    // ------------------
    //       OUTPUT
    // ------------------

    void *out_buf = jack_port_get_buffer(jack_midi->jack_port_out, nframes);
    if (out_buf == NULL) {
        log_error("Failed to get out port buffer");
        return 1;
    }

    jack_midi_clear_buffer(out_buf);

    while (true) {
        RingBufferElem rbe = {0};
        rb_pop(&jack_midi->rb_send, &rbe);
        if (rbe.size <= 0) {
            break;
        }

        int result = jack_midi_event_write(out_buf, 0, rbe.data, rbe.size);

        if (result != 0) {
            log_error("Failed to write MIDI event");
            return 1;
        }

        log_info("Sent a SysEx message:");
        debug_dump_bytes(rbe.data, rbe.size);
    }

    return 0;
}

static void handle_received_note_event(JACKMidi *jack, const RingBufferElem* rbe) {
    VoiceMsg voice_update = {0};
    RingBufferElem rbe_voice_update = {0};

    if (JMS_STOPPED == jack->state) {
        return;
    }

    if (rbe->size < 3) {
        return;
    }

    if (!(NOTE_OFF_HEADER == rbe->data[0] || NOTE_ON_HEADER == rbe->data[0])) {
        return;
    }

    log_info("Received a MIDI note message");

    voice_update.released = false;
    if (NOTE_OFF_HEADER == rbe->data[0]) {
        voice_update.released = true;
    }

    voice_update.timestamp = 0; // TODO: add timestamps to all midi events
    voice_update.v.key_idx = rbe->data[1];
    voice_update.v.key_idx -= 20;
    voice_update.v.velocity = rbe->data[2];

    memcpy(rbe_voice_update.data, &voice_update, sizeof(voice_update));
    rbe_voice_update.size = sizeof(voice_update);

    rb_push(&jack->rb_voice_msg, &rbe_voice_update);
}

static void handle_received_event(JACKMidi *jack, const RingBufferElem* rbe) {
    if (JMS_STOPPED == jack->state) {
        return;
    }

    if (NOTE_OFF_HEADER == rbe->data[0] || NOTE_ON_HEADER == rbe->data[0]) {
        handle_received_note_event(jack, rbe);
    }
}

void jack_midi_process_events(JACKMidi *jack) {
    RingBufferElem rbe = {};
    while (true) {
        rb_pop(&jack->rb_recv, &rbe);
        if (rbe.size <= 0) {
            break;
        }

        handle_received_event(jack, &rbe);
    }
}

bool send_control_change(JACKMidi* jack, uint8_t param_id, uint8_t val) {
    assert(param_id < 128);
    assert(val < 128);

    uint8_t header = CC_HEADER;
    PUSH_BYTES(header);
    PUSH_BYTES(param_id);
    PUSH_BYTES(val);

    push_event(jack);

    return true;
}

bool send_voice_change(JACKMidi* jack, VoiceMsg msg) {
    uint8_t header = NOTE_ON_HEADER;

    if (msg.released) {
        header = NOTE_OFF_HEADER;
    }

    msg.v.key_idx += 20;

    PUSH_BYTES(header);
    PUSH_BYTES(msg.v.key_idx);
    PUSH_BYTES(msg.v.velocity);

    push_event(jack);

    return true;
}

void jack_midi_init(JACKMidi* this) {
    memset(this, 0, sizeof(*this));

    this->jack = jack_client_open(JACK_CLIENT_NAME, JackNoStartServer, NULL);

    if (this->jack == NULL) {
        log_error("Failed to open JACK client");
    }

    log_info("Successfully opened JACK client");

    int err = jack_set_process_callback(this->jack, process, this);
    if (err != 0) {
        log_error("Failed to set JACK process callback. Err: %d", err);
        goto err_client;
    }

    this->jack_port_in = jack_port_register(this->jack, "MIDI In", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    if (this->jack_port_in == NULL) {
        log_error("Failed to register JACK IN port");
        goto err_client;
    }

    this->jack_port_out = jack_port_register(this->jack, "MIDI Out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
    if (this->jack_port_out == NULL) {
        log_error("Failed to register JACK OUT port");
        goto err_port_in;
    }

    log_info("Successfully registered JACK port");

    err = jack_activate(this->jack);
    if (err != 0) {
        log_error("Failed to activate jack. Err: %d", err);
        goto err_port_out;
    }

    log_info("JACK MIDI controller successfully initialized");

    this->state = JMS_RUNNING;

    return;
err_port_out:
    jack_port_unregister(this->jack, this->jack_port_out);
err_port_in:
    jack_port_unregister(this->jack, this->jack_port_in);
err_client:
    jack_client_close(this->jack);
err:
    return;
}

void jack_midi_deinit(JACKMidi* this) {
    jack_port_unregister(this->jack, this->jack_port_out);
    jack_port_unregister(this->jack, this->jack_port_in);
    jack_client_close(this->jack);
    this->state = JMS_STOPPED;

    log_info("JACK MIDI controller successfully deinitialized");
}

void push_event(JACKMidi* this) {
    rb_push(&this->rb_send, &this->rbe_send_buffer);

    memset(this->rbe_send_buffer.data, 0, this->rbe_send_buffer.size);
    this->rbe_send_buffer.size = 0;
}

void push_bytes(JACKMidi* this, const uint8_t *bytes, size_t size) {
    memcpy(this->rbe_send_buffer.data + this->rbe_send_buffer.size, bytes, size);
    this->rbe_send_buffer.size += size;
}

static uint16_t encode_i16(uint16_t val) {
    bool sign_bit = val < 0;
    val |= sign_bit << CC_VALUE_SIGN_BIT_OFFSET;
    val = htole16(val);

    uint16_t result = 0;

    result |= 0x7F & val;
    result |= 0x7F00 & (val << 1);

    return result;
}

static void debug_dump_bytes(const uint8_t *bytes, size_t size) {
    char buffer[256] = {0};
    size_t buffer_len = 0;

    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) {
            log_debug(buffer);

            memset(buffer, 0, sizeof(buffer));
            buffer_len = 0;
        }

        buffer_len += snprintf(
                buffer + buffer_len, sizeof(buffer),
                "%02hhx ", bytes[i]);
    }

    log_debug(buffer);
}

static RingBufferElem jack_event_to_rbe(jack_midi_event_t* e) {
    RingBufferElem result;

    if (e->size > RB_ELEM_MAX_SIZE) {
        e->size = RB_ELEM_MAX_SIZE;
        log_error("Jack MIDI event is too large!");
    }

    result.size = e->size;
    memcpy(result.data, e->buffer, e->size);

    return result;
}
