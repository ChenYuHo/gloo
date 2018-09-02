/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include <signal.h>


#include "dpdk.h"
#include "common.hpp"
#include "utils.hpp"
#include "params.hpp"
#include "worker.hpp"
#include "ps.hpp"

#define __DAIET_VERSION__ "0.1"

using namespace std;

namespace daiet {
    int rx_loop(void*);
    int tx_loop(void*);

#if SAVE_LATENCIES
    void write_latencies(string, uint64_t);
#endif

    int master(int, char *[]);
    void mbuf_pool_init();
    void port_init();
    void rings_init(string);
    void rings_cleanup(string);
    void signal_handler(int);
    void usage(const char*);
}
