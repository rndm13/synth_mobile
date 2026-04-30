#pragma once

#define MS_IN_S        1000
#define NS_IN_MS       (1000 * 1000)
#define NS_IN_S        (1000 * 1000 * 1000)

#define S_TO_MS(X)     ((X) * MS_IN_S)
#define NS_TO_MS(X)    ((X) / NS_IN_MS)
#define S_TO_NS(X)     ((X) * NS_IN_S)

#define ARRAY_SIZE(X)  (sizeof(X) / sizeof(*(X)))

#define X_ENUM(v, s) \
    v,

#define X_STR_ARR(v, s) \
    s,

#define X_STR_CASE(v, s) \
    case v: return s;
