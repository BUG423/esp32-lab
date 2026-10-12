/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OH_HILOG_LOG_H
#define OH_HILOG_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log Levels */
typedef enum {
    LOG_LEVEL_MIN = 0,
    LOG_DEBUG = 3,
    LOG_INFO  = 4,
    LOG_WARN  = 5,
    LOG_ERROR = 6,
    LOG_FATAL = 7,
    LOG_LEVEL_MAX
} LogLevel;

/* Log Domains */
typedef enum {
    LOG_CORE = 0,
    LOG_APP  = 1,
} LogType;

#define HILOG_DOMAIN_CORE 0xD001800
#define HILOG_DOMAIN_APP  0xD001801

/* HiLog Core Printing API */
int HiLogPrint(LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, ...);

/* HiLog Format Logging Macros */
#define HILOG_DEBUG(type, fmt, ...) \
    HiLogPrint(type, LOG_DEBUG, HILOG_DOMAIN_CORE, HILOG_MODULE, fmt, ##__VA_ARGS__)

#define HILOG_INFO(type, fmt, ...) \
    HiLogPrint(type, LOG_INFO, HILOG_DOMAIN_CORE, HILOG_MODULE, fmt, ##__VA_ARGS__)

#define HILOG_WARN(type, fmt, ...) \
    HiLogPrint(type, LOG_WARN, HILOG_DOMAIN_CORE, HILOG_MODULE, fmt, ##__VA_ARGS__)

#define HILOG_ERROR(type, fmt, ...) \
    HiLogPrint(type, LOG_ERROR, HILOG_DOMAIN_CORE, HILOG_MODULE, fmt, ##__VA_ARGS__)

#define HILOG_FATAL(type, fmt, ...) \
    HiLogPrint(type, LOG_FATAL, HILOG_DOMAIN_CORE, HILOG_MODULE, fmt, ##__VA_ARGS__)

/* Logger Control APIs */
void HiLogSetMinLevel(LogLevel level);
LogLevel HiLogGetMinLevel(void);
int HiLogGetRecordCount(void);
void HiLogInit(void);

#ifdef __cplusplus
}
#endif

#endif /* OH_HILOG_LOG_H */
