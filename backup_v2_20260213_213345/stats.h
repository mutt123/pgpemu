#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#include <stddef.h>

void stats_get_runtime();

void increment_caught(uint16_t conn_id);
void increment_fled(uint16_t conn_id);
void increment_spin(uint16_t conn_id);

typedef struct {
    uint16_t caught;
    uint16_t fled;
    uint16_t spin;
} Stats;

typedef struct {
    uint16_t conn_id;
    Stats stats;
} StatsForConn;

// NEW: Getter functions for web interface
size_t stats_get_count(void);
const StatsForConn* stats_get_entry(size_t index);

#endif /* STATS_H */
