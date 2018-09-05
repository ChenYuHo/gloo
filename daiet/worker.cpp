/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "worker.hpp"

#include <math.h>

namespace daiet {

#if SAVE_LATENCIES
    rte_atomic64_t* sent_timestamp;
#endif

    float scaling_factor;

    void msg_setup(ClientSendOpLogMsg* new_msg, uint32_t seq_num, TensorUpdate* tensor_update) {

        uint8_t* row_ptr;
        int32_t cell_value_be;
        /*
         bool last_packet=true;
         uint32_t sqn =0;
         // Check msb
         bitset<32> bs (sqn);
         if (bs.test(31)){
         LOG_FATAL("Sequence number out-of-range");
         }

         // If this is the last packet set msb
         if (last_packet)
         sqn= (sqn | 0x80000000);
         */

        new_msg->get_num_updates() = daiet_par.getNumUpdates();
        //new_msg->get_offset_position() = 0;

        /*
         new_msg->get_ack_num() = 3;
         new_msg->get_is_clock() = true;
         new_msg->get_client_id() = 7;
         new_msg->get_version() = 15;
         new_msg->get_bg_clock() = 31;
         new_msg->get_entity_id() = 63;
         new_msg->get_num_tables() = 127;
         new_msg->get_table_id() = 255;
         new_msg->get_update_size() = update_size;
         new_msg->get_num_rows() = 511;
         new_msg->get_row_id() = 16773631;
         */

        // Size
        //new_msg->get_avai_size() = INT_HEADER_SIZE + (daiet_par.getNumUpdates() * 2 * daiet_par.getUpdateSize());

        row_ptr = new_msg->get_first_entry_ptr();

        //LOG_INFO(,"Build message ....\n");
        /*
         int32_t upd1=1, upd2=256;
         for (uint32_t i=0; i<num_updates;i++){

         // Write update
         memcpy((void*)row_ptr, &upd1, update_size);
         row_ptr+=update_size;
         memcpy((void*)row_ptr, &upd2, update_size);
         row_ptr+=update_size;
         upd1++;
         upd2++;
         }
         */
        seq_num *= 32;

        for (uint32_t i = 0; i < daiet_par.getNumUpdates() * 2; i++) {

            //TODO separate functions for FLOAT and INT to avoid this IF for each entry
            if (tensor_update->type==INT)
                cell_value_be = rte_cpu_to_be_32(tensor_update->ptr.int_ptr[seq_num]);
            else if (tensor_update->type==FLOAT)
                cell_value_be = rte_cpu_to_be_32(round((tensor_update->ptr.float_ptr[seq_num])*scaling_factor));

            // Write update
            rte_memcpy((void*) row_ptr, &cell_value_be, sizeof(int32_t));
            row_ptr += sizeof(int32_t);
            seq_num++;
        }
    }

#if SAVE_LATENCIES
    static __rte_always_inline void write_timestamp(uint16_t offset) {

        uint64_t ts = rte_get_timer_cycles();

        rte_atomic64_set(&sent_timestamp[offset], ts);
    }

    static __rte_always_inline void save_latency(uint16_t offset, int64_t num_recv) {

        uint64_t ts = rte_get_timer_cycles();

        latencies[num_recv] = ts - rte_atomic64_exchange(reinterpret_cast<volatile uint64_t*>(&(sent_timestamp[offset].cnt)), ts);
    }
#endif

    static __rte_always_inline int aggregate_msg(ClientSendOpLogMsg& msg, uint num_workers, TensorUpdate* tensorUpdate) {
        int sum = 0;
        int32_t seqn = msg.get_row_id();
        seqn*=32;

        uint8_t* row_ptr = msg.get_first_entry_ptr();
        for (int i = 0; i < msg.get_num_updates() * 2; i++) {

            //TODO separate functions for FLOAT and INT to avoid this IF for each entry
            if (tensorUpdate->type==INT)
                tensorUpdate->ptr.int_ptr[seqn] = int32_t(rte_be_to_cpu_32(*(reinterpret_cast<int32_t*>(row_ptr))))/(int32_t)num_workers;
            else if (tensorUpdate->type==FLOAT)
                tensorUpdate->ptr.float_ptr[seqn] = (int32_t(rte_be_to_cpu_32(*(reinterpret_cast<int32_t*>(row_ptr))))/(int32_t)num_workers)/scaling_factor;

            row_ptr += sizeof(int32_t);
            seqn++;
        }

        return sum;
    }

