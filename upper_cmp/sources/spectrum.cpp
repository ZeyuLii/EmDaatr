#include "spectrum.h"
#include "main.h"
static bool Randomly_Generate_0_1(double one_probability) {
    // 以one_probability大小的概率生成1
    bool t = 0;
    // srand(time(NULL));
    double random_number = (double)rand() / RAND_MAX;
    // cout << random_number;
    if (random_number < one_probability) {
        t = 1;
    } else {
        t = 0;
    }
    return t;
}
void write_spectrum_to_file(double probability) {
    char Spectrum[TOTAL_FREQ_POINT] = {0};
    char filePath[100];
    for (int id = 1; id < subnet_node_num + 1; id++) {
        for (int times = 0; times < SPECTRUM_SENSING_TIME; times++) {
            sprintf(filePath, "./Spectrum_sensing/NODE%d/time%d.txt", id, times);
            int file_Spectrum = open(filePath, O_CREAT | O_TRUNC | O_WRONLY);

            if (!file_Spectrum) printf("文件打开错误");
            for (int i = 0; i < TOTAL_FREQ_POINT; i++) {
                if (Randomly_Generate_0_1(probability))
                    Spectrum[i] = '1';
                else
                    Spectrum[i] = '0';
            }

            write(file_Spectrum, Spectrum, TOTAL_FREQ_POINT);
            close(file_Spectrum);
            sprintf(filePath, "chmod 777 ./Spectrum_sensing/NODE%d/time%d.txt", id, times);
            system(filePath);
            memset(filePath, 0, 100);
        }
    }
}