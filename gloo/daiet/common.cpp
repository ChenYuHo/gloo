#include "common.hpp"

/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

namespace daiet {

    volatile bool force_quit;
    volatile bool worker_stop;
    volatile bool ps_stop;

    struct pkt_statistics pkt_stats;
}
