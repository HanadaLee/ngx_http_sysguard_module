/*
 * Copyright (C) 2010-2015 Alibaba Group Holding Limited
 * Copyright (C) YoungJoo Kim (vozlt)
 * Copyright (C) Hanada
 */


#include "ngx_http_sysguard_module.h"
#include "ngx_http_sysguard_variables.h"


static ngx_int_t ngx_http_sysguard_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_sysguard_memory_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);


static ngx_http_variable_t  ngx_http_sysguard_variables[] = {

    { ngx_string("sysguard_load"), NULL, ngx_http_sysguard_variable,
      0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("sysguard_swapstat"), NULL, ngx_http_sysguard_variable,
      1, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("sysguard_free"), NULL, ngx_http_sysguard_variable,
      2, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("sysguard_rt"), NULL, ngx_http_sysguard_variable,
      3, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("sysguard_meminfo_totalram"), NULL,
      ngx_http_sysguard_memory_variable,
      offsetof(ngx_http_sysguard_meminfo_t, totalram),
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("sysguard_meminfo_freeram"), NULL,
      ngx_http_sysguard_memory_variable,
      offsetof(ngx_http_sysguard_meminfo_t, freeram),
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("sysguard_meminfo_bufferram"), NULL,
      ngx_http_sysguard_memory_variable,
      offsetof(ngx_http_sysguard_meminfo_t, bufferram),
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("sysguard_meminfo_cachedram"), NULL,
      ngx_http_sysguard_memory_variable,
      offsetof(ngx_http_sysguard_meminfo_t, cachedram),
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("sysguard_meminfo_totalswap"), NULL,
      ngx_http_sysguard_memory_variable,
      offsetof(ngx_http_sysguard_meminfo_t, totalswap),
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("sysguard_meminfo_freeswap"), NULL,
      ngx_http_sysguard_memory_variable,
      offsetof(ngx_http_sysguard_meminfo_t, freeswap),
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

      ngx_http_null_variable
};


static ngx_int_t
ngx_http_sysguard_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    u_char                     *p;
    size_t                      value;
    ngx_http_sysguard_conf_t   *conf;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_sysguard_module);

    switch (data) {
    case 0:
        if (ngx_http_sysguard_update_load(r, conf) != NGX_OK) {
            goto not_found;
        }

        value = conf->sysinfo.load;
        break;

    case 1:
    case 2:
        if (ngx_http_sysguard_update_mem(r, conf) != NGX_OK) {
            goto not_found;
        }

        if (data == 1 && !conf->sysinfo.meminfo.swap_available) {
            goto not_found;
        }

        value = (data == 1) ? conf->sysinfo.swap / 100 : conf->sysinfo.free;
        break;

    default:
        value = ngx_http_sysguard_response_time(conf);
        break;
    }

    p = ngx_pnalloc(r->pool, NGX_SIZE_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(p, "%uz", value) - p;
    v->data = p;
    v->valid = 1;
    v->no_cacheable = 1;
    v->not_found = 0;
    return NGX_OK;

not_found:

    v->not_found = 1;
    return NGX_OK;
}


static ngx_int_t
ngx_http_sysguard_memory_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                     *p;
    size_t                      value;
    ngx_http_sysguard_conf_t   *conf;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_sysguard_module);

    if (ngx_http_sysguard_update_mem(r, conf) != NGX_OK
        || (!conf->sysinfo.meminfo.swap_available
            && (data == offsetof(ngx_http_sysguard_meminfo_t, totalswap)
                || data == offsetof(ngx_http_sysguard_meminfo_t, freeswap))))
    {
        v->not_found = 1;
        return NGX_OK;
    }

    value = *(size_t *) ((u_char *) &conf->sysinfo.meminfo + data);
    p = ngx_pnalloc(r->pool, NGX_SIZE_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(p, "%uz", value) - p;
    v->data = p;
    v->valid = 1;
    v->no_cacheable = 1;
    v->not_found = 0;
    return NGX_OK;
}


ngx_int_t
ngx_http_sysguard_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_sysguard_variables; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}
