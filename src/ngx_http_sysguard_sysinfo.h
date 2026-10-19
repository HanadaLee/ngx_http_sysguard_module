/*
 * Copyright (C) 2010-2015 Alibaba Group Holding Limited
 * Copyright (C) YoungJoo Kim (vozlt)
 * Copyright (C) Hanada
 */


#ifndef _NGX_HTTP_SYSGUARD_SYSINFO_H_INCLUDED_
#define _NGX_HTTP_SYSGUARD_SYSINFO_H_INCLUDED_


/* Memory sizes are in bytes. */
typedef struct {
    size_t          totalram;
    size_t          freeram;
    size_t          bufferram;
    size_t          cachedram;
    size_t          totalswap;
    size_t          freeswap;
    unsigned        swap_available:1;
} ngx_http_sysguard_meminfo_t;


typedef struct {
    time_t                           load_expires;
    time_t                           mem_expires;
    ngx_int_t                        load_status;
    ngx_int_t                        mem_status;
    ngx_int_t                        load;
    ngx_uint_t                       swap;
    size_t                           free;
    ngx_http_sysguard_meminfo_t      meminfo;
} ngx_http_sysguard_sysinfo_t;


ngx_int_t ngx_http_sysguard_getloadavg(ngx_int_t *load, ngx_log_t *log);
ngx_int_t ngx_http_sysguard_getmeminfo(ngx_http_sysguard_meminfo_t *meminfo,
    ngx_log_t *log);


#endif /* _NGX_HTTP_SYSGUARD_SYSINFO_H_INCLUDED_ */
