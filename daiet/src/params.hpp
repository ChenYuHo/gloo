/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include "utils.hpp"

using namespace std;

namespace daiet {

    struct dpdk_data {
            // CPU cores
            uint32_t core_rx;
            uint32_t core_tx;

            // Rings
            struct rte_ring *w_ring_rx;
            struct rte_ring *w_ring_tx;
            struct rte_ring *p_ring_rx;
            struct rte_ring *p_ring_tx;

            // Buffer pool size
            uint32_t pool_buffer_size;

            dpdk_data() {
                // Defaults

                // CPU cores
                core_rx = 0;
                core_tx = 0;

                // Rings
                w_ring_rx = NULL;
                w_ring_tx = NULL;
                p_ring_rx = NULL;
                p_ring_tx = NULL;

                pool_buffer_size = RTE_MBUF_DEFAULT_BUF_SIZE;
            }
    }__rte_cache_aligned;

    struct dpdk_params {

            // Ports
            uint16_t portid;
            uint16_t port_rx_ring_size;
            uint16_t port_tx_ring_size;

            // Rings
            uint32_t ring_rx_size;
            uint32_t ring_tx_size;

            // Buffer pool
            uint32_t pool_size;
            uint32_t pool_cache_size;

            // Burst sizes
            uint32_t burst_size_rx_read;
            uint32_t burst_size_worker;
            uint32_t burst_size_tx_read;
            uint32_t burst_drain_tx_us;

            dpdk_params() {
                // Defaults

                // Ports
                portid = 0;
                port_rx_ring_size = 128;
                port_tx_ring_size = 512;

                // Rings
                ring_rx_size = 65536;
                ring_tx_size = 65536;

                // Buffer pool
                pool_size = 8192 * 32;
                pool_cache_size = 256 * 2;

                // Burst sizes
                burst_size_rx_read = 64;
                burst_size_worker = 64;
                burst_size_tx_read = 64;
                burst_drain_tx_us = 100;
            }
    }__rte_cache_aligned;

    void print_dpdk_params();

    class daiet_params {
        private:

            string mode;
            uint32_t num_workers;

            uint8_t num_updates;

            uint32_t max_num_pending_messages;

            uint64_t tx_flags;

            float scaling_factor;

            uint16_t worker_port_be;
            uint16_t ps_port_be;
            uint32_t worker_ip_be;

            uint32_t* ps_ips_be;

            uint64_t* ps_macs_be;

            uint32_t num_ps;

        public:
            daiet_params();
            ~daiet_params();

            void print_params();

            string& getMode();
            uint32_t& getNumWorkers();

            __rte_always_inline uint8_t getNumUpdates() const {
                return num_updates;
            }

            __rte_always_inline uint32_t& getMaxNumPendingMessages() {
                return max_num_pending_messages;
            }

            void setNumUpdates(uint8_t);

            void setMaxFloat(float);

            __rte_always_inline float getScalingFactor() const {
                return scaling_factor;
            }

            __rte_always_inline int64_t getTxFlags() const {
                return tx_flags;
            }

            __rte_always_inline uint16_t getWorkerPortBe() const {
                return worker_port_be;
            }

            void setWorkerPort(uint16_t workerPort);

            __rte_always_inline uint16_t getPsPortBe() const {
                return ps_port_be;
            }

            void setPsPort(uint16_t);

            /*
             * Returns false if the IP is invalid
             */
            bool setWorkerIp(string);

            __rte_always_inline uint32_t getWorkerIpBe() {
                return worker_ip_be;
            }

            __rte_always_inline const uint32_t* getPsIpsBe() {
                return ps_ips_be;
            }

            __rte_always_inline const uint64_t* getPsMacsBe() {
                return ps_macs_be;
            }

            __rte_always_inline uint32_t getPsIpBe(int i) {
                return ps_ips_be[i % num_ps];
            }

            __rte_always_inline uint64_t getPsMacBe(int i) {
                return ps_macs_be[i % num_ps];
            }

            bool setPs(string, string);

            __rte_always_inline uint32_t getNumPs() const {
                return num_ps;
            }
    };

    extern struct dpdk_data dpdk_data;
    extern struct dpdk_params dpdk_par;
    extern daiet_params daiet_par;
}
