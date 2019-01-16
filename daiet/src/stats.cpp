/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "stats.hpp"
#include "utils.hpp"

namespace daiet {

    pkt_statistics pkt_stats;

    pkt_statistics::pkt_statistics() : total_w_tx(0), total_w_rx(0), total_ps_tx(0), total_ps_rx(0) {}

    void pkt_statistics::init(uint32_t nb_w, uint32_t nb_ps) {

        total_w_tx = 0;
        total_w_rx = 0;

        w_tx.resize(nb_w);
        w_rx.resize(nb_w);

        total_ps_tx = 0;
        total_ps_rx = 0;

        ps_tx.resize(nb_ps);
        ps_rx.resize(nb_ps);

#ifdef TIMERS
        w_timeouts.resize(nb_w);
#endif
    }

    void pkt_statistics::set_workers(uint16_t wid, uint64_t tx, uint64_t rx) {

        boost::unique_lock<boost::mutex> lock(w_mutex);

        w_tx[wid] = tx;
        w_rx[wid] = rx;

        total_w_tx += tx;
        total_w_rx += rx;
    }

    void pkt_statistics::set_ps(uint32_t psid, uint64_t tx, uint64_t rx) {

        boost::unique_lock<boost::mutex> lock(ps_mutex);

        ps_tx[psid] = tx;
        ps_rx[psid] = rx;

        total_ps_tx += tx;
        total_ps_rx += rx;
    }

#ifdef TIMERS
    void pkt_statistics::set_timeouts(uint32_t wid, uint64_t timeouts) {

        boost::unique_lock<boost::mutex> lock(timeouts_mutex);

        w_timeouts[wid] = timeouts;

        total_timeouts += timeouts;
    }
#endif

    void pkt_statistics::dump(){

#ifndef COLOCATED
            LOG_INFO("TX " + to_string(total_w_tx));
            LOG_INFO("RX " + to_string(total_w_rx));
#else
            LOG_INFO("Worker TX " + to_string(total_w_tx));
            LOG_INFO("Worker RX " + to_string(total_w_rx));
            LOG_INFO("PS TX " + to_string(total_ps_tx));
            LOG_INFO("PS RX " + to_string(total_ps_rx));
#endif
#ifdef TIMERS
            LOG_INFO("Timeouts " + to_string(w_timeouts));
#endif

            for (uint32_t i = 0; i < w_tx.size(); i++) {

                LOG_INFO("## Worker " + to_string(i));
                LOG_INFO("TX " + to_string(w_tx[i]));
                LOG_INFO("RX " + to_string(w_rx[i]));
            }

#ifdef COLOCATED
            for (uint32_t i = 0; i < ps_tx.size(); i++) {

                LOG_INFO("## PS" + to_string(i));
                LOG_INFO("TX " + to_string(ps_tx[i]));
                LOG_INFO("RX " + to_string(ps_rx[i]));
            }
#endif
    }
}
