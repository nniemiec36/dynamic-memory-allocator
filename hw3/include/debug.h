#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

#define NL "\n"

#ifdef COLOR
#define KNRM "\033[0m"
#define KRED "\033[1;31m"
#define KGRN "\033[1;32m"
#define KYEL "\033[1;33m"
#define KBLU "\033[1;34m"
#define KMAG "\033[1;35m"
#define KCYN "\033[1;36m"
#define KWHT "\033[1;37m"
#define KBWN "\033[0;33m"
#else
#define KNRM ""
#define KRED ""
#define KGRN ""
#define KYEL ""
#define KBLU ""
#define KMAG ""
#define KCYN ""
#define KWHT ""
#define KBWN ""
#endif

#ifdef VERBOSE
#define DEBUG
#define INFO
#define WARN
#define ERROR
#define SUCCESS
#endif

#ifdef DEBUG
#define debug(S, ...)                                                          \
  do {                                                                         \
    fprintf(stderr, KMAG "DEBUG: %s:%s:%d " KNRM S NL, __FILE__,               \
            __extension__ __FUNCTION__, __LINE__, ##__VA_ARGS__);                \
  } while (0)
#else
#define debug(S, ...)
#endif

#ifdef INFO
#define info(S, ...)                                                           \
  do {                                                                         \
    fprintf(stderr, KBLU "INFO: %s:%s:%d " KNRM S NL, __FILE__,                \
            __extension__ __FUNCTION__, __LINE__, ##__VA_ARGS__);                \
  } while (0)
#else
#define info(S, ...)
#endif

#ifdef WARN
#define warn(S, ...)                                                           \
  do {                                                                         \
    fprintf(stderr, KYEL "WARN: %s:%s:%d " KNRM S NL, __FILE__,                \
            __extension__ __FUNCTION__, __LINE__, ##__VA_ARGS__);                \
  } while (0)
#else
#define warn(S, ...)
#endif

#ifdef SUCCESS
#define success(S, ...)                                                        \
  do {                                                                         \
    fprintf(stderr, KGRN "SUCCESS: %s:%s:%d " KNRM S NL, __FILE__,             \
            __extension__ __FUNCTION__, __LINE__, ##__VA_ARGS__);                \
  } while (0)
#else
#define success(S, ...)
#endif

#ifdef ERROR
#define error(S, ...)                                                          \
  do {                                                                         \
    fprintf(stderr, KRED "ERROR: %s:%s:%d " KNRM S NL, __FILE__,               \
            __extension__ __FUNCTION__, __LINE__, ##__VA_ARGS__);                \
  } while (0)
#else
#define error(S, ...)
#endif

#endif /* DEBUG_H */
