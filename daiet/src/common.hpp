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
            uint64_t w_tx;
            uint64_t w_rx;

#ifdef TIMERS
            uint64_t w_timeouts;
#endif

            uint64_t p_tx;
            uint64_t p_rx;

            pkt_statistics() {
                w_tx = 0;
                w_rx = 0;
#ifdef TIMERS
                w_timeouts = 0;
#endif
                p_tx = 0;
                p_rx = 0;
            }
    }__rte_cache_aligned;

    extern volatile bool force_quit;
    extern volatile bool ps_stop;
    extern volatile bool converter_stop;

    extern uint32_t core_to_workers_ids[];

    extern struct pkt_statistics pkt_stats;
}
