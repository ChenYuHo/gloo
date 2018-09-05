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

            // Buffer pool
            struct rte_mempool *pool;
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

                // Buffer pool
                pool = NULL;
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
            //uint32_t burst_size_rx_write;
            uint32_t burst_size_worker;
            uint32_t burst_size_tx_read;
            uint32_t burst_size_tx_write;

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
                //burst_size_rx_write=64;
                burst_size_worker = 64;
                burst_size_tx_read = 64;
                burst_size_tx_write = 64;
            }
    }__rte_cache_aligned;

    void print_dpdk_params();

    class daiet_params {
        private:
            const size_t update_size = sizeof(int32_t);;
            const uint32_t max_seq_num = ~(1 << (sizeof(uint32_t) * 8 - 1));

            string mode;

            uint8_t num_updates;
            uint payload_size;

            uint max_num_pending_messages;
            uint num_workers;
            uint max_num_msgs;

            int32_t cell_value;
            int32_t cell_value_be;

            float scaling_factor;

            uint16_t worker_port_be;
            uint16_t ps_port_be;
            uint32_t worker_ip_be;

            uint32_t* ps_ips_be;

            uint64_t* ps_macs_be;

            uint num_ps;

        public:
            daiet_params();
            ~daiet_params();

            void print_params();

            __rte_always_inline const size_t getUpdateSize() const {
                return update_size;
            }

            string& getMode();

            const uint32_t getMaxSeqNum() const;

            __rte_always_inline uint8_t getNumUpdates() const {
                return num_updates;
            }
            void setNumUpdates(uint8_t);

            __rte_always_inline uint getPayloadSize() const {
                return payload_size;
            }

            uint& getMaxNumPendingMessages();

            uint& getNumWorkers();

            uint& getMaxNumMsgs();

            int32_t getCellValue() const;
            void setCellValue(int32_t);

            float getScalingFactor() const;
            void setMaxFloat(float);

            __rte_always_inline int32_t getCellValueBe() const {
                return cell_value_be;
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

            __rte_always_inline uint32_t getWorkerIpBe() const {
                return worker_ip_be;
            }

            __rte_always_inline const uint32_t* getPsIpsBe() const {
                return ps_ips_be;
            }

            __rte_always_inline const uint64_t* getPsMacsBe() const {
                return ps_macs_be;
            }

            __rte_always_inline const uint32_t getPsIpBe(int i) const {
                return ps_ips_be[i % num_ps];
            }

            __rte_always_inline const uint64_t getPsMacBe(int i) const {
                return ps_macs_be[i % num_ps];
            }

            bool setPs(string, string);

            __rte_always_inline uint getNumPs() const {
                return num_ps;
            }
    };

    extern struct dpdk_data dpdk_data;
    extern struct dpdk_params dpdk_par;
    extern daiet_params daiet_par;
}
