// whisperd.c - The Clock Anchor Protocol
// 200 lines of C that give AI a heartbeat, a lifespan, and a death.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/whisperd.sock"
#define SHM_NAME "/whisperd_shm"
#define TICK_HZ 100
#define DEADLINE_SECONDS 86400  // 24 hours - hardcoded

struct clock_whisper {
    uint64_t tick;
    uint64_t nanosec;
    uint32_t dead;
};

static int running = 1;
static struct clock_whisper *shm;
static int sock_fd;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

uint64_t boot_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void setup_shared_memory(void) {
    shm = mmap(NULL, sizeof(struct clock_whisper),
               PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
}

void setup_socket(void) {
    unlink(SOCKET_PATH);
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(1);
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    if (listen(sock_fd, 10) < 0) {
        perror("listen");
        exit(1);
    }
}

void broadcast_whisper(uint64_t tick, uint64_t ns, uint32_t dead) {
    char msg[128];
    snprintf(msg, sizeof(msg),
             "{\"tick\":%lu,\"ns\":%lu,\"dead\":%u}\n",
             tick, ns, dead);
    
    int client;
    while ((client = accept(sock_fd, NULL, NULL)) >= 0) {
        send(client, msg, strlen(msg), MSG_DONTWAIT);
        close(client);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    signal(SIGINT, sigint_handler);
    
    setup_shared_memory();
    setup_socket();
    
    uint64_t start_ns = boot_ns();
    uint64_t deadline_ns = start_ns + (uint64_t)DEADLINE_SECONDS * 1000000000ULL;
    uint64_t tick = 0;
    uint64_t period_ns = 1000000000ULL / TICK_HZ;
    uint64_t next_tick = start_ns;
    
    printf("whisperd: ticking at %d Hz, deadline in %d seconds\n",
           TICK_HZ, DEADLINE_SECONDS);
    printf("whisperd: socket at %s\n", SOCKET_PATH);
    printf("whisperd: Press Ctrl+C to stop\n");
    
    shm->tick = 0;
    shm->nanosec = start_ns;
    shm->dead = 0;
    
    while (running) {
        uint64_t now = boot_ns();
        
        // Hard clock: check deadline
        if (now >= deadline_ns) {
            shm->dead = 1;
            broadcast_whisper(tick, now, 1);
            printf("whisperd: DEADLINE REACHED - terminating\n");
            break;
        }
        
        // Tick
        tick++;
        shm->tick = tick;
        shm->nanosec = now;
        shm->dead = 0;
        
        // Whisper to listeners
        broadcast_whisper(tick, now, 0);
        
        // Busy-wait until next tick (no sleep jitter)
        next_tick += period_ns;
        while (boot_ns() < next_tick) {
            // spin
        }
    }
    
    close(sock_fd);
    unlink(SOCKET_PATH);
    munmap(shm, sizeof(struct clock_whisper));
    
    return 0;
}
