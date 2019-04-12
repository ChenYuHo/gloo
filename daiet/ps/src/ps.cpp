/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "ps.hpp"
#include "common.hpp"
#include "utils.hpp"
#include "params.hpp"
#include "stats.hpp"

using namespace std;

namespace daiet {

    struct mac_ip_pair {
        struct ether_addr mac;
        uint32_t be_ip;
    };

    thread_local static uint32_t num_updates;
    thread_local static mac_ip_pair* ps_workers_ip_to_mac;
    thread_local static uint32_t known_workers = 0;

    thread_local static int32_t** ps_aggregated_messages;
    thread_local static uint32_t* ps_received_message_counters;

    thread_local static uint16_t ps_port_be;

    thread_local static uint16_t ps_id;
    thread_local static struct rte_mempool *pool;
    thread_local static struct rte_mbuf** clone_burst;
    thread_local static struct rte_mbuf* cache_packet;
    thread_local static uint64_t ps_tx = 0;
    thread_local static uint16_t tno = 0;
    thread_local static struct rte_bitmap *bitmap = NULL;

#ifdef DEBUG
    __rte_always_inline struct daiet_hdr * is_daiet_pkt_to_ps(struct ether_hdr* eth_hdr, uint16_t size) {

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

                if (udp_hdr->dst_port == ps_port_be && size >= idx + sizeof(struct daiet_hdr)) {

                    return (struct daiet_hdr *) (udp_hdr + 1);
                }
            }
        }
        return NULL;
    }
#endif

    __rte_always_inline void ps_msg_setup(struct daiet_hdr * daiet, uint16_t pool_index) {

        struct entry_hdr *entry;
        int32_t* base_ptr = ps_aggregated_messages[pool_index];

        entry = (struct entry_hdr *) (daiet + 1);
        for (uint32_t i = 0; i < num_updates; i++, entry++) {
            entry->upd = rte_cpu_to_be_32(base_ptr[i]);
            base_ptr[i] = 0;
        }
    }

    /* Returns true if the aggregation for the offset is complete */
    __rte_always_inline bool ps_aggregate_message(struct daiet_hdr* daiet, uint32_t be_src_ip, struct ether_addr src_mac, uint16_t pool_index, uint16_t num_workers) {

        struct entry_hdr * entry = (struct entry_hdr *) (daiet + 1);
        int32_t* base_ptr = ps_aggregated_messages[pool_index];

        for (uint32_t i = 0; i < num_updates; i++, entry++) {
            base_ptr[i] += rte_be_to_cpu_32(entry->upd);
        }

        if (unlikely(known_workers < num_workers)) {

            bool found = false;

            for (uint32_t i = 0; i < known_workers && !found; i++) {

                if (ps_workers_ip_to_mac[i].be_ip==be_src_ip)
                    found = true;
            }

            if (!found) {

                // New worker
                char ipstring[INET_ADDRSTRLEN];

                if (unlikely(inet_ntop(AF_INET, &be_src_ip, ipstring, INET_ADDRSTRLEN) == NULL)) {
                    LOG_FATAL("Wrong IP: error " + to_string(errno));
                }

                LOG_INFO("Worker: " + string(ipstring) + " " + mac_to_str(src_mac));

                ps_workers_ip_to_mac[known_workers].mac = src_mac;
                ps_workers_ip_to_mac[known_workers].be_ip = be_src_ip;
                known_workers++;
            }
        }

        ps_received_message_counters[pool_index]++;

        if (unlikely(ps_received_message_counters[pool_index]==num_workers)) {
            ps_received_message_counters[pool_index] = 0;
            return true;
        }

        return false;
    }

