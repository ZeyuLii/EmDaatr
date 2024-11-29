#include "lstm.h"
#include <cmath>
#include <iostream>
#include <string.h>

using namespace std;

// 激活函数
static double sigmoid(double x) { return 1.0 / (1.0 + exp(-x)); }

void init_LstmStates(LstmStates *p, int hide, int out) {
    p->I_G = (double *)malloc(sizeof(double) * hide);
    p->F_G = (double *)malloc(sizeof(double) * hide);
    p->O_G = (double *)malloc(sizeof(double) * hide);
    p->N_I = (double *)malloc(sizeof(double) * hide);
    p->S = (double *)malloc(sizeof(double) * hide);
    p->H = (double *)malloc(sizeof(double) * hide);
    p->PreS = (double *)malloc(sizeof(double) * hide);
    p->PreH = (double *)malloc(sizeof(double) * hide);
    p->Y = (double *)malloc(sizeof(double) * out);
    p->yDelta = (double *)malloc(sizeof(double) * out);

    memset(p->I_G, 0, sizeof(double) * hide);
    memset(p->F_G, 0, sizeof(double) * hide);
    memset(p->O_G, 0, sizeof(double) * hide);
    memset(p->N_I, 0, sizeof(double) * hide);
    memset(p->S, 0, sizeof(double) * hide);
    memset(p->H, 0, sizeof(double) * hide);
    memset(p->PreS, 0, sizeof(double) * hide);
    memset(p->PreH, 0, sizeof(double) * hide);
    memset(p->Y, 0, sizeof(double) * out);
    memset(p->yDelta, 0, sizeof(double) * out);
}

/*
单个样本正向传播
参数：
x、单个样本特征向量
*/
void Lstm_forward(LstmStates *sp, Lstm *lp, double *x) {
    if (x == NULL) {
        return;
    }

    // 上个时间点的状态
    memcpy(sp->PreS, (lp->states).S, sizeof(double) * lp->hideNodeNum);
    memcpy(sp->PreH, (lp->states).H, sizeof(double) * lp->hideNodeNum);

    // 输入层转播到隐层
    for (int j = 0; j < lp->hideNodeNum; ++j) {
        double inGate = 0.0;
        double outGate = 0.0;
        double forgetGate = 0.0;
        double newIn = 0.0;

        for (int m = 0; m < lp->inNodeNum; ++m) {
            inGate += x[m] * lp->W_I[m][j];
            outGate += x[m] * lp->W_O[m][j];
            forgetGate += x[m] * lp->W_F[m][j];
            newIn += x[m] * lp->W_G[m][j];
        }

        for (int m = 0; m < lp->hideNodeNum; ++m) {
            inGate += sp->PreH[m] * lp->U_I[m][j];
            outGate += sp->PreH[m] * lp->U_O[m][j];
            forgetGate += sp->PreH[m] * lp->U_F[m][j];
            newIn += sp->PreH[m] * lp->U_G[m][j];
        }

        inGate += lp->B_I[j];
        outGate += lp->B_O[j];
        forgetGate += lp->B_F[j];
        newIn += lp->B_G[j];

        sp->I_G[j] = sigmoid(inGate);
        sp->O_G[j] = sigmoid(outGate);
        sp->F_G[j] = sigmoid(forgetGate);
        sp->N_I[j] = tanh(newIn);

        // 得出本时间点状态
        sp->S[j] = sp->F_G[j] * sp->PreS[j] + (sp->N_I[j] * sp->I_G[j]);
        // 本时间点的输出
        sp->H[j] = sp->O_G[j] * tanh(sp->S[j]); // changed
    }

    // 隐藏层传播到输出层
    double out = 0.0;
    for (int i = 0; i < lp->outNodeNum; ++i) {
        for (int j = 0; j < lp->hideNodeNum; ++j) {
            out += sp->H[j] * lp->W_Y[j][i];
        }
        out += lp->B_Y[i];
        sp->Y[i] = out; // 输出层各单元输出
    }
    return;
}

/*
预测单个样本
参数：
x、需预测样本的特征集
*/

double lstm_predict(LstmStates *s, Lstm *p, double *x) {
    double ret;

    Lstm_forward(s, p, x);
    ret = *s->Y;
    // memcpy(ret, s->Y, sizeof(double) * p->outNodeNum);//备份结果
    p->states = *s; // 记住当前时间点的单元状态
    return ret;
}
