#include <iostream>
#include <memory>
#include <chrono>

#include "gloo/allreduce_halving_doubling.h"
#include "gloo/rendezvous/context.h"
#include "gloo/rendezvous/redis_store.h"
#include "gloo/rendezvous/prefix_store.h"
#include "gloo/transport/tcp/device.h"

#include <signal.h>

using namespace std;

shared_ptr<gloo::rendezvous::Context> context;
vector<int> data;
int roundnum;
volatile int wait=false;

void signal_handler(int signum) {

    if (signum == SIGINT || signum == SIGTERM) {

        if (!wait){

            wait=true;
            cerr << " Signal " << signum << " received!";

            context->daietContext.StopMaster();

            // Init data
            cerr << "-- Tensor Re-initialization - round " << roundnum << endl;

            for (int i = 0; i < data.size(); i++) {
                data[i] = 1;
            }

            roundnum--;
            cerr << "---- ended" << endl << flush;
        }
        else {
            context->daietContext.StartMaster();

            cerr << " Restarting from round " << roundnum;
            wait=false;
        }
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

    // GLOO transport
    gloo::transport::tcp::attr attr;
    attr.iface = argv[1];
    attr.ai_family = AF_UNSPEC;
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
        data.insert(data.begin()+i, 1);
    }
    cout << "---- ended" << endl;

    vector<int*> ptrs;
    ptrs.push_back(&data[0]);

    int count = data.size();

    // Context
    context = make_shared<gloo::rendezvous::Context>(rank, size);
    context->connectFullMesh(prefixStore, dev);

    // Start rounds
    for (roundnum = 0; roundnum < num_rounds; roundnum++) {
        // Instantiate the collective algorithm.
        auto allreduce = make_shared<gloo::AllreduceHalvingDoubling<int>>(context, ptrs, count);

        cout << "-- Allreduce Round " << roundnum << endl;

        auto begin = chrono::high_resolution_clock::now();
        // Run the algorithm.
        allreduce->run();

        auto end = chrono::high_resolution_clock::now();

        if (wait)
            cout << "---- Round " << roundnum << " aborted!";
        else
            cout << "---- Ended" << endl << "#ms " << chrono::duration_cast<chrono::milliseconds>(end - begin).count() << endl;

        while (wait);
    }

    return 0;
}