#ifdef ALLOW_LOSS
    struct callback_arg {
        uint16_t pool_index;
        uint16_t original_pool_index;
        uint32_t tsi;
    };

    void send_updates(struct rte_timer *, void *arguments) {
        struct callback_arg* arg = (struct callback_arg*) arguments;
        uint16_t num_workers = daiet_par.getNumWorkers();
        if (unlikely(known_workers < num_workers)) {
            LOG_DEBUG("not all the workers are known yet.");
            return;
        }
        rte_prefetch0 (rte_pktmbuf_mtod(cache_packet, void *));
        struct ether_hdr* eth = rte_pktmbuf_mtod(cache_packet, struct ether_hdr *);
        struct daiet_hdr* daiet = (struct daiet_hdr *) ((uint8_t *) (eth+1) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
        ps_msg_setup(daiet, arg->pool_index);
        daiet->tsi = arg->tsi;
        daiet->pool_index = arg->original_pool_index;
        daiet->num_aggregated = ps_received_message_counters[arg->pool_index];

        // Allocate pkt burst
        int ret = rte_pktmbuf_alloc_bulk(pool, clone_burst, num_workers);
        if (unlikely(ret < 0))
            LOG_FATAL("Cannot allocate clone burst");

        for (unsigned i = 0; i < num_workers; i++) {

            // Clone packet
            deep_copy_single_segment_pkt(clone_burst[i], cache_packet);

            eth = rte_pktmbuf_mtod(clone_burst[i], struct ether_hdr *);

            // Set dst MAC
            ether_addr_copy(&(ps_workers_ip_to_mac[i].mac), &(eth->d_addr));

            // Set dst IP
            struct ipv4_hdr* ip = (struct ipv4_hdr *) (eth + 1);
            ip->dst_addr = ps_workers_ip_to_mac[i].be_ip;
        }
#ifdef DEBUG
        print_packet(eth, cache_packet->data_len);
#endif

        // Send packet burst
        unsigned sent = 0;
        do {
            sent += rte_eth_tx_burst(dpdk_par.portid, ps_id, clone_burst, num_workers);
        } while (sent < num_workers);

        rte_bitmap_set(bitmap, arg->tsi / num_updates);
        ps_tx += num_workers;
    }

#endif

    void ps_setup() {
#ifdef ALLOW_LOSS
        rte_timer_subsystem_init();
#endif
    }

    void ps_cleanup() {
    }

    int ps(void*) {

        int ret;

        unsigned lcore_id;
        unsigned nb_rx = 0, j = 0, i = 0, sent = 0;

        uint16_t num_workers = daiet_par.getNumWorkers();
        const uint32_t max_num_pending_messages = daiet_par.getMaxNumPendingMessages();
        num_updates = daiet_par.getNumUpdates();
        uint64_t ps_rx = 0;

        string pool_name = "ps_pool";
        struct rte_mbuf** pkts_burst;
        struct rte_mbuf* m;

        struct ether_hdr* eth;
        struct ipv4_hdr * ip;
        struct udp_hdr * udp;
        struct daiet_hdr* daiet;
        uint16_t pool_index = 0, start_pool_index = 0;

        bool packet_not_cached = true;
        uint32_t cache_tsi;
        uint16_t cache_pool_index;

        // Bitmap
        void* bitmap_mem = NULL;
        uint32_t bitmap_size;

        // Get core ID
        lcore_id = rte_lcore_id();
        ps_id = dpdk_data.core_to_thread_id[lcore_id];
        LOG_DEBUG("PS core: " + to_string(lcore_id) + " PS id: " + to_string(ps_id));

        start_pool_index = ps_id * max_num_pending_messages;
        ps_port_be = rte_cpu_to_be_16(daiet_par.getBasePsPort() + ps_id);

        ps_aggregated_messages = (int32_t**) rte_malloc_socket(NULL, max_num_pending_messages * sizeof(int32_t*), RTE_CACHE_LINE_SIZE, rte_socket_id());
        if (ps_aggregated_messages == NULL)
            LOG_FATAL("Failed PS aggregated messages allocation!");

        for (i = 0; i < max_num_pending_messages; i++) {
            ps_aggregated_messages[i] = (int32_t*) rte_zmalloc_socket(NULL, num_updates * sizeof(int32_t), RTE_CACHE_LINE_SIZE, rte_socket_id());
            if (ps_aggregated_messages[i] == NULL)
                LOG_FATAL("Failed PS aggregated messages allocation: element " + to_string(i));
        }

        ps_received_message_counters = (uint32_t*) rte_zmalloc_socket(NULL, max_num_pending_messages * sizeof(uint32_t), RTE_CACHE_LINE_SIZE, rte_socket_id());
        if (ps_received_message_counters == NULL)
            LOG_FATAL("Failed PS aggregated messages allocation!");

        ps_workers_ip_to_mac = (mac_ip_pair*) rte_zmalloc_socket(NULL, num_workers * sizeof(struct mac_ip_pair), RTE_CACHE_LINE_SIZE, rte_socket_id());
        if (ps_workers_ip_to_mac == NULL)
            LOG_FATAL("PS thread: cannot allocate ps_workers_ip_to_mac");

        pkts_burst = (rte_mbuf **) rte_malloc_socket(NULL, dpdk_par.burst_rx * sizeof(struct rte_mbuf*), RTE_CACHE_LINE_SIZE, rte_socket_id());
        if (pkts_burst == NULL)
            LOG_FATAL("PS thread: cannot allocate pkts burst");

        clone_burst = (rte_mbuf **) rte_malloc_socket(NULL, num_workers * sizeof(struct rte_mbuf*), RTE_CACHE_LINE_SIZE, rte_socket_id());
        if (clone_burst == NULL)
            LOG_FATAL("PS thread: cannot allocate clone burst");

        // Init the buffer pool
        pool_name = pool_name + to_string(ps_id);
        pool = rte_pktmbuf_pool_create(pool_name.c_str(), dpdk_par.pool_size, dpdk_par.pool_cache_size, 0, dpdk_data.pool_buffer_size, rte_socket_id());
        if (pool == NULL)
            LOG_FATAL("Cannot init mbuf pool: " + string(rte_strerror(rte_errno)));

#ifdef ALLOW_LOSS
        struct callback_arg arg[max_num_pending_messages];
        uint64_t timer_prev_tsc = 0, timer_cur_tsc;
        const uint64_t timer_cycles = (rte_get_timer_hz() / 1000) * daiet_par.getPsTimeout(); // cycles for daiet_par.getPsTimeout() ms
        struct rte_timer timers[max_num_pending_messages];
        for (i = 0; i < max_num_pending_messages; i++) {
            arg[i].pool_index = i;
            rte_timer_init(&timers[i]);
        }
#endif

        while (!force_quit) {

            nb_rx = rte_eth_rx_burst(dpdk_par.portid, ps_id, pkts_burst, dpdk_par.burst_rx);

            for (j = 0; j < nb_rx; j++) {

                m = pkts_burst[j];

                rte_prefetch0 (rte_pktmbuf_mtod(m, void *));
                eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

#ifdef DEBUG
                daiet = is_daiet_pkt_to_ps(eth, m->data_len);
                if (likely(daiet != NULL)) {
#else
                    daiet = (struct daiet_hdr *) ((uint8_t *) (eth+1) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
#endif

                    if (unlikely(rte_be_to_cpu_16(daiet->tno) > tno)) {
                        tno = rte_be_to_cpu_16(daiet->tno);
                        // new tensor
                        // Initialize bitmap
                        rte_bitmap_free(bitmap);
                        rte_free(bitmap_mem);
                        bitmap_size = rte_bitmap_get_memory_footprint(rte_be_to_cpu_32(daiet->total_num_msgs));
                        if (unlikely(bitmap_size == 0)) {
                            LOG_FATAL("Bitmap failed");
                        }

                        bitmap_mem = rte_malloc_socket("bitmap", bitmap_size, RTE_CACHE_LINE_SIZE, rte_socket_id());
                        if (unlikely(bitmap_mem == NULL)) {
                            LOG_FATAL("Cannot allocate bitmap");
                        }

                        bitmap = rte_bitmap_init(rte_be_to_cpu_32(daiet->total_num_msgs), (uint8_t*) bitmap_mem, bitmap_size);
                        if (unlikely(bitmap == NULL)) {
                            LOG_FATAL("Failed to init bitmap");
                        }
                        rte_bitmap_reset(bitmap);
                    } else if (unlikely(rte_be_to_cpu_16(daiet->tno) < tno)) {
                        // drop packet
                        rte_pktmbuf_free(m);
                        continue;
                    }

                    uint32_t pkt_idx = daiet->tsi / num_updates;

                    if (likely(rte_bitmap_get(bitmap, pkt_idx) == 0)) {
                        ps_rx++;
                        ip = (struct ipv4_hdr *) (eth + 1);
                        udp = (struct udp_hdr *) (ip + 1);

                        pool_index = (rte_be_to_cpu_16(daiet->pool_index) & 0x7FFF) - start_pool_index;


                        bool done_aggregating = ps_aggregate_message(daiet, ip->src_addr, eth->s_addr, pool_index, num_workers);

                        if (unlikely(packet_not_cached)) {
                            // Checksum offload
                            m->l2_len = sizeof(struct ether_hdr);
                            m->l3_len = sizeof(struct ipv4_hdr);
                            m->ol_flags |= daiet_par.getTxFlags();
                            // Set src MAC
                            ether_addr_copy(&(eth->d_addr), &(eth->s_addr));
                            // Set src IP
                            ip->hdr_checksum = 0;
                            ip->src_addr = ip->dst_addr;
                            // Swap ports
                            swap((uint16_t&) (udp->dst_port), (uint16_t&) (udp->src_port));
                            udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, m->ol_flags);
                            cache_packet = rte_pktmbuf_clone(m, pool);
                            if (unlikely(cache_packet == NULL))
                                LOG_FATAL("failed to allocate cache packet");
                            packet_not_cached = false;
                        }


                        if (done_aggregating) {

                            rte_bitmap_set(bitmap, pkt_idx);

#ifdef ALLOW_LOSS
                            rte_timer_stop_sync(&timers[pool_index]);
#endif

                            cache_tsi = daiet->tsi;
                            cache_pool_index = daiet->pool_index;
                            rte_prefetch0 (rte_pktmbuf_mtod(cache_packet, void *));
                            eth = rte_pktmbuf_mtod(cache_packet, struct ether_hdr *);
                            daiet = (struct daiet_hdr *) ((uint8_t *) (eth+1) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
                            ps_msg_setup(daiet, pool_index);
                            daiet->tsi = cache_tsi;
                            daiet->pool_index = cache_pool_index;

#ifdef ALLOW_LOSS
                            daiet->num_aggregated = num_workers;
#endif

                            // Allocate pkt burst
                            ret = rte_pktmbuf_alloc_bulk(pool, clone_burst, num_workers);
                            if (unlikely(ret < 0))
                                LOG_FATAL("Cannot allocate clone burst");

                            for (i = 0; i < num_workers; i++) {

                                // Clone packet
                                deep_copy_single_segment_pkt(clone_burst[i], cache_packet);

                                eth = rte_pktmbuf_mtod(clone_burst[i], struct ether_hdr *);

                                // Set dst MAC
                                ether_addr_copy(&(ps_workers_ip_to_mac[i].mac), &(eth->d_addr));

                                // Set dst IP
                                ip = (struct ipv4_hdr *) (eth + 1);
                                ip->dst_addr = ps_workers_ip_to_mac[i].be_ip;
                            }

                            // Send packet burst
                            sent = 0;
                            do {
                                sent += rte_eth_tx_burst(dpdk_par.portid, ps_id, clone_burst, num_workers);
                            } while (sent < num_workers);

                            ps_tx += num_workers;

                            // Free original packet
                            rte_pktmbuf_free(m);

#ifdef ALLOW_LOSS
                        } else if (ps_received_message_counters[pool_index] == 1) {
                            rte_pktmbuf_free(m);
                            arg[pool_index].tsi = daiet->tsi;
                            arg[pool_index].original_pool_index = daiet->pool_index;
                            rte_timer_reset_sync(&timers[pool_index], timer_cycles, SINGLE, lcore_id, send_updates, &arg[pool_index]);
#endif

                        } else { // done aggregation if
                            // Free original packet
                            rte_pktmbuf_free(m);
                        }

                    } else { // bitmap if
                        // Free original packet
                        rte_pktmbuf_free(m);
                    }
#ifdef DEBUG
                } else {
                    // Free original packet
                    rte_pktmbuf_free(m);
                }
#endif
            }

#ifdef ALLOW_LOSS
            // Check timers
            timer_cur_tsc = rte_get_timer_cycles();
            if (unlikely(timer_cur_tsc - timer_prev_tsc > timer_cycles)) {
                rte_timer_manage();
                timer_prev_tsc = timer_cur_tsc;
            }
#endif

        }

        // Set stats
        pkt_stats.set_ps(ps_id, ps_tx, ps_rx);

        // Cleanup
        rte_bitmap_free(bitmap);
        rte_free(bitmap_mem);
        rte_pktmbuf_free(cache_packet);
        rte_free(clone_burst);
        rte_free(pkts_burst);

        rte_free(ps_workers_ip_to_mac);

        rte_free(ps_received_message_counters);

        for (uint32_t i = 0; i < num_updates; i++) {
            rte_free(ps_aggregated_messages[i]);
        }

        rte_free(ps_aggregated_messages);

        return 0;
    }
}
