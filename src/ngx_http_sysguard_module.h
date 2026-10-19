/*
 * Copyright (C) 2010-2015 Alibaba Group Holding Limited
 * Copyright (C) YoungJoo Kim (vozlt)
 * Copyright (C) Hanada
 */


#ifndef _NGX_HTTP_SYSGUARD_MODULE_H_INCLUDED_
#define _NGX_HTTP_SYSGUARD_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_sysguard_sysinfo.h"
#include "ngx_http_sysguard_node.h"


#define NGX_HTTP_SYSGUARD_MODE_OR   0
#define NGX_HTTP_SYSGUARD_MODE_AND  1


typedef struct {
    ngx_flag_t                       enable;
    ngx_int_t                        load;
    ngx_str_t                        load_action;
    ngx_int_t                        swap;
    ngx_str_t                        swap_action;
    size_t                           free;
    ngx_str_t                        free_action;
    ngx_int_t                        rt;
    time_t                           rt_period;
    ngx_int_t                        rt_number;
    ngx_int_t                        rt_method;
    ngx_str_t                        rt_action;
    time_t                           interval;
    ngx_uint_t                       log_level;
    ngx_uint_t                       mode;
    ngx_http_sysguard_sysinfo_t      sysinfo;
    ngx_http_sysguard_rt_t           request_times;
} ngx_http_sysguard_conf_t;


ngx_int_t ngx_http_sysguard_update_load(ngx_http_request_t *r,
    ngx_http_sysguard_conf_t *conf);
ngx_int_t ngx_http_sysguard_update_mem(ngx_http_request_t *r,
    ngx_http_sysguard_conf_t *conf);
ngx_msec_t ngx_http_sysguard_response_time(ngx_http_sysguard_conf_t *conf);

extern ngx_module_t ngx_http_sysguard_module;


#endif /* _NGX_HTTP_SYSGUARD_MODULE_H_INCLUDED_ */
