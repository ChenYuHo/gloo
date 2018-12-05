/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include <string>
#include "DaietContext.hpp"

#define __DAIET_VERSION__ "0.1"

using namespace std;

namespace daiet {
    int rx_loop(void*);
    int tx_loop(void*);

#ifdef SAVE_LATENCIES
    void write_latencies(string, uint64_t);
#endif

    int master(int argc, char *argv[], BlockingQueue<TensorUpdate*> &in_queue, BlockingQueue<TensorUpdate*> &out_queue);
    void port_init();
    void rings_init(string);
    void rings_cleanup(string);
    void signal_handler(int);
    void parse_parameters(int, char *[]);
    void usage(const char*);
}
