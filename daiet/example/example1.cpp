#include <iostream>
#include <fstream>
#include "DaietContext.hpp" 
#include <signal.h>
#include "common.hpp" 

using namespace daiet;
using namespace std;

int main(){

DaietContext ctx;

int count= 131072*32;
int32_t* p = new int32_t[count];

for (int jj=1; jj<=5; jj++){
for (int i=0; i<count; i++)
    p[i]=-(jj*i);

ctx.AllReduceInt32(p, count);
std::cout<<"done round " <<jj<<std::endl;
}

float* fp = new float[count];

for (int jj=1; jj<=4; jj++){
for (int i=0; i<count; i++)
    fp[i]=-0.1*(jj*i);

ctx.AllReduceFloat(fp, count);
std::cout<<"done round " <<jj<<std::endl;
}

ofstream myfile;
myfile.open ("example.txt");
for (int i=0; i<count; i++){
    myfile<<fp[i]<<endl;
}
myfile.close();
return 0;
}
