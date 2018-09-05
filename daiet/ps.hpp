/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include <map>
#include "dpdk.h"
#include "common.hpp"
#include "utils.hpp"
#include "params.hpp"
#include "worker.hpp"

namespace daiet {
    void ps_msg_setup(ClientSendOpLogMsg*, uint16_t, uint32_t);
    bool ps_aggregate_message(ClientSendOpLogMsg*, uint32_t, struct ether_addr, uint16_t&);
    void ps_setup();
    void ps_cleanup();
    int ps(void*);
}
