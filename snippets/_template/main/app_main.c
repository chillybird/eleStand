/*
 * 代码片段模板
 *
 * 使用方法:
 *   1. 复制 _template 目录, 重命名为项目名
 *   2. 编辑 CMakeLists.txt 中的 project() 名称
 *   3. 在 main/CMakeLists.txt 的 PRIV_REQUIRES 中添加依赖
 *   4. 在 app_main() 中编写代码
 *   5. idf.py set-target esp32c3 && idf.py build flash -p COM3 monitor
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
    printf("Hello from template\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
