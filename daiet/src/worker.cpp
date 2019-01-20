/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "worker.hpp"
#include "DaietContext.hpp"
#include "utils.hpp"
#include "params.hpp"
#include "stats.hpp"

#ifdef TIMERS
#include <vector>
#ifdef TIMESTAMPS
#include <utility>
#endif
#endif

#if defined ( __AVX512F__ ) || defined ( __AVX512__ )
#define MAX_VECTOR_SIZE 512
#endif
#include "vcl/vectorclass.h"

using namespace std;

namespace daiet {

    thread_local static unsigned lcore_id;
    thread_local static uint16_t worker_id;
    thread_local static uint16_t worker_port_be, ps_port_be;
    thread_local static TensorUpdate tu;
    thread_local static uint32_t tensor_size;
    thread_local static uint32_t num_updates;
    thread_local static size_t entries_size;
    thread_local static uint16_t start_pool_index;
    thread_local static uint16_t shift;

    thread_local static float scalingfactor;

#if MAX_VECTOR_SIZE >= 512
    thread_local static Vec16f vec_f;
    thread_local static Vec16i vec_i;
    thread_local static Vec16f scalingfactor_vec;
#else
    thread_local static Vec8f vec_f;
    thread_local static Vec8i vec_i;
    thread_local static Vec8f scalingfactor_vec;
#endif

    thread_local static void (*fill_fn)(entry_hdr*, uint32_t, uint32_t);
    thread_local static void (*store_fn)(daiet_hdr*, uint32_t);

#ifdef TIMERS
    thread_local static struct rte_mempool *pool;

    thread_local static uint64_t timer_cycles = (rte_get_timer_hz() / 1000) * daiet_par.getTimeout(); // cycles for 1 ms
    thread_local static uint64_t w_timeouts = 0;

#ifdef TIMESTAMPS
    thread_local static vector<pair<uint32_t,uint64_t>> resent_pkt_timestamps;
#endif
#endif

#ifdef LATENCIES

    __rte_always_inline void write_timestamp(uint64_t* sent_timestamps, uint16_t offset) {

        sent_timestamps[offset] = rte_get_timer_cycles();
    }

    __rte_always_inline void save_latency(uint64_t* latencies, uint64_t* sent_timestamps, uint16_t offset, uint64_t num_recv) {

        uint64_t ts = rte_get_timer_cycles();
        latencies[num_recv] = ts - sent_timestamps[offset];
        sent_timestamps[offset] = ts;
    }

    void dump_latencies(uint64_t* latencies, uint32_t total_num_msgs, string file_name) {

        LOG_INFO("Writing latency file...");

        uint64_t hz = rte_get_timer_hz();

        ofstream latency_file(file_name);

        if (latency_file.is_open()) {
            for (uint32_t i = 0; i < total_num_msgs && !force_quit; i++) {
                latency_file << ((double) (latencies[i])) * 1000000 / hz << endl;
            }

            latency_file.close();
        } else {
            LOG_ERROR("Unable to open latency file");
        }
    }
#endif

#ifdef TIMESTAMPS
    __rte_always_inline void write_global_timestamp(vector<pair<uint32_t,uint64_t>> &global_sent_timestamps, uint32_t pool_index_monoset) {

        pair<uint32_t,uint64_t> ts;
        ts.first = pool_index_monoset;
        ts.second = rte_get_timer_cycles();
        global_sent_timestamps.push_back(ts);

        if (unlikely(first_global_ts=0))
            first_global_ts = ts.second;
    }

