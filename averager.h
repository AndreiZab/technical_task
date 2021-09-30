#include <vector>


#define ERROR_TOO_LARGE_WINDOW -1
#define ERROR_ZERO_WINDOW      -2
#define ERROR_NO_DATA          -3

#include <stdio.h>

typedef struct {
    uint32_t value;
    time_t time_stamp; // 32-bit system 'Year 2038 problem'
}
rx_value_t;

class Averager {
public:
    Averager(uint32_t avg_time_s) {
        p_avg_time_s = avg_time_s;
    };

    /* Меняем время усреднения в процессе работы */
    void SetAveragingTime(uint32_t avg_time_s) {
        p_avg_time_s = avg_time_s;
    };

    /* Возвращается среднее значение скорости */
    float GetAverageSpeed() {
        float n = 1;
        float avg_spd = -1;
        uint32_t avg_time_s = p_avg_time_s;
        time_t curr_time = time(nullptr);

        printf("s:%lu, a:%d\n", value_vector.size(), avg_time_s);

        if (value_vector.empty()) {
            return ERROR_NO_DATA;
        }
        if (!value_vector.empty() && value_vector[0].time_stamp > (curr_time - avg_time_s)) {
            return ERROR_TOO_LARGE_WINDOW;
        }
        if (avg_time_s == 0) {
            return ERROR_ZERO_WINDOW;
        }


        auto i = value_vector.size() - 1;
        while (value_vector[i].time_stamp > (curr_time - avg_time_s)) {
            avg_spd = avg_spd + ((float)value_vector[i].value - avg_spd) / n;
            n++;
            i--;
        }

//        avg_spd = ((float)sum)/((float)avg_time_s);
        return avg_spd;
    };

    /* Добавлем информацию о получении value байтов */
    void AddValue(uint32_t value) {
        rx_value_t rx_value;

        // TODO syscall all time - bad solution
        rx_value.time_stamp = time(nullptr);
        rx_value.value = value;

        // all value in one sec fits into 1 element
        if (!value_vector.empty() && value_vector.back().time_stamp == rx_value.time_stamp ) {
            // overflow protect
            if ((value_vector.back().value + value) >= value) {
                value_vector.back().value += value;
            }
            else {
                printf("1: %u,2: %u\n", value_vector.back().value, value);
                value_vector.back().value = UINT32_MAX;
            }
        }
        else {
            value_vector.push_back(rx_value);
        }
    };

private:
    std::vector<rx_value_t> value_vector;
    uint32_t p_avg_time_s;
};
