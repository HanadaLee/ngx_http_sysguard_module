/*
 * Copyright (C) 2010-2015 Alibaba Group Holding Limited
 * Copyright (C) YoungJoo Kim (vozlt)
 * Copyright (C) Hanada
 */


#ifndef _NGX_HTTP_SYSGUARD_NODE_H_INCLUDED_
#define _NGX_HTTP_SYSGUARD_NODE_H_INCLUDED_


#define NGX_HTTP_SYSGUARD_AVERAGE_AMM  0
#define NGX_HTTP_SYSGUARD_AVERAGE_WMA  1


typedef struct {
    ngx_msec_t                      time;
    ngx_msec_t                      elapsed;
} ngx_http_sysguard_sample_t;


typedef struct {
    ngx_http_sysguard_sample_t      *samples;
    ngx_uint_t                       first;
    ngx_uint_t                       count;
    ngx_uint_t                       capacity;
} ngx_http_sysguard_rt_t;


void ngx_http_sysguard_rt_add(ngx_http_sysguard_rt_t *rt, ngx_msec_t elapsed);
ngx_msec_t ngx_http_sysguard_rt_average(ngx_http_sysguard_rt_t *rt,
    ngx_msec_t period, ngx_uint_t method);


#endif /* _NGX_HTTP_SYSGUARD_NODE_H_INCLUDED_ */
