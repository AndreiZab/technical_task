#include <vector>


#define ERROR_TOO_LARGE_WINDOW -1
#define ERROR_ZERO_WINDOW      -2
#define ERROR_NO_DATA          -3

#define PERCENT_DIVIATION 10.0f

#include <stdio.h>

typedef struct {
    uint32_t value;
    time_t time_stamp; // 32-bit system 'Year 2038 problem'
}
rx_value_t;

typedef struct {
    uint32_t avg;
    time_t duration_s;
    time_t start_time_s; // 32-bit architecture:  'Year 2038 problem'
}
avg_speed_t;

class Averager {
public:
    Averager(uint32_t avg_time_s) {
        p_avg_time_s = avg_time_s;
        memset(&ultimate_avg, 0, sizeof(ultimate_avg));
    };

    /* Меняем время усреднения в процессе работы */
    void SetAveragingTime(uint32_t avg_time_s) {
        p_avg_time_s = avg_time_s;
    };

    /* Возвращается среднее значение скорости */
    float GetAverageSpeed() {
        float avg_spd = 0;
        time_t duration = 0;
        time_t sum_duration = 0;
        uint32_t avg_time_s = p_avg_time_s;
        time_t curr_time = time(nullptr);

        printf("s:%lu, a:%d\n", small_avg.size(), avg_time_s);

        if (avg_time_s == 0) {
            return ERROR_ZERO_WINDOW;
        }
        if (small_avg.empty() && mid_avg.empty() && large_avg.empty() && ultimate_avg.duration_s == 0) {
            return ERROR_NO_DATA;
        }
        if (window_too_long(curr_time, avg_time_s)) {
            return ERROR_TOO_LARGE_WINDOW;
        }


        auto i = small_avg.size() - 1;
        while ((i < small_avg.size()) &&
            ((small_avg[i].start_time_s + small_avg[i].duration_s) > (curr_time - avg_time_s))) {

            duration = small_avg[i].duration_s;
            if ((duration + sum_duration) > avg_time_s) {
                duration = avg_time_s - sum_duration;
            }

            avg_spd += ((float)small_avg[i].avg / (float)avg_time_s * (float)duration);
            printf("(%ld)", duration);
            sum_duration += duration;

            i--;
        }
        printf(" %d<%d &&   %d\n", i, small_avg.size(), small_avg[i+1].avg);
        return avg_spd;
    };


    /* Добавлем информацию о получении value байтов */
    void AddValue(uint32_t value) {
        avg_speed_t new_elem;
        avg_speed_t *last_elem = &small_avg.back(); // TODO can i do like this?

        // TODO syscall all time - bad solution
        time_t curr_time = time(nullptr);


        // если в эту секунду уже принимали данные, добавляем их к имеющимся
        if (!small_avg.empty() && (curr_time <= last_elem->duration_s + last_elem->start_time_s)) {
            last_elem->avg = (last_elem->avg * last_elem->duration_s + value) / last_elem->duration_s;
        }
        // проверка на то, можно ли добавить элемент в уже созданную ячейку
        // можно если он не сильно отклоняет avg и прошло немного времени
        else if ((!small_avg.empty()) &&
            (num_div_percent((float)last_elem->avg, (float)value, PERCENT_DIVIATION)) &&
            (curr_time <= (last_elem->start_time_s + last_elem->duration_s + 1))) {
            // TODO need overflow protect
            last_elem->avg = (last_elem->avg * last_elem->duration_s + value) / (last_elem->duration_s + 1);
            last_elem->duration_s++;
        }
        // создаем новый элемент
        else {
            new_elem.avg = value;
            new_elem.duration_s = 1;
            new_elem.start_time_s = curr_time;
            small_avg.push_back(new_elem);
        }
    };

private:
    std::vector<avg_speed_t> small_avg;
    std::vector<avg_speed_t> mid_avg;
    std::vector<avg_speed_t> large_avg;
    avg_speed_t ultimate_avg;

    uint32_t p_avg_time_s;

    // Если отклонение dst от src превышает div% то возвращает false
    bool num_div_percent(float src, float dst, float div) {
        bool ret;
        // TODO may be need float
        if (((src > dst) && ((src * (1.0f - div / 100)) < dst)) ||
            ((src < dst) && ((src * (1.0f + div / 100)) > dst)) ||
            (src == dst)) {
            ret = true;
        } else {
            ret = false;
        }
        return ret;
    };
    //  Проверяем не выходит ли запрашиваемое пользователем окно за рамки измеренного нами
    bool window_too_long(time_t curr_time, uint32_t avg_time_s) {
        bool ret;
        if ((ultimate_avg.avg != 0 && ultimate_avg.start_time_s < (curr_time - avg_time_s)) ||
            (!large_avg.empty() && large_avg.front().start_time_s < (curr_time - avg_time_s)) ||
            (!mid_avg.empty()   && mid_avg.front().start_time_s   < (curr_time - avg_time_s)) ||
            (!small_avg.empty() && small_avg.front().start_time_s < (curr_time - avg_time_s))) {
            ret = false;
        } else {

           ret = true;
         }
        return ret;
    };
};
