#ifndef SRE13_LOG_H
#define SRE13_LOG_H

#include <stdio.h>

#ifndef LOG_TAG
#define LOG_TAG "SRE13"
#endif

#define LOGV(fmt, ...) printf("[SRE13/V][%s] " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define LOGD(fmt, ...) printf("[SRE13/D][%s] " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define LOGI(fmt, ...) printf("[SRE13/I][%s] " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define LOGW(fmt, ...) printf("[SRE13/W][%s] " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define LOGE(fmt, ...) printf("[SRE13/E][%s] " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define LOGF(fmt, ...) printf("[SRE13/F][%s] " fmt "\n", LOG_TAG, ##__VA_ARGS__)

#endif /* SRE13_LOG_H */
