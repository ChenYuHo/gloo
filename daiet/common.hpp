/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
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
#include <fstream>
#include <cstring>

#include "dpdk.h"
#include "PsMsgs.hpp"

using namespace std;

namespace daiet {
    // Statistics
    struct pkt_statistics {
            rte_atomic64_t w_tx;
            rte_atomic64_t w_rx;
            rte_atomic64_t faulty;

            rte_atomic64_t p_tx;
            rte_atomic64_t p_rx;

            pkt_statistics() {
                rte_atomic64_init(&w_tx);
                rte_atomic64_init(&w_rx);
                rte_atomic64_init(&p_tx);
                rte_atomic64_init(&p_rx);
                rte_atomic64_init(&faulty);
            }
    }__rte_cache_aligned;

    // | MsgType | Sqn# | Ack# | Size | Is_clock | Client_id | Version | Bg_clock | Entity ID |
    const size_t EXT_HEADER_SIZE = sizeof(uint8_t) ;

    // | #tables | Table_id | Update_size | #rows | Row_id | #updates | offset position |
    const size_t INT_HEADER_SIZE = sizeof(int32_t) + sizeof(uint8_t) + sizeof(uint16_t);

    // Sqn, offset position, updates are in Big Endian

    extern volatile bool force_quit;
    extern volatile bool worker_stop;
    extern volatile bool ps_stop;

    extern uint32_t core_to_workers_ids[];

    extern struct pkt_statistics pkt_stats;
}
