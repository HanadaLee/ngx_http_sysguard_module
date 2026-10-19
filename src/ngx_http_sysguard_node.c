/*
 * Copyright (C) 2010-2015 Alibaba Group Holding Limited
 * Copyright (C) YoungJoo Kim (vozlt)
 * Copyright (C) Hanada
 */


#include "ngx_http_sysguard_module.h"


void
ngx_http_sysguard_rt_add(ngx_http_sysguard_rt_t *rt, ngx_msec_t elapsed)
{
    ngx_uint_t  i;

    i = (rt->first + rt->count) % rt->capacity;

    if (rt->count == rt->capacity) {
        rt->first = (rt->first + 1) % rt->capacity;

    } else {
        rt->count++;
    }

    rt->samples[i].time = ngx_current_msec;
    rt->samples[i].elapsed = elapsed;
}


ngx_msec_t
ngx_http_sysguard_rt_average(ngx_http_sysguard_rt_t *rt, ngx_msec_t period,
    ngx_uint_t method)
{
    double          total, weight, weights;
    ngx_uint_t      i, n;
    ngx_msec_int_t  age;

    while (rt->count) {
        age = (ngx_msec_int_t) (ngx_current_msec
                               - rt->samples[rt->first].time);

        if (age >= 0 && (ngx_msec_t) age < period) {
            break;
        }

        rt->first = (rt->first + 1) % rt->capacity;
        rt->count--;
    }

    total = 0;
    weights = 0;

    for (n = 0; n < rt->count; n++) {
        i = (rt->first + n) % rt->capacity;
        weight = (method == NGX_HTTP_SYSGUARD_AVERAGE_WMA) ? n + 1 : 1;
        total += (double) rt->samples[i].elapsed * weight;
        weights += weight;
    }

    return weights ? (ngx_msec_t) (total / weights) : 0;
}
