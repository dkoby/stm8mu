#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdarg.h>
#include "types.h"

#define ERR_PREFIX    "(E) "
#define WARN_PREFIX   "(W) "

#define NEW_LINE "\n"
#define NL       NEW_LINE
#define SQ       "\"%s\""
#define TAB      "    "

#define DEBUG_IMSG(msg) \
            PRINTF("%s: %s"NEW_LINE, __FUNCTION__, msg)
#define DEBUG_IMSGF(msg, ...)                                  \
        do                                                     \
        {                                                      \
            PRINTF("%s: %s, ", __FUNCTION__, msg);             \
            PRINTF(__VA_ARGS__);                               \
        } while (0)

#define DEBUG_WMSG(msg) \
            PRINTF(WARN_PREFIX"%s: %s"NEW_LINE, __FUNCTION__, msg)
#define DEBUG_WMSGF(msg, ...)                                  \
        do                                                     \
        {                                                      \
            PRINTF(WARN_PREFIX"%s: %s, ", __FUNCTION__, msg);  \
            PRINTF(__VA_ARGS__);                               \
        } while (0)

#define DEBUG_EMSG(msg) \
            PRINTF(ERR_PREFIX"%s: %s"NEW_LINE, __FUNCTION__, msg)
#define DEBUG_EMSGF(msg, ...)                                  \
        do                                                     \
        {                                                      \
            PRINTF(ERR_PREFIX"%s: %s, ", __FUNCTION__, msg);   \
            PRINTF(__VA_ARGS__);                               \
        } while (0)

#define debug_imsg(msg) \
            printf("%s: %s"NEW_LINE, __FUNCTION__, msg)
#define debug_imsgf(msg, ...)                                  \
        do                                                     \
        {                                                      \
            printf("%s: %s, ", __FUNCTION__, msg);             \
            printf(__VA_ARGS__);                               \
        } while (0)

#define debug_wmsg(msg) \
            printf(WARN_PREFIX"%s: %s"NEW_LINE, __FUNCTION__, msg)
#define debug_wmsgf(msg, ...)                                  \
        do                                                     \
        {                                                      \
            printf(WARN_PREFIX"%s: %s, ", __FUNCTION__, msg);  \
            printf(__VA_ARGS__);                               \
        } while (0)

#define debug_emsg(msg) \
            printf(ERR_PREFIX"%s: %s"NEW_LINE, __FUNCTION__, msg)
#define debug_emsgf(msg, ...)                                  \
        do                                                     \
        {                                                      \
            printf(ERR_PREFIX"%s: %s, ", __FUNCTION__, msg);   \
            printf(__VA_ARGS__);                               \
        } while (0)

void debug_buf(uint8_t *buf, uint32_t len);

#endif

