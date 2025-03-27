#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// print函数实现
void print(char* str) {
    printf("%s\n", str);
}

// input函数实现
char* input() {
    char* buffer = malloc(1024);
    fgets(buffer, 1024, stdin);
    // 移除换行符
    buffer[strcspn(buffer, "\n")] = 0;
    return buffer;
}
