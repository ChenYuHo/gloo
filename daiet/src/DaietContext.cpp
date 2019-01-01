/**
 * DAIET project
 * author: amedeo.sapio@kaust.edu.sa
 */

#include "DaietContext.hpp"
#include "daiet.hpp"
#include "utils.hpp"
#include "gloo/common/error.h"

namespace daiet {

    void* DaietMaster(void *ctx) {

        DaietContext* d_ctx_ptr = (DaietContext *) ctx;

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

        d_ctx_ptr->ret = master(args_c, args, d_ctx_ptr);

        // Cleanup
        for (vector<string>::size_type i = 0; i != par_vec.size(); i++) {
            delete[] args_ptr[i];
        }

        return NULL;
    }

    DaietContext::DaietContext() :
            master_ready(false), data_ready(false), result_empty(true), tensor_update_ptr(NULL), conversion_job(NULL), result_id(0), one_msec(1) {

        tid_counter.store(0);
        StartMaster();
    }

    DaietContext::~DaietContext() {

        StopMaster();
    }

    void DaietContext::wait_master_ready() {
        boost::unique_lock<boost::mutex> lock(master_ready_mutex);

        while (!master_ready)
            master_ready_event.wait(lock);
    }

    void DaietContext::set_master_ready() {

        boost::unique_lock<boost::mutex> lock(master_ready_mutex);

        master_ready = true;
        master_ready_event.notify_one();
    }

    void DaietContext::send_tensor(TensorUpdate* tuptr) {
        boost::unique_lock<boost::mutex> lock(data_ready_mutex);

        while (data_ready)
            data_pop_event.wait(lock);

        tensor_update_ptr = tuptr;
        data_ready = true;
        data_push_event.notify_one();
    }

    TensorUpdate* DaietContext::receive_tensor() {
        boost::unique_lock<boost::mutex> lock(data_ready_mutex);

        while (!data_ready) {
            if (data_push_event.wait_for(lock, one_msec) == boost::cv_status::timeout)
                return NULL;
        }

        TensorUpdate* tuptr = tensor_update_ptr;
        tensor_update_ptr = NULL;
        data_ready = false;
        data_pop_event.notify_one();
        return tuptr;
    }

    bool DaietContext::send_result(const int32_t rid) {
        boost::unique_lock<boost::mutex> lock(result_mutex);

        while (!result_empty) {
            if (result_pop_event.wait_for(lock, one_msec) == boost::cv_status::timeout)
                return false;
        }

        result_id = rid;
        result_empty = false;
        result_push_event.notify_all();

        return true;
    }

    void DaietContext::receive_result(const int32_t rid) {
        boost::unique_lock<boost::mutex> lock(result_mutex);

        while (result_id != rid)
            result_push_event.wait(lock);

        result_empty = true;
        result_id = 0;

        result_pop_event.notify_one();
    }

    bool DaietContext::send_conversion_job(TensorUpdate* tuptr) {

        boost::unique_lock<boost::mutex> lock(converter_mutex);

        while (conversion_job != NULL) {
            if (converter_ready_event.wait_for(lock, one_msec) == boost::cv_status::timeout)
                return false;
        }

        conversion_job = tuptr;
        converter_job_event.notify_one();

        return true;
    }

    TensorUpdate* DaietContext::receive_conversion_job() {
        boost::unique_lock<boost::mutex> lock(converter_mutex);

        while (conversion_job == NULL) {
            if (converter_job_event.wait_for(lock, one_msec) == boost::cv_status::timeout)
                return NULL;
        }

        TensorUpdate* tpp = conversion_job;

        conversion_job = NULL;

        converter_ready_event.notify_one();

        return tpp;
    }

    void DaietContext::StartMaster() {

        /* Launch dpdk master thread */
        if (pthread_create(&masterThread, NULL, DaietMaster, this))
            GLOO_THROW("Error starting master dpdk thread");

        //Wait for EAL setup
        wait_master_ready();
    }

    void DaietContext::StopMaster() {

        force_quit = true;

        int join_ret = pthread_join(masterThread, NULL);
        if (join_ret)
            GLOO_THROW("Error joining master dpdk thread: returned ", join_ret);

        if (this->ret < 0)
            GLOO_THROW("Master dpdk thread returned ", this->ret);

    }

    void DaietContext::AllReduceFloat(float* ptr, int count) {

        int32_t tensor_id = tid_counter.fetch_add(1)+1;
        TensorUpdate tu;
        tu.ptr = ptr;
        tu.count = count;
        tu.id = tensor_id;
        tu.type = FLOAT;

        send_tensor(&tu);
        receive_result(tensor_id);
    }

    void DaietContext::AllReduceInt32(int32_t* ptr, int count) {

        int32_t tensor_id = tid_counter.fetch_add(1)+1;
        TensorUpdate tu;
        tu.ptr = ptr;
        tu.count = count;
        tu.id = tensor_id;
        tu.type = INT;

        send_tensor(&tu);
        receive_result(tensor_id);
    }

    bool DaietContext::try_daiet(int32_t* ptr, int count, int fn_) {
        if (fn_ == 1) { //sum

            AllReduceInt32(ptr, count);

            return true;
        }

        return false;
    }

    bool DaietContext::try_daiet(float* ptr, int count, int fn_) {
        if (fn_ == 1) { //sum

            AllReduceFloat(ptr, count);

            return true;
        }

        return false;
    }

    bool DaietContext::try_daiet(__attribute__((unused)) void* ptr, __attribute__((unused)) int count, __attribute__((unused)) int fn_) {

        return false;
    }
}
