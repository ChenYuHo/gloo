/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "worker.hpp"

#if defined ( __AVX512F__ ) || defined ( __AVX512__ )
#define MAX_VECTOR_SIZE 512
#endif
#include "vcl/vectorclass.h"

using namespace std;

namespace daiet {

    TensorUpdate* tuptr;
    uint32_t num_updates;
    uint16_t shift;

    float scalingfactor;

#if MAX_VECTOR_SIZE >= 512
    Vec16f vec_f;
    Vec16i vec_i;
    Vec16f scalingfactor_vec;
#else
    Vec8f vec_f;
    Vec8i vec_i;
    Vec8f scalingfactor_vec;
#endif

    void (*fill_fn)(entry_hdr*, uint32_t, uint32_t);
    void (*store_fn)(daiet_hdr*, uint32_t);

#ifdef TIMERS
    // TOFIX this should be thread local
    struct rte_mempool *pool;
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

        // Write latency file
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

    __rte_always_inline void write_global_timestamp(uint64_t* sent_timestamps, uint64_t idx) {

        sent_timestamps[idx] = rte_get_timer_cycles();
    }

    void dump_timestamps(uint64_t* sent_timestamps, uint32_t total_num_msgs, string file_name) {

        // Write latency file
        LOG_INFO("Writing timestamps file...");

        uint64_t hz = rte_get_timer_hz();

        ofstream timestamps_file(file_name);

        if (timestamps_file.is_open()) {

            uint64_t first = sent_timestamps[0];
            timestamps_file << (double)(0) << endl;
            for (uint32_t i = 1; i < total_num_msgs && !force_quit; i++) {
                timestamps_file << ((double) (sent_timestamps[i]-first)) * 1000000 / hz << endl;
            }

            timestamps_file.close();
        } else {
            LOG_ERROR("Unable to open timestamps file");
        }
    }
#endif

    __rte_always_inline uint16_t tsi_to_pool_index(const uint32_t& tsi) {

        uint32_t i = ((tsi / num_updates) + shift) % (2 * daiet_par.getMaxNumPendingMessages());
        if (i < daiet_par.getMaxNumPendingMessages())
            // Set 0
            return i;
        else
            // Set 1
            return (i - daiet_par.getMaxNumPendingMessages()) | 0x8000;
    }

    __rte_always_inline void store_int32(daiet_hdr* daiet, uint32_t tensor_size) {

        uint32_t tsi = daiet->tsi;
        uint32_t final_tsi;
        struct entry_hdr * entry = (struct entry_hdr *) (daiet + 1);

        if (likely(tsi + num_updates <= tensor_size)) {

            //rte_memcpy(&(static_cast<uint32_t*>(tuptr->ptr)[tsi]), entry, entries_size);

            for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi++, entry++) {

                static_cast<uint32_t*>(tuptr->ptr)[tsi] = rte_be_to_cpu_32(entry->upd);
            }

        } else {

            //rte_memcpy(&(static_cast<uint32_t*>(tuptr->ptr)[tsi]), entry, sizeof(struct entry_hdr) * (tensor_size - tsi));

            for (final_tsi = tsi + tensor_size - tsi; tsi < final_tsi; tsi++, entry++) {

                static_cast<uint32_t*>(tuptr->ptr)[tsi] = rte_be_to_cpu_32(entry->upd);
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
             vec_f.store(&(static_cast<float*>(tuptr->ptr)[tsi]));
             }
             */

            for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi++, entry++) {

                static_cast<float*>(tuptr->ptr)[tsi] = (float(rte_be_to_cpu_32(entry->upd))) / scalingfactor;
            }

        } else {

            for (final_tsi = tsi + tensor_size - tsi; tsi < final_tsi; tsi++, entry++) {

                static_cast<float*>(tuptr->ptr)[tsi] = (float(rte_be_to_cpu_32(entry->upd))) / scalingfactor;
            }
        }

    }

    __rte_always_inline void fill_int32(struct entry_hdr *entry, uint32_t tsi, uint32_t tensor_size) {

        uint32_t final_tsi;

        if (likely(tsi + num_updates <= tensor_size)) {

            // rte_memcpy(entry, &(static_cast<uint32_t*>(tuptr->ptr)[tsi]), entries_size);
            for (final_tsi = tsi + num_updates; tsi < final_tsi; tsi++, entry++) {
                entry->upd = rte_cpu_to_be_32((static_cast<uint32_t*>(tuptr->ptr)[tsi]));
            }

        } else {
            //Padding
            uint32_t num_valid = tensor_size - tsi;
            uint32_t zeros = num_updates - num_valid;

            //rte_memcpy(entry, &(static_cast<uint32_t*>(tuptr->ptr)[tsi]), sizeof(struct entry_hdr) * num_valid);
            for (final_tsi = tsi + num_valid; tsi < final_tsi; tsi++, entry++) {
                entry->upd = rte_cpu_to_be_32((static_cast<uint32_t*>(tuptr->ptr)[tsi]));
            }

            memset(entry + num_valid, 0, sizeof(struct entry_hdr) * zeros);
        }
    }

    __rte_always_inline void fill_float32(struct entry_hdr *entry, uint32_t tsi, uint32_t tensor_size) {

        float* cur_float_ptr;
        uint32_t* cur_int_ptr;
        uint32_t* final_ptr;

        cur_float_ptr = &(static_cast<float*>(tuptr->ptr)[tsi]);
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
        udp->src_port = daiet_par.getWorkerPortBe();
        udp->dst_port = daiet_par.getPsPortBe();
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
        udp->src_port = daiet_par.getWorkerPortBe();
        udp->dst_port = daiet_par.getPsPortBe();
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

                if (udp_hdr->dst_port == daiet_par.getWorkerPortBe() && size >= idx + sizeof(struct daiet_hdr)) {

                    return (struct daiet_hdr *) (udp_hdr + 1);
                }
            }
        }
        return NULL;
    }

