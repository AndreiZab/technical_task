#include <iostream>
#include <cstdint>
#include <unistd.h>

#include "cmake-build-debug/averager.h"


#define TARGET_WINDOW_S 5



int main() {

    uint64_t elem_summ = 0;
    Averager network(0);
    network.SetAveragingTime(TARGET_WINDOW_S);

    for (int i = 0; i < 60; i++){
        uint32_t x = (uint32_t) rand()% 5000;


        network.AddValue(x);

        usleep(100000);
        if ((60 - i) <= (TARGET_WINDOW_S*10)) {
            elem_summ += x;
        }
    }
    printf("avg_speed:%f (%f)\n", network.GetAverageSpeed(), (float)elem_summ/(float)TARGET_WINDOW_S);

    return 0;
}
