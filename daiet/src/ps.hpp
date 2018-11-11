/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include <map>
#include "utils.hpp"
#include "params.hpp"

namespace daiet {

    void ps_setup();
    void ps_cleanup();
    int ps(void*);
}
