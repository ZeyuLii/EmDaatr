# -*- coding: utf-8 -*-
"""
Created on Fri Nov 17 20:46:49 2023

@author: 59770
"""

# slottable = [[0] * 200] * 19
# slotstate = [[2] * 200] * 19

slottable = []                  # 创建一个空列表
for i in range(19):      # 创建一个5行的列表（行）
    slottable.append([])        # 在空的列表中添加空的列表
    for j in range(200):  # 循环每一行的每一个元素（列）
        slottable[i].append(0)  # 为内层列表添加元素
        
slotstate = []                  # 创建一个空列表
for i in range(19):      # 创建一个5行的列表（行）
    slotstate.append([])        # 在空的列表中添加空的列表
    for j in range(200):  # 循环每一行的每一个元素（列）
        slotstate[i].append(2)  # 为内层列表添加元素

for i in range (2, 21):
    slottable[i-2][40 + 8 * (i - 2)] = 1
    slottable[i-2][40 + 8 * (i - 2) + 1] = 1
    slottable[i-2][40 + 8 * (i - 2) + 2] = 1
    slottable[i-2][40 + 8 * (i - 2) + 3] = 0
    slottable[i-2][40 + 8 * (i - 2) + 4] = 0
    slottable[i-2][40 + 8 * (i - 2) + 5] = 1
    slottable[i-2][40 + 8 * (i - 2) + 6] = 1
    slottable[i-2][40 + 8 * (i - 2) + 7] = 1

    slotstate[i-2][40 + 8 * (i - 2)]  = 0
    slotstate[i-2][40 + 8 * (i - 2) + 1] = 0
    slotstate[i-2][40 + 8 * (i - 2) + 2] = 0
    slotstate[i-2][40 + 8 * (i - 2) + 3] = 2
    slotstate[i-2][40 + 8 * (i - 2) + 4] = 2
    slotstate[i-2][40 + 8 * (i - 2) + 5] = 1
    slotstate[i-2][40 + 8 * (i - 2) + 6] = 1
    slotstate[i-2][40 + 8 * (i - 2) + 7] = 1


    
for i in range(2, 21):
    with open('Slottable_RXTX_Node_{n}.txt'.format(n = i), 'w') as f:
        for j in range(200):
            f.write(str(slottable[i-2][j]) + ',')
        for j in range(200):
            f.write(str(slottable[i-2][j]) + ',')
    
    with open('Slottable_RXTX_State_{n}.txt'.format(n = i), 'w') as f:
        for j in range(200):
            f.write(str(slotstate[i-2][j]) + ',')
        for j in range(200):
            f.write(str(slotstate[i-2][j]) + ',')
        