    uint64_t dump_timestamps(vector<pair<uint32_t,uint64_t>> &global_sent_timestamps, string file_name) {

        LOG_INFO("Writing timestamps file...");

        uint64_t hz = rte_get_timer_hz();

        ofstream timestamps_file(file_name);

        if (timestamps_file.is_open()) {

            uint64_t base_ts = first_global_ts;
            first_global_ts = 0;

            for (vector<pair<uint32_t,uint64_t>>::iterator it=global_sent_timestamps.begin(); it!=global_sent_timestamps.end() && !force_quit; ++it) {

                timestamps_file << to_string(it->first) + " " + to_string(((double) (it->second - base_ts)) * 1000000 / hz) << endl;
            }

            timestamps_file.close();

            return base_ts;
        } else {
            LOG_ERROR("Unable to open timestamps file");
            return 0;
        }
    }

#ifdef TIMERS
    void dump_resent_timestamps(uint64_t base_ts, string file_name) {

        LOG_INFO("Writing resent timestamps file...");

        uint64_t hz = rte_get_timer_hz();

        ofstream resent_timestamps_file(file_name);

        if (resent_timestamps_file.is_open()) {

            for (vector<pair<uint32_t,uint64_t>>::iterator i = resent_pkt_timestamps.begin();
                    i != resent_pkt_timestamps.end() && !force_quit; i++) {
                resent_timestamps_file << to_string(i->first) + " " +to_string(((double) (i->second-base_ts)) * 1000000 / hz) << endl;
            }

            resent_timestamps_file.close();
        } else {
            LOG_ERROR("Unable to open resent timestamps file");
        }
    }
#endif
#endif

    __rte_always_inline uint16_t tsi_to_pool_index(const uint32_t& tsi) {

        uint32_t i = ((tsi / num_updates) + shift) % (2 * daiet_par.getMaxNumPendingMessages());
        if (i < daiet_par.getMaxNumPendingMessages())
            // Set 0
            return (start_pool_index + i);
        else
            // Set 1
            return ((start_pool_index + (i - daiet_par.getMaxNumPendingMessages())) | 0x8000);
    }

