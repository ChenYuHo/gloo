/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include "utils.hpp"
#include "params.hpp"

namespace daiet {

    extern rte_atomic32_t* sent_message_counters;

#if SAVE_LATENCIES
    extern uint64_t* latencies;
#endif

    void worker_setup();
    void worker_cleanup();
    int worker(BlockingQueue<TensorUpdate*> &in_queue, BlockingQueue<TensorUpdate*> &out_queue);
}
