#ifndef __H_LSTM_H__
#define __H_LSTM_H__

using namespace std;

struct LstmStates {
    double *I_G;    // 输入门
    double *F_G;    // 遗忘门
    double *O_G;    // 输出门
    double *N_I;    // 新输入
    double *S;      // 状态值
    double *H;      // 隐层输出值
    double *Y;      // 输出值
    double *yDelta; // 保存误差关于输出层的偏导
    double *PreS;   // 上一时刻的状态值
    double *PreH;   // 上一时刻的隐层输出值
};

struct Lstm {
    int inNodeNum;       // 输入单元个数（特征数）
    int hideNodeNum;     // 隐藏单元个数
    int outNodeNum;      // 输出单元个数（结果维度）
    float verification;  // 验证集比例
    LstmStates states;   // 隐层单元状态
    double learningRate; // 学习率

    double **W_I; // 连接输入与隐含层单元中输入门的权值矩阵
    double **U_I; // 连接上一隐层输出与本隐含层单元中输入门的权值矩阵
    double *B_I;
    double **W_F; // 连接输入与隐含层单元中遗忘门的权值矩阵
    double **U_F; // 连接上一隐含层与本隐含层单元中遗忘门的权值矩阵
    double *B_F;
    double **W_O; // 连接输入与隐含层单元中输出门的权值矩阵
    double **U_O; // 连接上一隐含层与现在时刻的隐含层的权值矩阵
    double *B_O;
    double **W_G; // 连接输入与隐含层单元产生新记忆的权值矩阵
    double **U_G; // 连接隐含层间单元产生新记忆的权值矩阵
    double *B_G;
    double **W_Y; // 连接隐层与输出层的权值矩阵
    double *B_Y;
};

void init_LstmStates(LstmStates *sp, int hide, int out);

double lstm_predict(LstmStates *s, Lstm *p, double *X);
void forward(LstmStates *sp, Lstm *lp, double *x); // 单个样本正向传播
void BuildLstmModel(const char *input_filename, Lstm *lstmmodel);
void Lstm_forward(LstmStates *sp, Lstm *lp, double *x);
static double sigmoid(double x);

#endif //__H_LSTM_H__