    __rte_always_inline void store_int32(daiet_hdr* daiet, uint32_t tensor_size) {

        uint32_t tsi = daiet->tsi;
        uint32_t final_tsi;
        struct entry_hdr * entry = (struct entry_hdr *) (daiet + 1);

        if (likely(tsi + num_updates <= tensor_size)) {

            //rte_memcpy(&(static_cast<uint32_t*>(tu.ptr)[tsi]), entry, entries_size);

            for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi++, entry++) {

                static_cast<uint32_t*>(tu.ptr)[tsi] = rte_be_to_cpu_32(entry->upd);
            }

        } else {

            //rte_memcpy(&(static_cast<uint32_t*>(tu.ptr)[tsi]), entry, sizeof(struct entry_hdr) * (tensor_size - tsi));

            for (final_tsi = tsi + tensor_size - tsi; tsi < final_tsi; tsi++, entry++) {

                static_cast<uint32_t*>(tu.ptr)[tsi] = rte_be_to_cpu_32(entry->upd);
            }
        }

    }

    __rte_always_inline void store_float32(daiet_hdr* daiet, uint32_t tensor_size) {

        uint32_t tsi = daiet->tsi;
        uint32_t final_tsi;
        struct entry_hdr * entry = (struct entry_hdr *) (daiet + 1);

        if (likely(tsi + num_updates <= tensor_size)) {

            /* This SIMD code does not improve performance */
            /*

             for (i = 0; i < num_updates; i++) {

                 (entry+i)->upd = rte_be_to_cpu_32((entry+i)->upd);
             }

             #if MAX_VECTOR_SIZE >= 512
             for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi += 16, entry += 16) {
             #else
             for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi += 8, entry += 8) {
             #endif
             vec_i.load(entry);
             vec_f = to_float(vec_i) / scalingfactor_vec;
             vec_f.store(&(static_cast<float*>(tu.ptr)[tsi]));
             }
             */

            for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi++, entry++) {

                static_cast<float*>(tu.ptr)[tsi] = (float(rte_be_to_cpu_32(entry->upd))) / scalingfactor;
            }

        } else {

            for (final_tsi = tsi + tensor_size - tsi; tsi < final_tsi; tsi++, entry++) {

                static_cast<float*>(tu.ptr)[tsi] = (float(rte_be_to_cpu_32(entry->upd))) / scalingfactor;
            }
        }

    }

    __rte_always_inline void fill_int32(struct entry_hdr *entry, uint32_t tsi, uint32_t tensor_size) {

        uint32_t final_tsi;

        if (likely(tsi + num_updates <= tensor_size)) {

            // rte_memcpy(entry, &(static_cast<uint32_t*>(tu.ptr)[tsi]), entries_size);
            for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi++, entry++) {
                entry->upd = rte_cpu_to_be_32((static_cast<uint32_t*>(tu.ptr)[tsi]));
            }

        } else {
            //Padding
            uint32_t num_valid = tensor_size - tsi;
            uint32_t zeros = num_updates - num_valid;

            //rte_memcpy(entry, &(static_cast<uint32_t*>(tu.ptr)[tsi]), sizeof(struct entry_hdr) * num_valid);
            for (final_tsi = tsi + num_valid; tsi < final_tsi; tsi++, entry++) {
                entry->upd = rte_cpu_to_be_32((static_cast<uint32_t*>(tu.ptr)[tsi]));
            }

            memset(entry + num_valid, 0, sizeof(struct entry_hdr) * zeros);
        }
    }

    __rte_always_inline void fill_float32(struct entry_hdr *entry, uint32_t tsi, uint32_t tensor_size) {

        float* cur_float_ptr;
        uint32_t* cur_int_ptr;
        uint32_t* final_ptr;

        cur_float_ptr = &(static_cast<float*>(tu.ptr)[tsi]);
        cur_int_ptr = static_cast<uint32_t*>((void*) cur_float_ptr);

        if (likely(tsi + num_updates <= tensor_size)) {

#if MAX_VECTOR_SIZE >= 512
            for (uint32_t i = 0; i < num_updates; i += 16) {
#else
            for (uint32_t i = 0; i < num_updates; i += 8) {
#endif
                vec_f.load(cur_float_ptr + i);
                round_to_int(vec_f * scalingfactor_vec).store(cur_float_ptr + i);
            }

            for (final_ptr = cur_int_ptr + num_updates; cur_int_ptr < final_ptr; cur_int_ptr++, entry++) {

                entry->upd = rte_cpu_to_be_32(*(cur_int_ptr));
            }

        } else {
            //Padding
            uint32_t num_valid = tensor_size - tsi;
            uint32_t zeros = num_updates - num_valid;

            for (final_ptr = cur_int_ptr + num_valid;
                    cur_int_ptr < final_ptr;
                    cur_int_ptr++, cur_float_ptr++, entry++) {

                entry->upd = rte_cpu_to_be_32(round(*(cur_float_ptr) * scalingfactor));
            }

            memset(entry + num_valid, 0, sizeof(struct entry_hdr) * zeros);
        }
    }

    __rte_always_inline void reset_pkt(struct ether_hdr * eth, unsigned portid, uint32_t tsi, uint32_t tensor_size, uint64_t ol_flags) {

        struct ipv4_hdr * const ip = (struct ipv4_hdr *) (eth + 1);
        struct udp_hdr * const udp = (struct udp_hdr *) (ip + 1);
        struct daiet_hdr * const daiet = (struct daiet_hdr *) (udp + 1);
        struct entry_hdr * entry = (struct entry_hdr *) (daiet + 1);

        // Set MACs
        ether_addr_copy(&(eth->s_addr), &(eth->d_addr));
        rte_eth_macaddr_get(portid, &eth->s_addr);

        // Set IPs
        ip->hdr_checksum = 0;
        ip->dst_addr = ip->src_addr;
        ip->src_addr = daiet_par.getWorkerIpBe();

        // Set UDP
        udp->src_port = worker_port_be;
        udp->dst_port = ps_port_be;
        udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, ol_flags);

        // DAIET header
        daiet->tsi = tsi;
        // Swap msb
        daiet->pool_index = rte_cpu_to_be_16(tsi_to_pool_index(tsi));

        fill_fn(entry, tsi, tensor_size);
    }

    __rte_always_inline uint16_t build_pkt(rte_mbuf* m, unsigned portid, uint32_t tsi, uint32_t tensor_size) {

        uint16_t pool_index = tsi_to_pool_index(tsi);

        struct ether_hdr *eth;
        struct ipv4_hdr *ip;
        struct udp_hdr *udp;
        struct daiet_hdr *daiet;
        struct entry_hdr *entry;
        void *tmp;

        m->data_len = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + sizeof(struct daiet_hdr) + entries_size;
        m->pkt_len = m->data_len;

        // Checksum offload
        m->l2_len = sizeof(struct ether_hdr);
        m->l3_len = sizeof(struct ipv4_hdr);
        m->ol_flags |= daiet_par.getTxFlags();

        rte_prefetch0 (rte_pktmbuf_mtod(m, void *));
        eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

        // Set MAC addresses
        tmp = &eth->d_addr.addr_bytes[0];
        *((uint64_t *) tmp) = daiet_par.getPsMacBe(pool_index); // Changes the first 2B of the src address too
        rte_eth_macaddr_get(portid, &eth->s_addr);

        // Set ethertype
        eth->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

        // IP header
        ip = (struct ipv4_hdr *) (eth + 1);
        ip->version_ihl = 0x45;
        ip->total_length = rte_cpu_to_be_16(m->data_len - sizeof(struct ether_hdr));
        ip->time_to_live = 128;
        ip->next_proto_id = IPPROTO_UDP;
        ip->hdr_checksum = 0;
        ip->src_addr = daiet_par.getWorkerIpBe();
        ip->dst_addr = daiet_par.getPsIpBe(pool_index);

        // UDP header
        udp = (struct udp_hdr *) (ip + 1);
        udp->src_port = worker_port_be;
        udp->dst_port = ps_port_be;
        udp->dgram_len = rte_cpu_to_be_16(m->data_len - sizeof(struct ether_hdr) - sizeof(struct ipv4_hdr));
        udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, m->ol_flags);

        // DAIET header
        daiet = (struct daiet_hdr *) (udp + 1);
        daiet->tsi = tsi;
        daiet->pool_index = rte_cpu_to_be_16(pool_index);

        entry = (struct entry_hdr *) (daiet + 1);

        fill_fn(entry, tsi, tensor_size);

        return pool_index;
    }

