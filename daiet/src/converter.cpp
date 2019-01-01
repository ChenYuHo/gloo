#include "converter.hpp"

#include "DaietContext.hpp"
#include "params.hpp"
#include "utils.hpp"
#include "common.hpp"
#include "dpdk.h"

namespace daiet {

    int converter(void* dctx_ptr) {

        DaietContext* dctx = (DaietContext*) dctx_ptr;

        unsigned socket_id = rte_socket_id();

        rte_atomic32_t* top_idx_ptr = &dpdk_data.top_index;
        rte_ring* converter_ring = dpdk_data.converter_ring_ptr;
        TensorUpdate* tuptr;

        uint32_t tensor_size = 0;
        uint32_t num_updates = daiet_par.getNumUpdates();
        uint32_t tsi = 0, final_tsi = 0;
        uint32_t* recv_tsi_ptr = 0;
        float* cur_float_ptr = NULL;
        uint32_t* cur_int_ptr = NULL;

        unsigned nb_rx = 0, j = 0;
        bool end_job = false;
        uint32_t** tsis_burst;
        tsis_burst = (uint32_t **) rte_malloc_socket(NULL, dpdk_par.burst_size_converter_read * sizeof(uint32_t*), RTE_CACHE_LINE_SIZE, socket_id);
        if (unlikely(tsis_burst == NULL))
            LOG_FATAL("Converter thread: cannot allocate tsis burst");

        while (!force_quit && !converter_stop) {

            tuptr = dctx->receive_conversion_job();
            if (tuptr != NULL) {

                tensor_size = tuptr->count;
                tsi = 0;
                end_job = false;

                if (tuptr->type == FLOAT) {

                    while (tsi < tensor_size) {

                        for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi++) {

                            cur_float_ptr = &(static_cast<float*>(tuptr->ptr)[tsi]);

                            *(static_cast<uint32_t*>((void*) cur_float_ptr)) = rte_cpu_to_be_32(round(*cur_float_ptr * daiet_par.getScalingFactor()));
                        }

                        rte_atomic32_set(top_idx_ptr, tsi);
                    }

                    while (!end_job && !force_quit && !converter_stop) {

                        // Read tsi from RX ring
                        nb_rx = rte_ring_dequeue_burst(converter_ring, (void **) tsis_burst, dpdk_par.burst_size_converter_read, NULL);

                        for (j = 0; j < nb_rx; j++) {

                            recv_tsi_ptr = tsis_burst[j];
                            tsis_burst[j] = NULL;

                            if (unlikely(recv_tsi_ptr == nullptr)) {
                                end_job = true;
                            } else {

                                for (final_tsi = (*recv_tsi_ptr) + num_updates; (*recv_tsi_ptr) < final_tsi; (*recv_tsi_ptr)++) {

                                    cur_int_ptr = &(static_cast<uint32_t*>(tuptr->ptr)[*recv_tsi_ptr]);

                                    *(static_cast<float*>((void*) cur_int_ptr)) = (float(rte_be_to_cpu_32(*cur_int_ptr))) / daiet_par.getScalingFactor();
                                }
                            }
                        }
                    }

                    rte_atomic32_set(top_idx_ptr, 0);

                } else if (tuptr->type == INT) {

                    while (tsi < tensor_size) {

                        for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi++) {

                            cur_int_ptr = &(static_cast<uint32_t*>(tuptr->ptr)[tsi]);

                            *cur_int_ptr = rte_cpu_to_be_32(*cur_int_ptr);
                        }

                        rte_atomic32_set(top_idx_ptr, tsi);
                    }

                    while (!end_job && !force_quit && !converter_stop) {

                        // Read tsi from RX ring
                        nb_rx = rte_ring_dequeue_burst(converter_ring, (void **) tsis_burst, dpdk_par.burst_size_converter_read, NULL);

                        for (j = 0; j < nb_rx; j++) {

                            recv_tsi_ptr = tsis_burst[j];
                            tsis_burst[j] = NULL;

                            if (unlikely(recv_tsi_ptr == nullptr)) {
                                end_job = true;
                            } else {

                                for (final_tsi = (*recv_tsi_ptr) + num_updates; (*recv_tsi_ptr) < final_tsi; (*recv_tsi_ptr)++) {
                                    cur_int_ptr = &(static_cast<uint32_t*>(tuptr->ptr)[*recv_tsi_ptr]);

                                    *cur_int_ptr = rte_be_to_cpu_32(*cur_int_ptr);
                                }
                            }
                        }
                    }

                    rte_atomic32_set(top_idx_ptr, 0);

                } else {
                    LOG_ERROR("Wrong update type");
                }
            }
        }

        // Cleanup
        rte_free(tsis_burst);

        return 0;
    }
}
