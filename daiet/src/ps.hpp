/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include "utils.hpp"
#include "params.hpp"

namespace daiet {

    struct mac_ip_pair {
        struct ether_addr mac;
        uint32_t be_ip;
    };

    void ps_setup();
    void ps_cleanup();
    int ps(void*);
}
