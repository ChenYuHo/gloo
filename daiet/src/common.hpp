/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>
#include <cstring>
#include <fstream>

#include "dpdk.h"
#include "msgs.h"

using namespace std;

namespace daiet {
    // Statistics
    struct pkt_statistics {
            rte_atomic64_t w_tx;
            rte_atomic64_t w_rx;
            rte_atomic64_t w_timeouts;

            rte_atomic64_t p_tx;
            rte_atomic64_t p_rx;

            pkt_statistics() {
                rte_atomic64_init(&w_tx);
                rte_atomic64_init(&w_rx);
                rte_atomic64_init(&p_tx);
                rte_atomic64_init(&p_rx);
            }
    }__rte_cache_aligned;

    extern volatile bool force_quit;
    extern volatile bool ps_stop;

    extern uint32_t core_to_workers_ids[];

    extern struct pkt_statistics pkt_stats;
}
