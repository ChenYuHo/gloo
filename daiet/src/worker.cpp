/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "worker.hpp"

namespace daiet {

    TensorUpdate* tuptr;

#if SAVE_LATENCIES
    rte_atomic64_t* sent_timestamp;

    static __rte_always_inline void write_timestamp(uint16_t offset) {

        uint64_t ts = rte_get_timer_cycles();

        rte_atomic64_set(&sent_timestamp[offset], ts);
    }

    static __rte_always_inline void save_latency(uint16_t offset, int64_t num_recv) {

        uint64_t ts = rte_get_timer_cycles();

        latencies[num_recv] = ts - rte_atomic64_exchange(reinterpret_cast<volatile uint64_t*>(&(sent_timestamp[offset].cnt)), ts);
    }
#endif

    static __rte_always_inline uint16_t tsi_to_pool_index(const uint32_t& tsi) {

        uint32_t i = (tsi / daiet_par.getNumUpdates()) % (2 * daiet_par.getMaxNumPendingMessages());
        if (i < daiet_par.getMaxNumPendingMessages())
            // Set 0
            return i;
        else
            // Set 1
            return (i - daiet_par.getMaxNumPendingMessages()) | 0x8000;
    }

    static __rte_always_inline void store(daiet_hdr* daiet) {

        uint32_t tsi = daiet->tsi;
        struct entry_hdr * entry = (struct entry_hdr *) (daiet + 1);

        if (tuptr->type == INT) {

            for (uint32_t i = 0; i < daiet_par.getNumUpdates(); i++) {
                tuptr->ptr.int_ptr[tsi] = int32_t(rte_be_to_cpu_32(entry->upd));
                entry++;
                tsi++;
            }
        } else if (tuptr->type == FLOAT) {

            for (uint32_t i = 0; i < daiet_par.getNumUpdates(); i++) {
                tuptr->ptr.float_ptr[tsi] = (int32_t(rte_be_to_cpu_32(entry->upd))) / daiet_par.getScalingFactor();
                entry++;
                tsi++;
            }
        }
    }

    static __rte_always_inline void reset_pkt(struct ether_hdr * eth, unsigned portid, uint32_t tsi, uint64_t ol_flags) {

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

        if (tuptr->type == INT) {

            for (uint32_t i = 0; i < daiet_par.getNumUpdates(); i++) {
                entry->upd = rte_cpu_to_be_32(tuptr->ptr.int_ptr[tsi]);
                entry++;
                tsi++;
            }
        } else if (tuptr->type == FLOAT) {

            for (uint32_t i = 0; i < daiet_par.getNumUpdates(); i++) {
                entry->upd = rte_cpu_to_be_32(round((tuptr->ptr.float_ptr[tsi]) * daiet_par.getScalingFactor()));
                entry++;
                tsi++;
            }
        }
    }

    static __rte_always_inline void build_pkt(rte_mbuf* m, unsigned portid, uint32_t tsi) {

        uint16_t pool_index = tsi_to_pool_index(tsi);

        struct ether_hdr *eth;
        struct ipv4_hdr *ip;
        struct udp_hdr *udp;
        struct daiet_hdr *daiet;
        struct entry_hdr *entry;
        void *tmp;

        m->data_len = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + sizeof(struct daiet_hdr)
                + sizeof(struct entry_hdr) * daiet_par.getNumUpdates();
        m->pkt_len = m->data_len;

        // Checksum offload
        m->l2_len = sizeof(struct ether_hdr);
        m->l3_len = sizeof(struct ipv4_hdr);
        m->ol_flags |= daiet_par.getTxFlags();

        rte_prefetch0(rte_pktmbuf_mtod(m, void *));
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

        if (tuptr->type == INT) {

            for (uint32_t i = 0; i < daiet_par.getNumUpdates(); i++) {
                entry->upd = rte_cpu_to_be_32(tuptr->ptr.int_ptr[tsi]);
                entry++;
                tsi++;
            }
        } else if (tuptr->type == FLOAT) {

            for (uint32_t i = 0; i < daiet_par.getNumUpdates(); i++) {
                entry->upd = rte_cpu_to_be_32(round((tuptr->ptr.float_ptr[tsi]) * daiet_par.getScalingFactor()));
                entry++;
                tsi++;
            }
        }
    }

