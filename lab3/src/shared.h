#ifndef SHARED_H
#define SHARED_H

#define BUF_SIZE 1024
#define SHM_FILE "shared.bin"

// Общая структура в отображаемом файле
typedef struct {
    int  stage;          // 0 - idle, 1 - parent->child1, 2 - child1->child2, 3 - child2->parent, 4 - terminate
    char buf[BUF_SIZE];  // общий текстовый буфер
} shared_data_t;

#endif // SHARED_H
