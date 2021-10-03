#include <vector>


#define ERROR_TOO_LONG_WINDOW -1
#define ERROR_ZERO_WINDOW     -2
#define NO_DATA                0

#define PERCENT_DEVIATION 10.0f

#define VEC_TARGET_SIZE 100

typedef struct {
    uint32_t avg; // средняя скорость
    time_t dur_s; // продолжительность временного отрезка
    time_t start_s; // время начало отрезка (Unix time)
    // TODO 32-bit architecture: 'Year 2038 problem'
}
avg_chunk_t;

class Averager {
public:
    Averager(uint32_t avg_time_s) {
        p_avg_time_s = avg_time_s;
        memset(&ultimate, 0, sizeof(ultimate));
    };

    /* Меняем время усреднения в процессе работы */
    void SetAveragingTime(uint32_t avg_time_s) {
        p_avg_time_s = avg_time_s;
    };

    /* Возвращается среднее значение скорости */
    float GetAverageSpeed() {

        uint32_t avg_time_s = p_avg_time_s;
        time_t cur_time = time(nullptr);

        if (avg_time_s == 0) {
            return ERROR_ZERO_WINDOW;
        }
        if (window_too_long(cur_time, avg_time_s)) {
            return ERROR_TOO_LONG_WINDOW;
        }
        if (small.empty() && mid.empty() && large.empty() && ultimate.dur_s == 0) {
            return NO_DATA;
        }

        // обход всех векторов с сохраненными данными
        return calc_avg_spd(cur_time, avg_time_s);
    };


    /* Добавлем информацию о получении value байтов */
    void AddValue(uint32_t value) {
        avg_chunk_t new_elem;
        avg_chunk_t *last = nullptr;
        if (!small.empty()) {
            last = &small.back();
        }
        // TODO постоянный syscall - плохое решение
        time_t curr_time = time(nullptr);

        // если в эту секунду уже принимали данные, добавляем их к имеющимся
        if (!small.empty() && (curr_time <= last->dur_s + last->start_s - 1)) {
            last->avg = (last->avg * last->dur_s + value) / last->dur_s;
        }
        // проверка на то, можно ли добавить элемент в уже созданную ячейку
        // можно если он не сильно отклоняет avg и присутствие последовательность во времени приема
        else if ((!small.empty()) &&
                 (num_div_percent((float)last->avg, (float)value, PERCENT_DEVIATION)) &&
                 (curr_time <= (last->start_s + last->dur_s + 1))) {
            // TODO need overflow protect
            last->avg = (last->avg * last->dur_s + value) / (last->dur_s + 1);
            last->dur_s++;
        }
        // создаем новый элемент
        else {
            new_elem.avg = value;
            new_elem.dur_s = 1;
            new_elem.start_s = curr_time;
            small.push_back(new_elem);
        }

        // когда накопили много данных, аппроксимируем их и добавляем в вектор с большим усреднением
        // (очищая начальный вектор)
        if (small.size() > VEC_TARGET_SIZE) {
            average_and_transfer_data(small, mid);
        }
        if (mid.size() > VEC_TARGET_SIZE) {
            average_and_transfer_data(mid, large);
        }
        // если данных накопилось предельного много, сохраняем их в переменную с максимальным усреднением
        if (large.size() > VEC_TARGET_SIZE) {
            avg_chunk_t src_sum;
            uint32_t full_duration =
                    large.back().dur_s + large.back().start_s - large.front().start_s;
            memset(&src_sum, 0, sizeof(src_sum));

            for (auto it = begin (large); it != end (large); it++) {
                src_sum.avg += it->avg * it->dur_s / full_duration;
            }
            src_sum.start_s = large.front().start_s;
            src_sum.dur_s = full_duration;

            // TODO need overflow protect
            ultimate.avg = ((src_sum.dur_s * src_sum.avg)
                            + (ultimate.dur_s * ultimate.avg))
                           / (src_sum.dur_s + ultimate.dur_s);
            ultimate.dur_s += src_sum.dur_s;

            large.clear();
        }
    };

private:
    // изначально получаемые данные хранятся в векторе 'small' с 1-секундным усреднением
    // когда он наполняется, находится средняя скорость и перекладывается в одну ячейку 'mid' вектора
    // в конечном итоге, при продолжительном измерении, максимально усредненные данные попадут в 'ultimate' переменную
    std::vector<avg_chunk_t> small;
    std::vector<avg_chunk_t> mid;
    std::vector<avg_chunk_t> large;
    avg_chunk_t ultimate;

