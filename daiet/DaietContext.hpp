/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#pragma once

#include "gloo/common/error.h"
#include "gloo/common/logging.h"

#include <boost/thread.hpp>
#include <deque>

namespace daiet {

    void *DaietMaster(void *ctx);

    template<typename T>
        class BlockingQueue {
            public:
                explicit BlockingQueue(size_t capacity = 1);

                void push(T elem);
                T pop();

            private:
                boost::mutex _mutex;
                boost::condition_variable _push_event, _pop_event;
                std::deque<T> _buffer;
                size_t _capacity;
        };

        enum TensorUpdateType {
            INT = 1,
            FLOAT = 2
        };

        struct TensorUpdate {
                union {
                        float* float_ptr;
                        int32_t* int_ptr;
                } ptr;
                int count;

                TensorUpdateType type;
        };

    class DaietContext {
        public:
            DaietContext();
            virtual ~DaietContext();

            void StopMaster();

            void AllReduceFloat(float*, int count);
            void AllReduceInt32(int32_t*, int count);

            friend void *DaietMaster(void *ctx);
        private:
            pthread_t masterThread;
            int ret;
            BlockingQueue<TensorUpdate*> in_queue;
            BlockingQueue<TensorUpdate*> out_queue;
    };
}

