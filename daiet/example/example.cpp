#include <iostream>
#include <fstream>
#include <signal.h>
#include <chrono>
#include <daiet/DaietContext.hpp>

using namespace daiet;
using namespace std;

int main() {

    DaietContext ctx;

    int count = 10485760 * 32;
    int num_workers = 2;
    int faulty = 0;

    int32_t* p = new int32_t[count];

    for (int jj = 1; jj <= 5; jj++) {

        std::cout << "INT round " << jj << std::endl;

        faulty = 0;

        for (int i = 0; i < count; i++)
            p[i] = -(jj * i);

        auto begin = std::chrono::high_resolution_clock::now();
        ctx.AllReduceInt32(p, count);
        auto end = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < count; i++) {
            if (p[i] != -(jj * i * num_workers))
                faulty++;
        }

        std::cout << "Done INT round " << jj << ": Faulty: " << faulty << " Time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
                << " ns" << std::endl;
    }

    float* fp = new float[count];

    for (int jj = 1; jj <= 4; jj++) {

        std::cout << "FLOAT round " << jj << std::endl;

        faulty = 0;

        for (int i = 0; i < count; i++)
            fp[i] = -0.1 * (jj * i);

        auto begin = std::chrono::high_resolution_clock::now();
        ctx.AllReduceFloat(fp, count);
        auto end = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < count; i++) {
            if (fp[i] != -(jj * i * num_workers))
                faulty++;
        }

        std::cout << "Done FLOAT round " << jj << ": Faulty: " << faulty << " Time: "
                << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() << " ns" << std::endl;
    }

    /*
     ofstream myfile;
     myfile.open("example.txt");
     for (int i = 0; i < count; i++) {
     myfile << fp[i] << endl;
     }
     myfile.close();
     */
    exit(EXIT_SUCCESS);
}
