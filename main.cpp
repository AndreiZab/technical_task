#include <iostream>
#include <stdint.h>
#include "cmake-build-debug/averager.h"


int main() {

    uint32_t rx_byte = 0;
    Averager network(0);
    network.SetAveragingTime(10);

    while(1) {
        std::cin >> rx_byte;
        network.AddValue(rx_byte);
        printf("avg_speed:%f\n", network.GetAverageSpeed());
    }

    return 0;
}
