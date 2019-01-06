/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include <string>
#include "DaietContext.hpp"

using namespace std;

namespace daiet {
    int rx_loop(void*);
    int tx_loop(void*);

    int master(DaietContext* dctx);
    void port_init();
    void rings_init(string);
    void rings_cleanup(string);
    void signal_handler(int);
    void parse_parameters();
    void usage(const char*);
}