    static __rte_always_inline void reset_pkt(struct ether_hdr * eth, ClientSendOpLogMsg* msg, unsigned portid, uint16_t offset, uint32_t seq_num, TensorUpdate* tensorUpdate) {

        int32_t cell_value_be;

        struct ipv4_hdr * const ip_hdr = (struct ipv4_hdr *) (&eth[1]);
        struct udp_hdr * const udp_hdr = (struct udp_hdr *) (&ip_hdr[1]);

        // Set MACs
        ether_addr_copy(&(eth->s_addr), &(eth->d_addr));
        rte_eth_macaddr_get(portid, &eth->s_addr);

        // Set IPs
        ip_hdr->dst_addr = ip_hdr->src_addr;
        ip_hdr->src_addr = daiet_par.getWorkerIpBe();

        // Set UDP
        udp_hdr->dst_port = daiet_par.getPsPortBe();
        udp_hdr->src_port = daiet_par.getWorkerPortBe();

        msg->get_row_id() = seq_num;
        msg->get_seq_num()=0;

        seq_num *= 32;

        uint8_t * row_ptr = msg->get_first_entry_ptr();

        for (uint32_t i = 0; i < daiet_par.getNumUpdates() * 2; i++) {

            //TODO separate functions for FLOAT and INT to avoid this IF for each entry
            if (tensorUpdate->type==INT)
                cell_value_be = rte_cpu_to_be_32(tensorUpdate->ptr.int_ptr[seq_num]);
            else if (tensorUpdate->type==FLOAT)
                cell_value_be = rte_cpu_to_be_32(round((tensorUpdate->ptr.float_ptr[seq_num])*scaling_factor));

            // Write update
            rte_memcpy((void*) row_ptr, &cell_value_be, sizeof(int32_t));
            row_ptr += sizeof(int32_t);
            seq_num++;
        }
    }

