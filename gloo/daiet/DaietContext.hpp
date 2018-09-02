/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include <pthread.h>

#include "gloo/common/error.h"
#include "gloo/common/logging.h"

#include "daiet.hpp"

namespace daiet {

    class DaietContext {
     public:
      DaietContext();
      virtual ~DaietContext();

      void* DaietMaster(void*);

     private:
      pthread_t masterThread;
    };
}
