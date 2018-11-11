#include <iostream>
#include <fstream>
#include <signal.h>
#include <chrono>
#include <thread> 
#include <daiet/DaietContext.hpp>

using namespace daiet;
using namespace std;

    DaietContext* ctx;

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        cout << " Signal " << signum << " received, preparing to exit...";
        delete ctx;
        exit(EXIT_SUCCESS);
    }
}

int main() {

    ctx = new DaietContext();
    sleep(2);

    int count = 10485760 * 32;
    int num_rounds = 5;
    int num_workers = 2;
    int faulty = 0;

    int32_t* p = new int32_t[count];

    /* Set signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    for (int jj = 1; jj <= num_rounds; jj++) {

        std::cout << "INT round " << jj << std::endl;

        faulty = 0;

        for (int i = 0; i < count; i++)
            p[i] = -(jj * i);

        auto begin = std::chrono::high_resolution_clock::now();
        if (!ctx->try_daiet(p, count,1)){
            cout << "Daiet failed";
            exit(EXIT_FAILURE);
        }
        auto end = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < count; i++) {
            if (p[i] != -(jj * i * num_workers))
                faulty++;
        }

        std::cout << "Done INT round " << jj << ": Faulty: " << faulty << " Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()
                << " ms" << std::endl;
    }

    float* fp = new float[count];

    for (int jj = 1; jj <= num_rounds; jj++) {

        std::cout << "FLOAT round " << jj << std::endl;

        faulty = 0;

        for (int i = 0; i < count; i++)
            fp[i] = -0.1 * (jj * i);

        auto begin = std::chrono::high_resolution_clock::now();
        if (!ctx->try_daiet(fp, count,1)){
            cout << "Daiet failed";
            exit(EXIT_FAILURE);
        }

        auto end = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < count; i++) {
            if (fp[i] != -(jj * i * num_workers))
                faulty++;
        }

        std::cout << "Done FLOAT round " << jj << ": Faulty: " << faulty << " Time: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms" << std::endl;
    }

    /*
     ofstream myfile;
     myfile.open("example.txt");
     for (int i = 0; i < count; i++) {
     myfile << fp[i] << endl;
     }
     myfile.close();
     */

    delete ctx;
    exit(EXIT_SUCCESS);
}
