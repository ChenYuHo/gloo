/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "DaietContext.hpp"
#include "daiet.hpp"
#include "utils.hpp"

namespace daiet {

    void *DaietMaster(void *ctx) {

    DaietContext* d_ctx_ptr = (DaietContext *)ctx;

    string corestr = "daiet -c daiet.cfg";

    vector<string> par_vec = split(corestr);
    int args_c = par_vec.size();
    char* args[args_c];
    char* args_ptr[args_c];

    for (vector<string>::size_type i = 0; i != par_vec.size(); i++) {
        args[i] = new char[par_vec[i].size() + 1];
        args_ptr[i] = args[i];
        strcpy(args[i], par_vec[i].c_str());
    }

    d_ctx_ptr->ret = master(args_c, args, d_ctx_ptr->in_queue, d_ctx_ptr->out_queue);
    return NULL;
    }

    DaietContext::DaietContext() {

        /* Launch dpdk master thread */
        if (pthread_create(&masterThread, NULL, DaietMaster, this))
            GLOO_THROW("Error starting master dpdk thread");
    }

    void DaietContext::StopMaster() {

        force_quit = true;

        int join_ret = pthread_join(masterThread, NULL);
        if(join_ret)
            GLOO_THROW("Error joining master dpdk thread: returned ", join_ret);

        if (this->ret < 0)
            GLOO_THROW("Master dpdk thread returned ", this->ret);
    }

    DaietContext::~DaietContext() {

        StopMaster();
    }

    void DaietContext::AllReduceFloat(float* ptr, int count){

        TensorUpdate tu;
        TensorUpdate* tuptr;;
        tu.ptr.float_ptr=ptr;
        tu.count=count;
        tu.type=FLOAT;
        in_queue.push(&tu);

        do {
            tuptr = out_queue.pop();
        } while(tuptr==NULL);

        tu=*tuptr;
    }

    void DaietContext::AllReduceInt32(int32_t* ptr, int count){
        TensorUpdate tu;
        TensorUpdate* tuptr;;
        tu.ptr.int_ptr=ptr;
        tu.count=count;
        tu.type=INT;
        in_queue.push(&tu);

        do {
            tuptr = out_queue.pop();
        } while(tuptr==NULL);

        tu=*tuptr;
    }

    template<typename T>
    BlockingQueue<T>::BlockingQueue(size_t capacity) :
            _buffer(), _capacity(capacity) {
        assert(capacity > 0);
    }

    template<typename T>
    void BlockingQueue<T>::push(T elem) {
        boost::unique_lock<boost::mutex> lock(_mutex);
        _pop_event.wait(lock, [&] {return _buffer.size() < _capacity;});
        _buffer.push_back(elem);
        _push_event.notify_one();
    }

    template<typename T>
    T BlockingQueue<T>::pop() {
        boost::chrono::microseconds one_usec(1);

        boost::unique_lock<boost::mutex> lock(_mutex);
        if (_push_event.wait_for(lock, one_usec, [&] {return _buffer.size() > 0;}) == false)
            return NULL;

        T elem = _buffer.front();
        _buffer.pop_front();
        _pop_event.notify_one();
        return elem;
    }
}
