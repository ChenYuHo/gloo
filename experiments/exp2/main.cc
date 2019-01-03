#include <iostream>
#include <memory>
#include <chrono>
#include <cmath>

#include "gloo/allreduce_halving_doubling.h"
#include "gloo/rendezvous/context.h"
#include "gloo/rendezvous/redis_store.h"
#include "gloo/rendezvous/prefix_store.h"
#include "gloo/transport/tcp/device.h"
#include "gloo/barrier_all_to_one.h"

#include <signal.h>

#include "common.h"

using namespace std;

shared_ptr<gloo::rendezvous::Context> context;

void signal_handler(int signum) {

    if (signum == SIGINT || signum == SIGTERM) {

        cerr << " Signal " << signum << " received!";

        context->daietContext.StopMaster();
    }
}

int main(int argc, char* argv[]) {

    if (argc != 8) {
        cout << " Usage: " << argv[0] << " INTERFACE REDIS_SERVER_IP PREFIX NUM_WORKERS RANK TENSOR_SIZE NUM_ROUNDS" << endl;
        return 0;
    }

    /* Set signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    vector<float, aligned_allocator<float, kBufferAlignment>> data;
    int roundnum = 0;

    float elem = 0.01;

    // GLOO transport
    gloo::transport::tcp::attr attr;
    attr.iface = argv[1];
    auto dev = gloo::transport::tcp::CreateDevice(attr);

    // Rendezvous
    auto redisStore = gloo::rendezvous::RedisStore(argv[2]);
    string prefix = argv[3];
    auto prefixStore = gloo::rendezvous::PrefixStore(prefix, redisStore);

    const int size = atoi(argv[4]);
    const int rank = atoi(argv[5]);
    const int tensor_size = atoi(argv[6]);
    const int num_rounds = atoi(argv[7]);

    // Init data
    data.reserve(tensor_size);
    cout << "-- Tensor initialization" << endl;
    for (int i = 0; i < tensor_size; i++) {
        data.insert(data.begin()+i, elem);
    }
    cout << "---- Ended" << endl;

    vector<float*> ptrs;
    ptrs.push_back(&data[0]);

    int count = data.size();

    // Context
    context = make_shared<gloo::rendezvous::Context>(rank, size);
    context->connectFullMesh(prefixStore, dev);

    auto barrier = make_shared<gloo::BarrierAllToOne>(context);

    barrier->run();

    //Warm up rounds
    for (int i=0; i<10; i++){
        auto allreduce = make_shared<gloo::AllreduceHalvingDoubling<float>>(context, ptrs, count);
        allreduce->run();
    }

    // Start rounds
    for (roundnum = 0; roundnum < num_rounds; roundnum++) {
        // Instantiate the collective algorithm
        auto allreduce = make_shared<gloo::AllreduceHalvingDoubling<float>>(context, ptrs, count);

        cout << "-- Allreduce Round " << roundnum << endl;

        auto begin = chrono::high_resolution_clock::now();
        // Run the algorithm
        allreduce->run();

        auto end = chrono::high_resolution_clock::now();

        cout << "---- Ended" << endl << "#ms " << chrono::duration_cast<chrono::milliseconds>(end - begin).count() << endl;

    }

    cout << "-- Final check" << endl;
    for (int i = 0; i < tensor_size; i++) {
        if (data[i] != elem * powf(size, num_rounds+10)) {
            cout << "---- Failed: index: " << i << " -> received " << data[i] << " instead of " <<  elem * powf(size, num_rounds+10) << endl;
            break;
        }
    }
    cout << "---- Ended" << endl;

    return 0;
}