    static __rte_always_inline struct daiet_hdr * is_daiet_pkt_from_ps(struct ether_hdr* eth_hdr, uint16_t size) {

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

    static void timeout_cb(struct rte_timer *timer, void *arg) {

        int ret;
        uint32_t* tsi = (uint32_t*) arg;

        LOG_DEBUG("Timeout TSI: " + to_string(*tsi));

        rte_atomic64_inc(&pkt_stats.w_timeouts);
        // Reallocate, Rebuild, Resend packet
        struct rte_mbuf* m = rte_pktmbuf_alloc(dpdk_data.pool);
        if (unlikely(m == NULL))
            LOG_FATAL("Cannot allocate one packet");

        build_pkt(m, dpdk_par.portid, *tsi);

        ret = rte_ring_enqueue(dpdk_data.w_ring_tx, m);
        if (unlikely(ret < 0))
            LOG_FATAL("Cannot enqueue one packet");
    }

    void worker_setup() {

        sent_message_counters = new rte_atomic32_t[daiet_par.getMaxNumPendingMessages()];
        for (int i = 0; i < daiet_par.getMaxNumPendingMessages(); i++) {
            rte_atomic32_init(&sent_message_counters[i]);
        }

        // Initialize timer library
        rte_timer_subsystem_init();

#if SAVE_LATENCIES
        sent_timestamp = new rte_atomic64_t[daiet_par.getMaxNumPendingMessages()];

        latencies = new uint64_t[daiet_par.getMaxNumMsgs()];
        memset(latencies, 0, (sizeof *latencies) * daiet_par.getMaxNumMsgs());

        for (int i = 0; i < daiet_par.getMaxNumPendingMessages(); i++) {
            rte_atomic64_init(&sent_timestamp[i]);
        }
#endif

    }

    void worker_cleanup() {
        delete[] sent_message_counters;

#if SAVE_LATENCIES
        delete[] sent_timestamp;
#endif
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
    static inline void __attribute__((always_inline))
    rte_pktmbuf_free_bulk(struct rte_mbuf *m_list[], int16_t npkts) {
        while (npkts--)
            rte_pktmbuf_free(*m_list++);
    }

    int worker(BlockingQueue<TensorUpdate*> &in_queue, BlockingQueue<TensorUpdate*> &out_queue) {

        const uint32_t max_num_pending_messages = daiet_par.getMaxNumPendingMessages();

        volatile int rx_pkts = 0;
        uint32_t total_num_msgs = 0;
        uint32_t burst_size = 0;
        uint32_t tensor_size = 0;

        int ret;

        unsigned lcore_id;
        unsigned socket_id = rte_socket_id();
        unsigned nb_rx = 0, j = 0;
        uint32_t worker_id;

#if SAVE_LATENCIES
        int64_t lat_indx = 0;
#endif

        struct rte_mbuf **pkts_burst;
        struct rte_mbuf* m;

        struct ether_hdr* eth;
        struct daiet_hdr* daiet;

        uint32_t tsi = 0;
        uint16_t pool_index = 0;
        uint16_t pool_index_monoset = 0;

        // Get core ID
        lcore_id = rte_lcore_id();
        worker_id = core_to_workers_ids[lcore_id];
        LOG_DEBUG("Worker core: " + to_string(lcore_id) + " worker id: " + to_string(worker_id));

        // Timer
        uint64_t timer_cycles = rte_get_timer_hz() / 10; // cycles for 100 ms
        uint64_t timer_prev_tsc = 0, timer_cur_tsc;
        uint32_t timer_tsis[max_num_pending_messages];

        struct rte_timer timers[max_num_pending_messages];

        for (int i = 0; i < max_num_pending_messages; i++) {
            rte_timer_init(&timers[i]);
        }

        // Bitmap
        void* bitmap_mem;
        uint32_t bitmap_size;
        struct rte_bitmap *bitmap;
        uint32_t bitmap_idx = 0;

        pkts_burst = (rte_mbuf **) rte_malloc_socket(NULL, max_num_pending_messages * sizeof(struct rte_mbuf*), RTE_CACHE_LINE_SIZE, socket_id);
        if (unlikely(pkts_burst == NULL))
            LOG_FATAL("Worker thread: cannot allocate pkts burst");

        while (!force_quit) {
            tuptr = in_queue.pop();

            if (tuptr == NULL)
                continue;

            memset(sent_message_counters, 0, daiet_par.getMaxNumPendingMessages() * sizeof(*sent_message_counters));

            rx_pkts = 0;
            tensor_size = tuptr->count;
            total_num_msgs = tensor_size / daiet_par.getNumUpdates();

            // Initialize bitmap
            bitmap_size = rte_bitmap_get_memory_footprint(total_num_msgs);
            if (unlikely(bitmap_size == 0)) {
                LOG_FATAL("Worker thread: bitmap failed");
            }

            bitmap_mem = rte_zmalloc_socket(NULL, bitmap_size, RTE_CACHE_LINE_SIZE, socket_id);
            if (unlikely(bitmap_mem == NULL)) {
                LOG_FATAL("Worker thread: cannot allocate bitmap");
            }

            bitmap = rte_bitmap_init(total_num_msgs, (uint8_t*) bitmap_mem, bitmap_size);
            if (unlikely(bitmap == NULL)) {
                LOG_FATAL("Failed to init bitmap");
            }
            rte_bitmap_reset (bitmap);

#if !COLOCATED
            // Only on master core
            if (lcore_id == rte_get_master_lcore())
#endif
                    {
                // Send first pkt burst

                burst_size = total_num_msgs < max_num_pending_messages ? total_num_msgs : max_num_pending_messages;

                // Allocate pkt burst
                ret = rte_pktmbuf_alloc_bulk(dpdk_data.pool, pkts_burst, burst_size);
                if (unlikely(ret < 0))
                    LOG_FATAL("Cannot allocate pkts burst");

                for (j = 0; j < burst_size; j++) {
                    m = pkts_burst[j];

                    timer_tsis[j] = daiet_par.getNumUpdates() * j;
                    build_pkt(m, dpdk_par.portid, timer_tsis[j]);

                    rte_atomic32_inc(&sent_message_counters[j]);

                    rte_timer_reset_sync(&timers[j], timer_cycles * max_num_pending_messages, PERIODICAL, lcore_id, timeout_cb, &(timer_tsis[j]));

#if SAVE_LATENCIES
                    write_timestamp(j);
#endif
                }

                // Transmit the packet burst
                do {
                    ret = rte_ring_enqueue_bulk(dpdk_data.w_ring_tx, (void **) pkts_burst, burst_size, NULL);
                } while (ret == 0);

                rte_atomic64_add(&pkt_stats.w_tx, max_num_pending_messages);
            }

            while (rx_pkts < total_num_msgs && !force_quit) {

                // Check timers
                timer_cur_tsc = rte_rdtsc();
                if (unlikely(timer_cur_tsc - timer_prev_tsc > TIMER_RESOLUTION_CYCLES)) {
                    rte_timer_manage();
                    timer_prev_tsc = timer_cur_tsc;
                }

                // Read packet from RX ring
                nb_rx = rte_ring_dequeue_burst(dpdk_data.w_ring_rx, (void **) pkts_burst, dpdk_par.burst_size_worker, NULL);

                for (j = 0; j < nb_rx; j++) {

                    m = pkts_burst[j];

                    // Checksum offload
                    // TOFIX these assignments have a ~20% performance overhead
                    //m->l2_len = sizeof(struct ether_hdr);
                    //m->l3_len = sizeof(struct ipv4_hdr);
                    //m->ol_flags |= daiet_par.getTxFlags();

                    rte_prefetch0(rte_pktmbuf_mtod(m, void *));
                    eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

#if !COLOCATED
                    daiet = is_daiet_pkt_from_ps(eth, m->data_len);
                    if (likely(daiet!=NULL)) {
#else
                        daiet = (struct daiet_hdr *) ((uint8_t *) (eth+1) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
#endif
                        tsi = daiet->tsi;
                        bitmap_idx = tsi / daiet_par.getNumUpdates();

                        pool_index = rte_be_to_cpu_16(daiet->pool_index);
                        // Clear msb
                        pool_index_monoset = pool_index & 0x7FFF;

                        rte_atomic64_inc(&pkt_stats.w_rx);

                        if (likely(rte_bitmap_get(bitmap, bitmap_idx) == 0)) {

                            rx_pkts++;
                            rte_timer_stop_sync(&timers[pool_index_monoset]);
                            rte_bitmap_set(bitmap, bitmap_idx);
#if SAVE_LATENCIES
                            // Save latency
                            save_latency(pool_index_monoset, lat_indx);

                            lat_indx += 1;
#endif
                            // Store result
                            store(daiet);

                            tsi += daiet_par.getNumUpdates() * max_num_pending_messages;
                            timer_tsis[pool_index_monoset] = tsi;

                            if (likely(tsi < tensor_size)) {

                                //Resend the packet
                                reset_pkt(eth, dpdk_par.portid, tsi, m->ol_flags);

                                ret = rte_ring_enqueue(dpdk_data.w_ring_tx, m);
                                if (unlikely(ret < 0))
                                    LOG_FATAL("Cannot enqueue one packet");

                                // Start timer
                                rte_timer_reset_sync(&timers[pool_index_monoset], timer_cycles, PERIODICAL, lcore_id, timeout_cb,
                                        &timer_tsis[pool_index_monoset]);

                                rte_atomic64_inc(&pkt_stats.w_tx);
                                rte_atomic32_inc(&sent_message_counters[pool_index_monoset]);
                            }

                        } else {
                            // We have seen this packet before
                            rte_pktmbuf_free(m);
                        }
#if !COLOCATED
                    } else {
                        // Free original packet
                        rte_pktmbuf_free(m);
                    }
#endif
                }
            }
            // Done update
            out_queue.push(tuptr);

#if !COLOCATED
            // Only on master core
            if (lcore_id == rte_get_master_lcore())
#endif
                    {
                rte_pktmbuf_free_bulk(pkts_burst, burst_size);
            }
            rte_bitmap_free(bitmap);
            rte_free(bitmap_mem);
        }

        // Cleanup
        rte_free(pkts_burst);

        return 0;
    }
}
