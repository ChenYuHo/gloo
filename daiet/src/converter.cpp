#include "converter.hpp"

#include "DaietContext.hpp"
#include "params.hpp"
#include "utils.hpp"
#include "common.hpp"
#include "dpdk.h"

#if defined ( __AVX512F__ ) || defined ( __AVX512__ )
#define MAX_VECTOR_SIZE 512
#endif
#include "vcl/vectorclass.h"

namespace daiet {

    int converter(void* dctx_ptr) {

        DaietContext* dctx = (DaietContext*) dctx_ptr;

        unsigned socket_id = rte_socket_id();

        rte_atomic32_t* top_idx_ptr = &dpdk_data.top_index;
        rte_ring* converter_ring = dpdk_data.converter_ring_ptr;
        TensorUpdate* tuptr;

        uint32_t tensor_size = 0, last_entries = 0;
        const uint32_t num_updates = daiet_par.getNumUpdates();
        const float scalingfactor = daiet_par.getScalingFactor();

#if MAX_VECTOR_SIZE >= 512
        Vec16f vec_f;
        Vec16i vec_i;
        const Vec16f scalingfactor_vec(scalingfactor);
#else
        Vec8f vec_f;
        Vec8i vec_i;
        const Vec8f scalingfactor_vec(scalingfactor);
#endif

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

                last_entries = tensor_size % num_updates;
                if (last_entries!=0)
                    tensor_size -= last_entries;

                if (tuptr->type == FLOAT) {

                    // First pass
                    while (tsi < tensor_size) {

                        cur_float_ptr = &(static_cast<float*>(tuptr->ptr)[tsi]);

#if MAX_VECTOR_SIZE >= 512
                        for (uint32_t i = 0; i < num_updates; i += 16) {
#else
                        for (uint32_t i = 0; i < num_updates; i += 8) {
#endif
                            vec_f.load(cur_float_ptr + i);
                            round_to_int(vec_f * scalingfactor_vec).store(cur_float_ptr + i);
                        }

                        for (uint32_t i = 0; i < num_updates; i++) {

                            cur_int_ptr = static_cast<uint32_t*>((void*) (cur_float_ptr + i));
                            *(cur_int_ptr) = rte_cpu_to_be_32(*(cur_int_ptr));
                        }

                        tsi += num_updates;

                        rte_atomic32_set(top_idx_ptr, tsi);
                    }

                    // Last packet
                    cur_float_ptr = &(static_cast<float*>(tuptr->ptr)[tsi]);

                    for (uint32_t i = 0; i < last_entries; i++) {

                        cur_int_ptr = static_cast<uint32_t*>((void*) (cur_float_ptr + i));
                        *(cur_int_ptr) = rte_cpu_to_be_32(round(*(cur_float_ptr + i) * scalingfactor));
                    }

                    rte_atomic32_set(top_idx_ptr, tsi+num_updates);

                    // Second pass
                    while (!end_job && !force_quit && !converter_stop) {

                        // Read tsi from RX ring
                        nb_rx = rte_ring_dequeue_burst(converter_ring, (void **) tsis_burst, dpdk_par.burst_size_converter_read, NULL);

                        for (j = 0; j < nb_rx; j++) {

                            recv_tsi_ptr = tsis_burst[j];
                            tsis_burst[j] = NULL;

                            if (unlikely(recv_tsi_ptr == nullptr)) {
                                end_job = true;
                            } else {

                                /* This SIMD code does not improve performance */
                                /*
                                cur_int_ptr = &(static_cast<uint32_t*>(tuptr->ptr)[*recv_tsi_ptr]);

                                for (uint32_t i = 0; i < num_updates; i++) {

                                    *(cur_int_ptr + i) = rte_be_to_cpu_32(*(cur_int_ptr + i));
                                }

#if MAX_VECTOR_SIZE >= 512
                                for (uint32_t i = 0; i < num_updates; i += 16) {
#else
                                for (uint32_t i = 0; i < num_updates; i += 8) {
#endif
                                    vec_i.load(cur_int_ptr + i);
                                    vec_f = to_float(vec_i) / scalingfactor_vec;
                                    vec_f.store(static_cast<float*>((void*) (cur_int_ptr + i)));
                                }
                                */

                                if (likely(*recv_tsi_ptr!=tensor_size)) {
                                    for (final_tsi = (*recv_tsi_ptr) + num_updates; (*recv_tsi_ptr) < final_tsi; (*recv_tsi_ptr)++) {

                                        cur_int_ptr = &(static_cast<uint32_t*>(tuptr->ptr)[*recv_tsi_ptr]);

                                        *(static_cast<float*>((void*) cur_int_ptr)) = (float(rte_be_to_cpu_32(*cur_int_ptr))) / scalingfactor;
                                    }
                                } else {
                                    for (final_tsi = (*recv_tsi_ptr) + last_entries; (*recv_tsi_ptr) < final_tsi; (*recv_tsi_ptr)++) {

                                        cur_int_ptr = &(static_cast<uint32_t*>(tuptr->ptr)[*recv_tsi_ptr]);

                                        *(static_cast<float*>((void*) cur_int_ptr)) = (float(rte_be_to_cpu_32(*cur_int_ptr))) / scalingfactor;
                                    }
                                }

                            }
                        }
                    }

                    rte_atomic32_set(top_idx_ptr, 0);

                } else if (tuptr->type == INT) {

                    // First pass
                    while (tsi < tensor_size) {

                        for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi++) {

                            cur_int_ptr = &(static_cast<uint32_t*>(tuptr->ptr)[tsi]);

                            *cur_int_ptr = rte_cpu_to_be_32(*cur_int_ptr);
                        }

                        rte_atomic32_set(top_idx_ptr, tsi);
                    }

                    // Last packet
                    for (final_tsi = tsi + last_entries; tsi < final_tsi; tsi++) {

                        cur_int_ptr = &(static_cast<uint32_t*>(tuptr->ptr)[tsi]);

                        *cur_int_ptr = rte_cpu_to_be_32(*cur_int_ptr);
                    }

                    rte_atomic32_set(top_idx_ptr, tensor_size+num_updates);

                    // Second pass
                    while (!end_job && !force_quit && !converter_stop) {

                        // Read tsi from RX ring
                        nb_rx = rte_ring_dequeue_burst(converter_ring, (void **) tsis_burst, dpdk_par.burst_size_converter_read, NULL);

                        for (j = 0; j < nb_rx; j++) {

                            recv_tsi_ptr = tsis_burst[j];
                            tsis_burst[j] = NULL;

                            if (unlikely(recv_tsi_ptr == nullptr)) {
                                end_job = true;
                            } else {

                                if (likely(*recv_tsi_ptr!=tensor_size)) {
                                    for (final_tsi = (*recv_tsi_ptr) + num_updates; (*recv_tsi_ptr) < final_tsi; (*recv_tsi_ptr)++) {
                                        cur_int_ptr = &(static_cast<uint32_t*>(tuptr->ptr)[*recv_tsi_ptr]);

                                        *cur_int_ptr = rte_be_to_cpu_32(*cur_int_ptr);
                                    }
                                } else {
                                    for (final_tsi = (*recv_tsi_ptr) + last_entries; (*recv_tsi_ptr) < final_tsi; (*recv_tsi_ptr)++) {
                                        cur_int_ptr = &(static_cast<uint32_t*>(tuptr->ptr)[*recv_tsi_ptr]);

                                        *cur_int_ptr = rte_be_to_cpu_32(*cur_int_ptr);
                                    }
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