#ifdef DEBUG
    __rte_always_inline struct daiet_hdr * is_daiet_pkt_from_ps(struct ether_hdr* eth_hdr, uint16_t size) {

        int idx;
        uint16_t etherType;
        struct ipv4_hdr* ip_hdr;
        struct udp_hdr* udp_hdr;

        idx = sizeof(struct ether_hdr);
        etherType = rte_be_to_cpu_16(eth_hdr->ether_type);

        if (etherType == ETHER_TYPE_IPv4 && size >= idx + sizeof(struct ipv4_hdr)) {

            idx += sizeof(struct ipv4_hdr);
            ip_hdr = (struct ipv4_hdr *) (eth_hdr + 1);

            if (ip_hdr->next_proto_id == IPPROTO_UDP && size >= idx + sizeof(struct udp_hdr)) {
                idx += sizeof(struct udp_hdr);
                udp_hdr = (struct udp_hdr *) (ip_hdr + 1);

                if (udp_hdr->dst_port == worker_port_be && size >= idx + sizeof(struct daiet_hdr)) {

                    return (struct daiet_hdr *) (udp_hdr + 1);
                }
            }
        }
        return NULL;
    }
#endif

#ifdef TIMERS
    void resend_pkt(struct rte_timer *timer, void *arg) {

        uint32_t tsi = *((uint32_t*) arg);

        LOG_DEBUG("Timeout TSI: " + to_string(tsi));

        w_timeouts++;

        // Reallocate, Rebuild, Resend packet
        struct rte_mbuf* m[1];
        m[0] = rte_pktmbuf_alloc(pool);
        if (unlikely(m == NULL)) {
            LOG_FATAL("Cannot allocate one packet");
        }

        uint32_t pool_index_monoset = build_pkt(m[0], dpdk_par.portid, tsi, tensor_size) & 0x7FFF;

        while (rte_eth_tx_burst(dpdk_par.portid, worker_id, m, 1)==0)
            ;

#ifdef TIMESTAMPS
        pair<uint32_t,uint64_t> ts;
        ts.first = pool_index_monoset;
        ts.second = rte_get_timer_cycles();
        resent_pkt_timestamps.push_back(ts);
#endif

        rte_timer_reset_sync(timer, timer_cycles, PERIODICAL, lcore_id, resend_pkt, arg);
    }
