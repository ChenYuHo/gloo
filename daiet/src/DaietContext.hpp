/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include <boost/thread.hpp>
#include <atomic>

namespace daiet {

    void *DaietMaster(void *ctx);

    enum TensorUpdateType {
        NONE = 0, INT = 1, FLOAT = 2
    };

    struct TensorUpdate {
            void* ptr;
            int count;
            int32_t id;
            TensorUpdateType type;
    };

    /* Singleton class*/
    class DaietContext {
        public:

            static DaietContext& getInstance() {
                // Guaranteed to be destroyed and instantiated on first use.
                static DaietContext instance;
                return instance;
            }

            DaietContext(DaietContext const&) = delete;
            void operator=(DaietContext const&) = delete;

            void wait_master_ready();
            void set_master_ready();

            TensorUpdate* receive_conversion_job();
            bool send_conversion_job(TensorUpdate*);

            void receive_result(const int32_t);
            bool send_result(const int32_t);
            TensorUpdate* receive_tensor();
            void send_tensor(TensorUpdate*);

            void StartMaster();
            void StopMaster();

            void AllReduceFloat(float*, int);
            void AllReduceInt32(int32_t*, int);

            bool try_daiet(int32_t*, int, int);
            bool try_daiet(float*, int, int);
            bool try_daiet(void*, int, int);

            friend void *DaietMaster(void*);

        private:

            DaietContext();
            virtual ~DaietContext();

            pthread_t masterThread;
            int ret;

            std::atomic_uint_fast32_t tid_counter;
            boost::mutex master_ready_mutex, data_ready_mutex, result_mutex, converter_mutex;
            boost::condition_variable master_ready_event, data_push_event, data_pop_event, result_push_event, result_pop_event, converter_ready_event,
                    converter_job_event;
            bool master_ready;

            // Shared
            bool data_ready, result_empty;
            TensorUpdate* tensor_update_ptr;
            TensorUpdate* conversion_job;
            int32_t result_id;
            // ***

            boost::chrono::milliseconds one_msec;
    };
}

