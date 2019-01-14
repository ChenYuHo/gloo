/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include <vector>
#include <boost/thread.hpp>

#include "common.hpp"

using namespace std;

namespace daiet {

    class pkt_statistics {

        public:
            pkt_statistics();
            void init(uint32_t, uint32_t);
            void set_workers(uint32_t, uint64_t, uint64_t);
            void set_ps(uint32_t, uint64_t, uint64_t);
            void dump();

#ifdef TIMERS
            void set_timeouts(uint32_t, uint64_t);
#endif

        private:

            boost::mutex w_mutex;
            boost::mutex ps_mutex;
            uint64_t total_w_tx;
            uint64_t total_w_rx;
            vector<uint64_t> w_tx;
            vector<uint64_t> w_rx;

            uint64_t total_ps_tx;
            uint64_t total_ps_rx;
            vector<uint64_t>  ps_tx;
            vector<uint64_t>  ps_rx;

#ifdef TIMERS
            boost::mutex timeouts_mutex;
            vector<uint64_t> w_timeouts;
            uint64_t total_timeouts;
#endif
    };

    extern pkt_statistics pkt_stats;
}