#ifdef TIMERS
    void timeout_cb(struct rte_timer *timer, void *arg) {

        int ret;
        uint32_t* tsi = (uint32_t*) arg;

        LOG_DEBUG("Timeout TSI: " + to_string(*tsi));

        pkt_stats.w_timeouts++;

        // Reallocate, Rebuild, Resend packet
        struct rte_mbuf* m = rte_pktmbuf_alloc(pool);
        if (unlikely(m == NULL)) {
            LOG_FATAL("Cannot allocate one packet");
        }

        build_pkt(m, dpdk_par.portid, *tsi, sizeof(struct entry_hdr) * daiet_par.getNumUpdates());

        ret = rte_ring_enqueue(dpdk_data.w_ring_tx, m);
        if (unlikely(ret < 0)) {
            LOG_FATAL("Cannot enqueue one packet in timeout callback");
        }
    }
#endif

    void tx_buffer_callback(struct rte_mbuf **pkts, uint16_t unsent, __attribute__((unused)) void *userdata) {

        LOG_DEBUG("TX buffer error: unsent " + to_string(unsent));
        unsigned nb_tx = 0, sent = 0;

        do {
            nb_tx = rte_eth_tx_burst(dpdk_par.portid, 0, &pkts[sent], unsent - sent);

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
        shift = 0;
        scalingfactor = daiet_par.getScalingFactor();
        scalingfactor_vec = scalingfactor;

        volatile uint32_t rx_pkts = 0;
        uint32_t total_num_msgs = 0;
        uint32_t burst_size = 0;
        uint32_t tensor_size = 0;
        uint32_t sent_message_counters[max_num_pending_messages];

#ifdef LATENCIES
        uint64_t sent_timestamps[max_num_pending_messages];
        uint64_t lat_idx = 0;
#endif

#ifdef TIMESTAMPS
        uint32_t round_ts= 0;
        uint64_t ts_idx = 0;
#endif

        int ret;

        unsigned lcore_id;
        unsigned socket_id = rte_socket_id();
        unsigned nb_rx = 0, nb_tx = 0, sent = 0, j = 0;
        uint32_t worker_id;

#ifndef TIMERS
        struct rte_mempool *pool;
#endif
        string pool_name = "worker_pool";
        struct rte_mbuf **pkts_burst;
        struct rte_mbuf* m;
        struct rte_eth_dev_tx_buffer* tx_buffer;

        uint64_t prev_tsc = 0, diff_tsc = 0, cur_tsc = 0;
        const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * dpdk_par.bulk_drain_tx_us;

        struct ether_hdr* eth;
        struct daiet_hdr* daiet;

        uint32_t tsi = 0;
        uint16_t pool_index = 0;
        uint16_t pool_index_monoset = 0;

        // Get core ID
        lcore_id = rte_lcore_id();
        worker_id = core_to_workers_ids[lcore_id];
        LOG_DEBUG("Worker core: " + to_string(lcore_id) + " worker id: " + to_string(worker_id));

#ifdef TIMERS
        // Timer
        uint64_t timer_cycles = rte_get_timer_hz() / 1000;// cycles for 1 ms
        uint64_t timer_prev_tsc = 0, timer_cur_tsc;
        uint32_t timer_tsis[max_num_pending_messages];

        struct rte_timer timers[max_num_pending_messages];

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

        pkts_burst = (rte_mbuf **) rte_malloc_socket(NULL, max_num_pending_messages * sizeof(struct rte_mbuf*), RTE_CACHE_LINE_SIZE, socket_id);
        if (unlikely(pkts_burst == NULL))
            LOG_FATAL("Worker thread: cannot allocate pkts burst");

        dctx_ptr->set_master_ready();

        while (!force_quit) {

            tuptr = dctx_ptr->receive_tensor();

            if (tuptr != NULL) {

                memset(sent_message_counters, 0, max_num_pending_messages * sizeof(*sent_message_counters));

                rx_pkts = 0;
                tsi = 0;
                tensor_size = tuptr->count;
                total_num_msgs = tensor_size / num_updates;

                if (tensor_size % num_updates != 0)
                    total_num_msgs++; // one final padded packet

                if (tuptr->type == FLOAT) {
                    fill_fn = &fill_float32;
                    store_fn = &store_float32;
                } else if (tuptr->type == INT) {
                    fill_fn = &fill_int32;
                    store_fn = &store_int32;
                }

#ifdef LATENCIES
                uint64_t latencies[total_num_msgs];
                memset(latencies, 0, total_num_msgs * (sizeof(*latencies)));
                lat_idx = 0;

                memset(sent_timestamps, 0, max_num_pending_messages * (sizeof(*sent_timestamps)));
#endif

#ifdef TIMESTAMPS
                uint64_t global_sent_timestamps[total_num_msgs];
                memset(global_sent_timestamps, 0, total_num_msgs * (sizeof(*global_sent_timestamps)));
                ts_idx = 0;
#endif

                // Initialize bitmap
                bitmap_size = rte_bitmap_get_memory_footprint(total_num_msgs);
                if (unlikely(bitmap_size == 0)) {
                    LOG_FATAL("Worker thread: bitmap failed");
                }

                bitmap_mem = rte_zmalloc_socket("bitmap", bitmap_size, RTE_CACHE_LINE_SIZE, socket_id);
                if (unlikely(bitmap_mem == NULL)) {
                    LOG_FATAL("Worker thread: cannot allocate bitmap");
                }

                bitmap = rte_bitmap_init(total_num_msgs, (uint8_t*) bitmap_mem, bitmap_size);
                if (unlikely(bitmap == NULL)) {
                    LOG_FATAL("Failed to init bitmap");
                }
                rte_bitmap_reset(bitmap);

                // Send first pkt burst
                burst_size = total_num_msgs < max_num_pending_messages ? total_num_msgs : max_num_pending_messages;

                // Allocate pkt burst
                ret = rte_pktmbuf_alloc_bulk(pool, pkts_burst, burst_size);
                if (unlikely(ret < 0))
                    LOG_FATAL("Cannot allocate pkts burst");

                for (j = 0; j < burst_size; j++) {
                    m = pkts_burst[j];

                    pool_index_monoset = build_pkt(m, dpdk_par.portid, tsi, tensor_size) & 0x7FFF;

#ifdef TIMERS
                    timer_tsis[pool_index_monoset] = tsi;
                    rte_timer_reset_sync(&timers[pool_index_monoset], timer_cycles * max_num_pending_messages, PERIODICAL, lcore_id, timeout_cb, &(timer_tsis[pool_index_monoset]));
#endif

                    tsi += num_updates;

                    sent_message_counters[pool_index_monoset]++;

#ifdef LATENCIES
                    write_timestamp(sent_timestamps,pool_index_monoset);
#endif

#ifdef TIMESTAMPS
                    write_global_timestamp(global_sent_timestamps,j);
#endif
                }

                // Transmit the packet burst
                sent = 0;
                do {
                    nb_tx = rte_eth_tx_burst(dpdk_par.portid, 0, &pkts_burst[sent], burst_size - sent);

                    sent += nb_tx;
                } while (sent < burst_size);

                pkt_stats.w_tx += burst_size;

#ifdef LATENCIES
                lat_idx += burst_size;
#endif

#ifdef TIMESTAMPS
                ts_idx += burst_size;
#endif

                while (rx_pkts < total_num_msgs && !force_quit) {

#ifdef TIMERS
                    // Check timers
                    timer_cur_tsc = rte_rdtsc();
                    if (unlikely(timer_cur_tsc - timer_prev_tsc > timer_cycles)) {
                        rte_timer_manage();
                        timer_prev_tsc = timer_cur_tsc;
                    }
#endif

                    // Read packet from RX ring
                    nb_rx = rte_eth_rx_burst(dpdk_par.portid, 0, pkts_burst, dpdk_par.burst_rx);

                    if (unlikely(nb_rx == 0)) {

                        cur_tsc = rte_get_timer_cycles();

                        diff_tsc = cur_tsc - prev_tsc;
                        if (unlikely(diff_tsc > drain_tsc)) {
                            // TX drain
                            nb_tx = rte_eth_tx_buffer_flush(dpdk_par.portid, 0, tx_buffer);
                            if (nb_tx)
                                pkt_stats.w_tx += nb_tx;

                            prev_tsc = cur_tsc;
                        }
                    } else {

                        for (j = 0; j < nb_rx; j++) {

                            m = pkts_burst[j];
                            pkts_burst[j] = NULL;

                            // Checksum offload
                            // TOFIX these assignments have a ~20% performance overhead
                            //m->l2_len = sizeof(struct ether_hdr);
                            //m->l3_len = sizeof(struct ipv4_hdr);
                            //m->ol_flags |= daiet_par.getTxFlags();

                            rte_prefetch0 (rte_pktmbuf_mtod(m, void *));
                            eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

#ifndef COLOCATED
                            daiet = is_daiet_pkt_from_ps(eth, m->data_len);
                            if (likely(daiet != NULL)) {
#else
                                daiet = (struct daiet_hdr *) ((uint8_t *) (eth+1) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
#endif
                                tsi = daiet->tsi;

                                pkt_idx = tsi / num_updates;

                                pool_index = rte_be_to_cpu_16(daiet->pool_index);
                                // Clear msb
                                pool_index_monoset = pool_index & 0x7FFF;

                                pkt_stats.w_rx++;

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
                                    // Save latency
                                    write_global_timestamp(global_sent_timestamps, ts_idx);

                                    ts_idx += 1;
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

                                        nb_tx = rte_eth_tx_buffer(dpdk_par.portid, 0, tx_buffer, m);
                                        if (nb_tx) {
                                            pkt_stats.w_tx += nb_tx;
                                            prev_tsc = cur_tsc;
                                        }

#ifdef TIMERS
                                        // Start timer
                                        rte_timer_reset_sync(&timers[pool_index_monoset], timer_cycles, PERIODICAL, lcore_id, timeout_cb,
                                                &timer_tsis[pool_index_monoset]);
#endif

                                        sent_message_counters[pool_index_monoset]++;
                                    }

                                } else {
                                    // We have seen this packet before
#ifdef DEBUG
                                    LOG_DEBUG("Duplicated packet");
                                    print_packet(eth,m->data_len, daiet_par.getWorkerPortBe(),daiet_par.getPsPortBe());
#endif

                                    rte_pktmbuf_free(m);
                                }
#ifndef COLOCATED
                            } else {

#ifdef DEBUG
                                LOG_DEBUG("Wrong packet");
                                print_packet(eth,m->data_len, daiet_par.getWorkerPortBe(),daiet_par.getPsPortBe());
#endif

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

                while (!dctx_ptr->send_result(tuptr->id) && !force_quit)
                    ;

                rte_pktmbuf_free_bulk(pkts_burst, burst_size);

                rte_bitmap_free(bitmap);
                rte_free(bitmap_mem);

#ifdef LATENCIES
                dump_latencies(latencies, total_num_msgs, "latency_usec.dat");
#endif

#ifdef TIMESTAMPS
                dump_timestamps(global_sent_timestamps, total_num_msgs, "timestamps_" + to_string(round_ts) + "_usec.dat");
                round_ts++;
#endif

            }
        } // force quit

        // Cleanup
        rte_free(pkts_burst);
        rte_free(tx_buffer);

        return 0;
    }
}
