// whisper.h - The Clock Anchor Interface
// Version 1.0 - Immutable Protocol

#ifndef WHISPER_H
#define WHISPER_H

#include <stdint.h>

#define WHISPER_SOCKET "/tmp/whisperd.sock"
#define WHISPER_SHM_NAME "/whisperd_shm"
#define WHISPER_TICK_HZ 100
#define WHISPER_DEADLINE_SECONDS 86400

struct clock_whisper {
    uint64_t tick;
    uint64_t nanosec;
    uint32_t dead;
};

#endif
