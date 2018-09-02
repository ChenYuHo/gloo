/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "DaietContext.hpp"

namespace daiet {

    void* DaietContext::DaietMaster(void* parVoidPtr) {

        string corestr = "daiet -c daiet.cfg";

        vector<string> par_vec = split(corestr);
        int args_c = par_vec.size();
        char* args[args_c];
        char* args_ptr[args_c];

        for (vector<string>::size_type i = 0; i != par_vec.size(); i++) {
            args[i] = new char[par_vec[i].size() + 1];
            args_ptr[i] = args[i];
            strcpy(args[i], par_vec[i].c_str());
        }

        master(args_c, args);
        return NULL;
    }

    DaietContext::DaietContext() {

        /* Launch dpdk master thread */
        if (pthread_create(&masterThread, NULL, DaietMaster, NULL))
            GLOO_THROW("Error starting master dpdk thread");
    }

    DaietContext::~DaietContext() {

        int* ret;
        if (pthread_join(masterThread, (void**) &ret))
            GLOO_THROW("Error joining master dpdk thread");

        if (*ret < 0)
            GLOO_THROW("Master dpdk thread returned ", *ret);
    }
}