#endif

    void tx_buffer_callback(struct rte_mbuf **pkts, uint16_t unsent, __attribute__((unused)) void *userdata) {

        LOG_DEBUG("TX buffer error: unsent " + to_string(unsent));
        unsigned nb_tx = 0, sent = 0;

        do {
            nb_tx = rte_eth_tx_burst(dpdk_par.portid, worker_id, &pkts[sent], unsent - sent);

            sent += nb_tx;
        } while (sent < unsent);

    }

    /**
     * Free a list of packet mbufs back into its original mempool.
     *
     * Free a list of mbufs by calling rte_pktmbuf_free() in a loop as a wrapper function.
     *
     * @param m_list
     *   An array of rte_mbuf pointers to be freed.
     * @param npkts
     *   Number of packets to free in m_list.
     */
    __rte_always_inline void rte_pktmbuf_free_bulk(struct rte_mbuf *m_list[], int16_t npkts) {
        while (npkts--)
            rte_pktmbuf_free(*m_list++);
    }

    void worker_setup() {

#ifdef TIMERS
        // Initialize timer library
        rte_timer_subsystem_init();
#endif
    }

    void worker_cleanup() {
    }

    int worker(void* arg) {

        DaietContext* dctx_ptr = (DaietContext*) arg;

        const uint32_t max_num_pending_messages = daiet_par.getMaxNumPendingMessages();
        num_updates = daiet_par.getNumUpdates();
        entries_size = sizeof(struct entry_hdr) * num_updates;
        shift = 0;
        tensor_size = 0;
        scalingfactor = daiet_par.getScalingFactor();
        scalingfactor_vec = scalingfactor;

        volatile uint32_t rx_pkts = 0;
        uint64_t w_tx = 0, w_rx = 0;

        uint32_t total_num_msgs = 0;
        uint32_t burst_size = 0;

#ifdef DEBUG
        uint32_t sent_message_counters[max_num_pending_messages];
#endif

#ifdef LATENCIES
        uint64_t sent_timestamps[max_num_pending_messages];
        uint64_t lat_idx = 0;
#endif

#ifdef TIMESTAMPS
        vector<pair<uint32_t,uint64_t>> global_sent_timestamps;
        uint64_t  first_global_ts = 0;
#endif

#if defined(TIMESTAMPS) || defined(LATENCIES)
        uint32_t round_ts= 0;
#endif

        int ret;

        unsigned socket_id = rte_socket_id();
        unsigned nb_rx = 0, nb_tx = 0, sent = 0, j = 0;

#ifndef TIMERS
        struct rte_mempool *pool;
#else
        struct rte_timer timers[max_num_pending_messages];
        uint32_t timer_tsis[max_num_pending_messages];
#endif
        string pool_name = "worker_pool";
        struct rte_mbuf **pkts_tx_burst;
        struct rte_mbuf **pkts_rx_burst;
        struct rte_mbuf* m;
        struct rte_eth_dev_tx_buffer* tx_buffer;

        uint64_t prev_tsc = 0, cur_tsc = 0;
        const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * dpdk_par.bulk_drain_tx_us;

        struct ether_hdr* eth;
        struct daiet_hdr* daiet;

        uint32_t tsi = 0;
        uint16_t pool_index = 0;
        uint16_t pool_index_monoset = 0;

        // Get core ID
        lcore_id = rte_lcore_id();
        worker_id = dpdk_data.core_to_thread_id[lcore_id];
        LOG_DEBUG("Worker core: " + to_string(lcore_id) + " worker id: " + to_string(worker_id));
        worker_port_be = rte_cpu_to_be_16(daiet_par.getBaseWorkerPort() + worker_id);

#ifdef COLOCATED
        ps_port_be = rte_cpu_to_be_16(daiet_par.getBasePsPort() + worker_id);
#else
        ps_port_be = rte_cpu_to_be_16(daiet_par.getBasePsPort());
#endif

        start_pool_index = worker_id *max_num_pending_messages;

#ifdef TIMERS

        uint64_t timer_prev_tsc = 0, timer_cur_tsc;

        for (uint32_t i = 0; i < max_num_pending_messages; i++) {
            rte_timer_init(&timers[i]);
        }
#endif

        // Init the buffer pool
        pool_name = pool_name + to_string(worker_id);
        pool = rte_pktmbuf_pool_create(pool_name.c_str(), dpdk_par.pool_size, dpdk_par.pool_cache_size, 0, dpdk_data.pool_buffer_size, rte_socket_id());
        if (pool == NULL)
            LOG_FATAL("Cannot init mbuf pool: " + string(rte_strerror(rte_errno)));

        // Initialize TX buffers
        tx_buffer = (rte_eth_dev_tx_buffer*) rte_zmalloc_socket("tx_buffer", RTE_ETH_TX_BUFFER_SIZE(dpdk_par.burst_tx), RTE_CACHE_LINE_SIZE, socket_id);

        if (tx_buffer == NULL)
            LOG_FATAL("Cannot allocate TX buffer");

        rte_eth_tx_buffer_init(tx_buffer, dpdk_par.burst_tx);

        ret = rte_eth_tx_buffer_set_err_callback(tx_buffer, tx_buffer_callback, NULL);

        if (ret < 0)
            LOG_FATAL("Cannot set callback for tx buffer");

        // Bitmap
        void* bitmap_mem;
        uint32_t bitmap_size;
        struct rte_bitmap *bitmap;
        uint32_t pkt_idx = 0;

        // Allocate pkt burst
        pkts_tx_burst = (rte_mbuf **) rte_malloc_socket(NULL, max_num_pending_messages * sizeof(struct rte_mbuf*), RTE_CACHE_LINE_SIZE, socket_id);
        if (unlikely(pkts_tx_burst == NULL))
            LOG_FATAL("Cannot allocate pkts tx burst");

        ret = rte_pktmbuf_alloc_bulk(pool, pkts_tx_burst, max_num_pending_messages);
        if (unlikely(ret < 0))
            LOG_FATAL("Cannot allocate mbuf tx burst");

        pkts_rx_burst = (rte_mbuf **) rte_malloc_socket(NULL, max_num_pending_messages * sizeof(struct rte_mbuf*), RTE_CACHE_LINE_SIZE, socket_id);
        if (unlikely(pkts_rx_burst == NULL))
            LOG_FATAL("Cannot allocate pkts rx burst");

        dctx_ptr->set_master_ready();

        while (!force_quit) {

            if (dctx_ptr->receive_tensor(tu)) {

#ifdef DEBUG
                memset(sent_message_counters, 0, max_num_pending_messages * sizeof(*sent_message_counters));
#endif

                rx_pkts = 0;
                tsi = 0;
                tensor_size = tu.count;
                total_num_msgs = tensor_size / num_updates;

                if (tensor_size % num_updates != 0)
                    total_num_msgs++; // one final padded packet

                if (tu.type == INT) {

                    tu.ptr = static_cast<uint32_t*>(tu.ptr) + tu.start_idx;
                    fill_fn = &fill_int32;
                    store_fn = &store_int32;

                } else if (tu.type == FLOAT) {

                    tu.ptr = static_cast<float*>(tu.ptr) + tu.start_idx;
                    fill_fn = &fill_float32;
                    store_fn = &store_float32;
                }

#ifdef LATENCIES
                uint64_t latencies[total_num_msgs];
                memset(latencies, 0, total_num_msgs * (sizeof(*latencies)));
                lat_idx = 0;

                memset(sent_timestamps, 0, max_num_pending_messages * (sizeof(*sent_timestamps)));
#endif

#ifdef TIMESTAMPS
                global_sent_timestamps.clear();
                global_sent_timestamps.reserve(total_num_msgs);
                first_global_ts = 0;
#ifdef TIMERS
                resent_pkt_timestamps.clear();
#endif
#endif

                // Initialize bitmap
                bitmap_size = rte_bitmap_get_memory_footprint(total_num_msgs);
                if (unlikely(bitmap_size == 0)) {
                    LOG_FATAL("Bitmap failed");
                }

                bitmap_mem = rte_zmalloc_socket("bitmap", bitmap_size, RTE_CACHE_LINE_SIZE, socket_id);
                if (unlikely(bitmap_mem == NULL)) {
                    LOG_FATAL("Cannot allocate bitmap");
                }

                bitmap = rte_bitmap_init(total_num_msgs, (uint8_t*) bitmap_mem, bitmap_size);
                if (unlikely(bitmap == NULL)) {
                    LOG_FATAL("Failed to init bitmap");
                }
                rte_bitmap_reset(bitmap);

                // Send first pkt burst
                burst_size = total_num_msgs < max_num_pending_messages ? total_num_msgs : max_num_pending_messages;

                for (j = 0; j < burst_size; j++) {
                    m = pkts_tx_burst[j];

                    // Increase refcnt so it is not freed
                    rte_mbuf_refcnt_update(m,1);

                    pool_index_monoset = (build_pkt(m, dpdk_par.portid, tsi, tensor_size) -start_pool_index) & 0x7FFF;

#ifdef TIMERS
                    timer_tsis[pool_index_monoset] = tsi;
                    rte_timer_reset_sync(&timers[pool_index_monoset], timer_cycles * max_num_pending_messages, PERIODICAL, lcore_id, resend_pkt, &(timer_tsis[pool_index_monoset]));
#endif

#ifdef DEBUG
                    sent_message_counters[pool_index_monoset]++;
#endif

#ifdef LATENCIES
                    write_timestamp(sent_timestamps,pool_index_monoset);
#endif

                    tsi += num_updates;
                }

                // Transmit the packet burst
                sent = 0;
                do {
                    nb_tx = rte_eth_tx_burst(dpdk_par.portid, worker_id, &pkts_tx_burst[sent], burst_size - sent);

                    sent += nb_tx;
                } while (sent < burst_size);

                w_tx += burst_size;

                while (rx_pkts < total_num_msgs && !force_quit) {

                    // Read packet from RX ring
                    nb_rx = rte_eth_rx_burst(dpdk_par.portid, worker_id, pkts_rx_burst, dpdk_par.burst_rx);

                    if (unlikely(nb_rx == 0)) {

                        cur_tsc = rte_get_timer_cycles();

                        if (unlikely((cur_tsc - prev_tsc) > drain_tsc)) {
                            // TX drain
                            nb_tx = rte_eth_tx_buffer_flush(dpdk_par.portid, worker_id, tx_buffer);
                            if (nb_tx)
                                w_tx += nb_tx;

                            prev_tsc = cur_tsc;
                        }

#ifdef TIMERS
                        // Check timers
                        timer_cur_tsc = cur_tsc;
                        if (unlikely(timer_cur_tsc - timer_prev_tsc > timer_cycles)) {
                            rte_timer_manage();
                            timer_prev_tsc = timer_cur_tsc;
                        }
#endif

                    } else {

                        for (j = 0; j < nb_rx; j++) {

                            m = pkts_rx_burst[j];
                            pkts_rx_burst[j] = NULL;

                            // Checksum offload
                            // TOFIX these assignments have a ~20% performance overhead
                            //m->l2_len = sizeof(struct ether_hdr);
                            //m->l3_len = sizeof(struct ipv4_hdr);
                            //m->ol_flags |= daiet_par.getTxFlags();

                            rte_prefetch0 (rte_pktmbuf_mtod(m, void *));
                            eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

#ifdef DEBUG
                            daiet = is_daiet_pkt_from_ps(eth, m->data_len);
                            if (likely(daiet != NULL)) {
#else
                                daiet = (struct daiet_hdr *) ((uint8_t *) (eth+1) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
#endif
                                tsi = daiet->tsi;

                                pkt_idx = tsi / num_updates;

                                pool_index = rte_be_to_cpu_16(daiet->pool_index);
                                // Clear msb
                                pool_index_monoset = (pool_index - start_pool_index) & 0x7FFF;

                                w_rx++;

                                if (likely(rte_bitmap_get(bitmap, pkt_idx) == 0)) {

                                    rx_pkts++;

#ifdef TIMERS
                                    rte_timer_stop_sync(&timers[pool_index_monoset]);
#endif

                                    rte_bitmap_set(bitmap, pkt_idx);
#ifdef LATENCIES
                                    // Save latency
                                    save_latency(latencies, sent_timestamps, pool_index_monoset, lat_idx);

                                    lat_idx += 1;
#endif

#ifdef TIMESTAMPS
                                    // Save timestamp
                                    write_global_timestamp(global_sent_timestamps, pool_index_monoset);
#endif

                                    // Store result
                                    store_fn(daiet, tensor_size);

                                    tsi += num_updates * max_num_pending_messages;

#ifdef TIMERS
                                    timer_tsis[pool_index_monoset] = tsi;
#endif

                                    if (likely(tsi < tensor_size)) {

                                        //Resend the packet
                                        reset_pkt(eth, dpdk_par.portid, tsi, tensor_size, m->ol_flags);

                                        nb_tx = rte_eth_tx_buffer(dpdk_par.portid, worker_id, tx_buffer, m);
                                        if (nb_tx) {
                                            w_tx += nb_tx;
                                            prev_tsc = cur_tsc;
                                        }

#ifdef TIMERS
                                        // Start timer
                                        rte_timer_reset_sync(&timers[pool_index_monoset], timer_cycles, PERIODICAL, lcore_id, resend_pkt,
                                                &timer_tsis[pool_index_monoset]);
#endif

#ifdef DEBUG
                                        sent_message_counters[pool_index_monoset]++;
#endif
                                    } else {
                                        
                                        // Free the packet
                                        rte_pktmbuf_free(m);
                                    }
                                } else {
                                    // We have seen this packet before
#ifdef DEBUG
                                    LOG_DEBUG("Duplicated packet");
                                    print_packet(eth,m->data_len);
#endif

                                    rte_pktmbuf_free(m);
                                }
#ifdef DEBUG
                            } else {

                                LOG_DEBUG("Wrong packet");
                                print_packet(eth,m->data_len);

                                // Free original packet
                                rte_pktmbuf_free(m);
                            }
#endif
                        }
                    }
                }
                // Done update

                // Update shift
                shift = (shift + total_num_msgs) % (2 * max_num_pending_messages);

                while (!dctx_ptr->send_result(tu.id) && !force_quit)
                    ;

                rte_bitmap_free(bitmap);
                rte_free(bitmap_mem);

#ifdef LATENCIES
                dump_latencies(latencies, total_num_msgs, "latency_" + to_string(round_ts) + "_usec.dat");
#endif

#ifdef TIMESTAMPS
                uint64_t base_ts = dump_timestamps(global_sent_timestamps, "recv_timestamps_" + to_string(round_ts) + "_usec.dat");
                round_ts++;
#ifdef TIMERS
                dump_resent_timestamps(base_ts, "resent_timestamps_" + to_string(round_ts) + "_usec.dat");
#endif
#endif

            }
        } // force quit

#ifdef DEBUG
        LOG_DEBUG("Sent messages counters");
        for (uint32_t i = 1; i < max_num_pending_messages; i++){
            if (sent_message_counters[i-1] != sent_message_counters[i]){
                LOG_DEBUG("Index: " + to_string(i-1) + " -> " + to_string(sent_message_counters[i-1]));
                LOG_DEBUG("Index: " + to_string(i) + " -> " + to_string(sent_message_counters[i]));
            }
        }
#endif
        // Set stats
        pkt_stats.set_workers(worker_id, w_tx, w_rx);

#ifdef TIMERS
        pkt_stats.set_timeouts(worker_id, w_timeouts);
#endif
        // Cleanup
        rte_free(pkts_rx_burst);
        rte_pktmbuf_free_bulk(pkts_tx_burst, burst_size);
        rte_free(pkts_tx_burst);
        rte_free(tx_buffer);

        return 0;
    }
}