    uint32_t p_avg_time_s; // окно по которому будет происходить усреднение

    // если отклонение dst от src превышает div% то возвращает false
    bool num_div_percent(const float src, const float dst, const float div) {
        bool ret;
        if (((src > dst) && ((src * (1.0f - div / 100)) < dst)) ||
            ((src < dst) && ((src * (1.0f + div / 100)) > dst)) ||
            (src == dst)) {
            ret = true;
        } else {
            ret = false;
        }
        return ret;
    };

    // проверяем не выходит ли запрашиваемое пользователем окно за рамки измеренного программой
    bool window_too_long(const time_t curr_time, const uint32_t avg_time_s) {
        bool ret;
        if ((ultimate.avg != 0 && ultimate.start_s <= (curr_time - avg_time_s)) ||
            (!large.empty() && large.front().start_s <= (curr_time - avg_time_s)) ||
            (!mid.empty() && mid.front().start_s <= (curr_time - avg_time_s)) ||
            (!small.empty() && small.front().start_s <= (curr_time - avg_time_s))) {
            ret = false;
        } else {
           ret = true;
         }
        return ret;
    };

    // обход полученного вектора vec с сохранением суммы в avg_spd и продолжительности в sum_dur
    void sum_elem(const std::vector<avg_chunk_t>& vec,
                  const time_t cur_time, const uint32_t avg_time_s,
                  float* avg_spd, time_t* sum_dur) {
        time_t dur = 0;

        if (!vec.empty()) {
            auto i = vec.size() - 1;
            // обходим вектор с конца пока время в ячейках соответствует запрашиваемому окну
            while ((i < vec.size()) &&
                   ((vec[i].start_s + vec[i].dur_s) > (cur_time - avg_time_s))) {

                dur = vec[i].dur_s;
                // если необходима только часть данных их ячейки
                if ((dur + (*sum_dur)) > avg_time_s) {
                    dur = avg_time_s - (*sum_dur);
                }

                (*avg_spd) += ((float) vec[i].avg / (float) avg_time_s * (float) dur);
                (*sum_dur) += dur;
                i--;
            }
        }
    };

    // расчет средней скорости приема данных
    float calc_avg_spd(const time_t cur_time, const uint32_t avg_time_s) {
        float avg_spd = 0;
        time_t sum_dur = 0;

        // обходим каждый вектор и суммируем скорость в них (с учетом веса каждого временного отрезка)
        sum_elem(small, cur_time, avg_time_s, &avg_spd, &sum_dur);
        sum_elem(mid, cur_time, avg_time_s, &avg_spd, &sum_dur);
        sum_elem(large, cur_time, avg_time_s, &avg_spd, &sum_dur);
        if ((ultimate.start_s + ultimate.dur_s) > (cur_time - avg_time_s)) {
            avg_spd += ((float) ultimate.avg / (float) avg_time_s * (float) (avg_time_s - sum_dur));
        }

        return avg_spd;
    };

    // найти среднее значение элементов src и переложить его в одну ячейку dst
    void average_and_transfer_data(std::vector<avg_chunk_t>& src, std::vector<avg_chunk_t>& dst) {

        avg_chunk_t src_sum;
        uint32_t full_src_duration = 0;

        if (src.empty()) {
            return ;
        }
        memset(&src_sum, 0, sizeof(src_sum));

        // находим продолжительность приема данных в усредняемом векторе (включая промежутки без входящих данных)
        full_src_duration = src.back().dur_s + src.back().start_s - src.front().start_s ;

        for (auto it = begin (src); it != end (src); it++) {
            src_sum.avg += it->avg * it->dur_s / full_src_duration;
        }
        src_sum.start_s = src.front().start_s;
        src_sum.dur_s = full_src_duration;

        // при возможности добавляем данные в уже существующую ячейку
        // (если нет большого отклонение в avg и присутствие последовательность во времени приема)
        if ((!dst.empty()) &&
            (num_div_percent((float)dst.back().avg, (float)src_sum.avg, PERCENT_DEVIATION)) &&
            (src_sum.start_s <= (dst.back().start_s + dst.back().dur_s + 1))) {
            // TODO need overflow protect
            dst.back().avg = ((src_sum.dur_s * src_sum.avg)
                            + (dst.back().dur_s * dst.back().avg))
                            / (src_sum.dur_s + dst.back().dur_s);
            dst.back().dur_s += src_sum.dur_s;
        }
        // добавляем новый элемент в вектор с большим усреднением
        else {
            dst.push_back(src_sum);
        }

        // очищаем усредняемый вектор
        src.clear();
    };
};