    void build_pkt(rte_mbuf* m, unsigned portid, uint16_t offset, uint32_t seq_num, TensorUpdate* tensor_update) {

        struct ether_hdr *eth;
        struct ipv4_hdr *ip_hdr;
        struct udp_hdr *udp_hdr;
        void *tmp;

        m->data_len = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + daiet_par.getPayloadSize();
        m->pkt_len = m->data_len;

        rte_prefetch0(rte_pktmbuf_mtod(m, void *));
        eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

        // Set MAC addresses
        tmp = &eth->d_addr.addr_bytes[0];
        *((uint64_t *) tmp) = daiet_par.getPsMacBe(offset); // Changes the first 2B of the src address too
        rte_eth_macaddr_get(portid, &eth->s_addr);

        // Set ethertype
        eth->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

        // IP header
        ip_hdr = (struct ipv4_hdr *) (&eth[1]);
        ip_hdr->version_ihl = 0x45;
        ip_hdr->time_to_live = 128;
        ip_hdr->total_length = rte_cpu_to_be_16(sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + daiet_par.getPayloadSize());
        ip_hdr->next_proto_id = IPPROTO_UDP;
        ip_hdr->src_addr = daiet_par.getWorkerIpBe();
        ip_hdr->dst_addr = daiet_par.getPsIpBe(offset);

        // UDP header
        udp_hdr = (struct udp_hdr *) (&ip_hdr[1]);
        udp_hdr->src_port = daiet_par.getWorkerPortBe();
        udp_hdr->dst_port = daiet_par.getPsPortBe();
        udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct udp_hdr) + daiet_par.getPayloadSize());

        // ClientSendOpLogMsg

        ClientSendOpLogMsg msg((void*) (&udp_hdr[1]));
        msg_setup(&msg, seq_num, tensor_update);
        msg.get_offset_position() = rte_cpu_to_be_16(offset);
        msg.get_row_id() = seq_num;
    }

    void worker_setup() {

        sent_message_counters = new rte_atomic32_t[daiet_par.getMaxNumPendingMessages()];
        for (int i = 0; i < daiet_par.getMaxNumPendingMessages(); i++) {
            rte_atomic32_init(&sent_message_counters[i]);
        }

        scaling_factor = daiet_par.getScalingFactor();

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

    int worker(BlockingQueue<TensorUpdate*> &in_queue, BlockingQueue<TensorUpdate*> &out_queue) {

        volatile int rx_pkts =0;
        uint max_num_pending_messages = daiet_par.getMaxNumPendingMessages();
        uint num_workers = daiet_par.getNumWorkers();

        uint max_num_msgs = daiet_par.getMaxNumMsgs();

        const uint32_t max_seq_num = daiet_par.getMaxSeqNum();

        int ret;

        int lower_bound_num_updates;
        uint32_t upper_bound_num_updates;
        uint32_t last_update_index;
        int32_t tmp_rx_msg_ctr;

        int burst_size;
        unsigned lcore_id;
        unsigned nb_rx = 0, j = 0;
        uint32_t worker_id;

#if SAVE_LATENCIES
        int64_t lat_indx = 0;
#endif

        struct rte_mbuf **pkts_burst;
        struct rte_mbuf* m;

        struct ether_hdr *eth;
        ClientSendOpLogMsg* msg = new ClientSendOpLogMsg((void*) NULL);

        uint32_t seq_num = 0;
        uint16_t offset = 0;

        // Get core ID
        lcore_id = rte_lcore_id();
        worker_id = core_to_workers_ids[lcore_id];
        LOG_DEBUG("Worker core: " + to_string(lcore_id) + " worker id: " + to_string(worker_id));

        pkts_burst = (rte_mbuf **) rte_malloc_socket(NULL, max_num_pending_messages * sizeof(struct rte_mbuf*), RTE_CACHE_LINE_SIZE, rte_socket_id());
        if (unlikely(pkts_burst == NULL))
            LOG_FATAL("Worker thread: cannot allocate pkts burst");

        TensorUpdate* tuptr;

        while (!force_quit) {
            tuptr = in_queue.pop();

            if (tuptr==NULL)
                continue;

            memset(sent_message_counters, 0, daiet_par.getMaxNumPendingMessages()*sizeof(*sent_message_counters));

            seq_num = 0;
            offset = 0;

            rx_pkts = 0;

            max_num_msgs=tuptr->count/32;

            lower_bound_num_updates = max_num_msgs / max_num_pending_messages;
            upper_bound_num_updates = lower_bound_num_updates + 1;
            uint32_t last_update_index = max_num_msgs % max_num_pending_messages;
            int32_t tmp_rx_msg_ctr;

#if !COLOCATED
            // Only on master core
            if (lcore_id == rte_get_master_lcore())
#endif
                    {
                // Send first pkt burst
                burst_size = max_num_msgs < max_num_pending_messages ? max_num_msgs : max_num_pending_messages;
                // Allocate pkt burst

                ret = rte_pktmbuf_alloc_bulk(dpdk_data.pool, pkts_burst, burst_size);
                if (unlikely(ret < 0))
                    LOG_FATAL("Cannot allocate pkts burst");

                for (j = 0; j < burst_size; j++) {
                    m = pkts_burst[j];
                    build_pkt(m, dpdk_par.portid, offset, seq_num, tuptr);
                    seq_num++;

                    rte_atomic32_inc(&sent_message_counters[offset]);

#if SAVE_LATENCIES
                    write_timestamp(offset);
#endif

                    offset++;
                }

                // Transmit the packet burst
                do {
                    ret = rte_ring_enqueue_bulk(dpdk_data.w_ring_tx, (void **) pkts_burst, burst_size, NULL);
                } while (ret == 0);

                rte_atomic64_add(&pkt_stats.w_tx, max_num_pending_messages);
            }

            while (rx_pkts<max_num_msgs) {

                // Read packet from RX ring
                nb_rx = rte_ring_dequeue_burst(dpdk_data.w_ring_rx, (void **) pkts_burst, dpdk_par.burst_size_worker, NULL);

                for (j = 0; j < nb_rx; j++) {

                    m = pkts_burst[j];

                    rte_prefetch0(rte_pktmbuf_mtod(m, void *));
                    eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

#if COLOCATED
                    msg->Reset((void*) ((uint8_t *) (&eth[1]) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr)));
#else
                    if (likely(is_daiet_pkt(eth, m->data_len, msg))) {
#endif

                        offset = rte_be_to_cpu_16(msg->get_offset_position());

                        rte_atomic64_inc(&pkt_stats.w_rx);

#if SAVE_LATENCIES
                        // Save latency
                        save_latency(offset, lat_indx);

                        lat_indx += 1;
#endif

                        rx_pkts++;
                        aggregate_msg(*msg, num_workers, tuptr);

                        tmp_rx_msg_ctr = rte_atomic32_read(&sent_message_counters[offset]);
                        /*Check if the offset has been sent enough times */
                        if (likely(tmp_rx_msg_ctr < lower_bound_num_updates || (tmp_rx_msg_ctr < upper_bound_num_updates && offset < last_update_index))) {

                            //Resend the packet
                            seq_num = max_num_pending_messages*tmp_rx_msg_ctr+offset;
                            reset_pkt(eth, msg, dpdk_par.portid, offset, seq_num, tuptr);

                            ret = rte_ring_enqueue(dpdk_data.w_ring_tx, m);
                            if (unlikely(ret < 0))
                                LOG_FATAL("Cannot enqueue one packet");

                            rte_atomic64_inc(&pkt_stats.w_tx);
                            rte_atomic32_inc(&sent_message_counters[offset]);
                        }

#if !COLOCATED
                    } else {
                        // Free original packet
                        rte_pktmbuf_free(m);
                    }
#endif
                }
            }
            // DONE UPDATE
            out_queue.push(tuptr);
        }
        // Cleanup
        rte_free(pkts_burst);
        // Cleanup
        delete msg;
        msg = 0;

        return 0;
    }
}
