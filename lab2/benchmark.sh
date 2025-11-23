#!/bin/bash

echo "Performance benchmark for multithreaded determinant (definition)"
echo "================================================================"

gcc -O2 -o lab2_determinant lab2_determinant.c -lpthread -lm

N=9   # размер матрицы

echo "Matrix size: $N"
echo ""

for threads in 1 2 3 4 5 6
do
    echo "Threads: $threads"
    ./lab2_determinant $threads $N | grep -E "(Time:|Threads used:)"
    echo "----------------------------------------"
done
