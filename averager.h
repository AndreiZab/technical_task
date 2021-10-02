#include <vector>


#define ERROR_TOO_LARGE_WINDOW -1
#define ERROR_ZERO_WINDOW      -2
#define ERROR_NO_DATA          -3

#define PERCENT_DIVIATION 10.0f

#define VEC_TARGET_SIZE 5

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

        // обход всех векторов с сохраненными данными
        return calc_avg_spd(curr_time, avg_time_s);
    };


    /* Добавлем информацию о получении value байтов */
    void AddValue(uint32_t value) {
        avg_speed_t new_elem;
        avg_speed_t *last_elem = &small_avg.back(); // TODO can i do like this?

        // TODO syscall all time - bad solution
        time_t curr_time = time(nullptr);

        // если в эту секунду уже принимали данные, добавляем их к имеющимся
        if (!small_avg.empty() && (curr_time <= last_elem->duration_s + last_elem->start_time_s - 1)) {
//            printf("ADD: %ld(%d)\n", curr_time, value);
            last_elem->avg = (last_elem->avg * last_elem->duration_s + value) / last_elem->duration_s;
        }
        // проверка на то, можно ли добавить элемент в уже созданную ячейку
        // можно если он не сильно отклоняет avg и прошло немного времени
        else if ((!small_avg.empty()) &&
            (num_div_percent((float)last_elem->avg, (float)value, PERCENT_DIVIATION)) &&
            (curr_time <= (last_elem->start_time_s + last_elem->duration_s + 1))) {
            // TODO need overflow protect
//            printf("D++: %ld(%d)\n", curr_time, value);

            last_elem->avg = (last_elem->avg * last_elem->duration_s + value) / (last_elem->duration_s + 1);
            last_elem->duration_s++;
        }
        // создаем новый элемент
        else {
//            printf("NEW: %ld(%d)\n", curr_time, value);

            new_elem.avg = value;
            new_elem.duration_s = 1;
            new_elem.start_time_s = curr_time;
            small_avg.push_back(new_elem);
        }

        if (small_avg.size() > VEC_TARGET_SIZE) {
            average_and_transfer_data(small_avg, mid_avg);
            printf("go!%d\n",mid_avg[0].avg);
        }
        if (mid_avg.size() > VEC_TARGET_SIZE) {
            average_and_transfer_data(mid_avg, large_avg);
        }
        if (large_avg.size() > VEC_TARGET_SIZE) {
            avg_speed_t src_sum;
            uint32_t full_src_duration =
                    large_avg.back().duration_s + large_avg.back().start_time_s - large_avg.front().start_time_s;
            memset(&src_sum, 0, sizeof(src_sum));

            for (auto it = begin (large_avg); it != end (large_avg); it++) {
                src_sum.avg += it->avg * it->duration_s / full_src_duration;
            }
            src_sum.start_time_s = large_avg.front().start_time_s;
            src_sum.duration_s = full_src_duration;

            ultimate_avg.avg = ((src_sum.duration_s * src_sum.avg)
                              + (ultimate_avg.duration_s * ultimate_avg.avg))
                             / (src_sum.duration_s + ultimate_avg.duration_s);
            ultimate_avg.duration_s += src_sum.duration_s;

            large_avg.clear();
        }
    };

private:
    std::vector<avg_speed_t> small_avg;
    std::vector<avg_speed_t> mid_avg;
    std::vector<avg_speed_t> large_avg;
    avg_speed_t ultimate_avg;

    uint32_t p_avg_time_s;

    // Если отклонение dst от src превышает div% то возвращает false
    bool num_div_percent(const float src, const float dst, const float div) {
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
    bool window_too_long(const time_t curr_time, const uint32_t avg_time_s) {
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

    void sum_elem(const std::vector<avg_speed_t>& vec,
                  const time_t curr_time, const uint32_t avg_time_s,
                  float* avg_spd, time_t* sum_duration) {
        time_t duration = 0;

        if (!vec.empty()) {
            auto i = vec.size() - 1;
            while ((i <= vec.size()) &&
                   ((vec[i].start_time_s + vec[i].duration_s) > (curr_time - avg_time_s))) {

                duration = vec[i].duration_s;
                if ((duration + (*sum_duration)) > avg_time_s) {
                    duration = avg_time_s - (*sum_duration);
                }
                printf("(%ld)", duration);

                (*avg_spd) += ((float) vec[i].avg / (float) avg_time_s * (float) duration);
                (*sum_duration) += duration;
                i--;
            }
        }

    };

    float calc_avg_spd(const time_t curr_time, const uint32_t avg_time_s) {
        float avg_spd = 0;
        time_t sum_duration = 0;

        sum_elem(small_avg, curr_time, avg_time_s, &avg_spd, &sum_duration);
        sum_elem(mid_avg,   curr_time, avg_time_s, &avg_spd, &sum_duration);
        sum_elem(large_avg, curr_time, avg_time_s, &avg_spd, &sum_duration);

        if ((ultimate_avg.start_time_s + ultimate_avg.duration_s) > (curr_time - avg_time_s)) {
            avg_spd += ((float) ultimate_avg.avg / (float) avg_time_s * (float) (avg_time_s - sum_duration));
        }
        return avg_spd;
    };

    // найти среднее значение элементов src и переложить его в одну ячейку dst
    void average_and_transfer_data(std::vector<avg_speed_t>& src, std::vector<avg_speed_t>& dst) {

        avg_speed_t src_sum;
        uint32_t full_src_duration = 0;

        if (src.empty()) {
            return ;
        }
        memset(&src_sum, 0, sizeof(src_sum));
        full_src_duration = src.back().duration_s + src.back().start_time_s - src.front().start_time_s ;

        for (auto it = begin (src); it != end (src); it++) {
            src_sum.avg += it->avg * it->duration_s / full_src_duration;
        }
        src_sum.start_time_s = src.front().start_time_s;
        src_sum.duration_s = full_src_duration;


        if ((!dst.empty()) &&
            (num_div_percent((float)dst.back().avg, (float)src_sum.avg, PERCENT_DIVIATION)) &&
            (src_sum.start_time_s <= (dst.back().start_time_s + dst.back().duration_s + 1))) {
            // TODO need overflow protect
            dst.back().avg = ((src_sum.duration_s * src_sum.avg)
                            + (dst.back().duration_s * dst.back().avg))
                            / (src_sum.duration_s + dst.back().duration_s);
            dst.back().duration_s += src_sum.duration_s;
        }
        else {
            dst.push_back(src_sum);
        }

        src.clear();
    };
};
