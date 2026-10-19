/*
 * Copyright (C) 2010-2015 Alibaba Group Holding Limited
 * Copyright (C) YoungJoo Kim (vozlt)
 * Copyright (C) Hanada
 */


#include "ngx_http_sysguard_module.h"
#include "ngx_http_sysguard_variables.h"


typedef struct {
    ngx_flag_t                       rt_used;
} ngx_http_sysguard_main_conf_t;


typedef struct {
    ngx_http_sysguard_conf_t         *conf;
    unsigned                          checked:1;
    unsigned                          rejected:1;
} ngx_http_sysguard_ctx_t;


static void *ngx_http_sysguard_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_sysguard_create_conf(ngx_conf_t *cf);
static char *ngx_http_sysguard_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);
static char *ngx_http_sysguard_cpu(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_sysguard_load(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_sysguard_mem(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_sysguard_rt(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static ngx_int_t ngx_http_sysguard_action(ngx_str_t *value, ngx_str_t *action);
static ngx_int_t ngx_http_sysguard_init(ngx_conf_t *cf);
static void ngx_http_sysguard_cleanup(void *data);


static ngx_conf_enum_t  ngx_http_sysguard_log_levels[] = {
    { ngx_string("info"), NGX_LOG_INFO },
    { ngx_string("notice"), NGX_LOG_NOTICE },
    { ngx_string("warn"), NGX_LOG_WARN },
    { ngx_string("error"), NGX_LOG_ERR },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_sysguard_modes[] = {
    { ngx_string("or"), NGX_HTTP_SYSGUARD_MODE_OR },
    { ngx_string("and"), NGX_HTTP_SYSGUARD_MODE_AND },
    { ngx_null_string, 0 }
};


static ngx_command_t  ngx_http_sysguard_commands[] = {

    { ngx_string("sysguard"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sysguard_conf_t, enable),
      NULL },

    { ngx_string("sysguard_mode"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sysguard_conf_t, mode),
      &ngx_http_sysguard_modes },

    { ngx_string("sysguard_load"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_sysguard_load,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("sysguard_cpu"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE123,
      ngx_http_sysguard_cpu,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("sysguard_mem"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_sysguard_mem,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("sysguard_rt"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1234,
      ngx_http_sysguard_rt,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("sysguard_interval"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_sec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sysguard_conf_t, interval),
      NULL },

    { ngx_string("sysguard_log_level"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sysguard_conf_t, log_level),
      &ngx_http_sysguard_log_levels },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_sysguard_module_ctx = {
    ngx_http_sysguard_add_variables,        /* preconfiguration */
    ngx_http_sysguard_init,                 /* postconfiguration */

    ngx_http_sysguard_create_main_conf,     /* create main configuration */
    NULL,                                   /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    ngx_http_sysguard_create_conf,          /* create location configuration */
    ngx_http_sysguard_merge_conf            /* merge location configuration */
};


ngx_module_t  ngx_http_sysguard_module = {
    NGX_MODULE_V1,
    &ngx_http_sysguard_module_ctx,          /* module context */
    ngx_http_sysguard_commands,             /* module directives */
    NGX_HTTP_MODULE,                        /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    NULL,                                   /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    NULL,                                   /* exit process */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};


ngx_int_t
ngx_http_sysguard_update_cpu(ngx_http_request_t *r,
    ngx_http_sysguard_conf_t *conf)
{
    uint64_t                      busy, total, delta;
    ngx_uint_t                     i;
    ngx_http_sysguard_cpuinfo_t    current;
    ngx_http_sysguard_sysinfo_t   *info;

    info = &conf->sysinfo;

    if (info->cpu_expires > ngx_time()) {
        return info->cpu_status;
    }

    info->cpu_expires = ngx_time() + conf->cpu_period;
    info->cpu_status = NGX_DECLINED;

    if (ngx_http_sysguard_getcpuinfo(&current, r->connection->log) != NGX_OK) {
        info->cpu_started = 0;
        info->cpu_status = NGX_ERROR;
        return NGX_ERROR;
    }

    if (!info->cpu_started) {
        goto baseline;
    }

    busy = 0;
    total = 0;

    for (i = 0; i < 7; i++) {
        if (current.times[i] < info->cpu_previous.times[i]) {
            goto baseline;
        }

        delta = current.times[i] - info->cpu_previous.times[i];
        if (delta > (uint64_t) -1 - total) {
            goto baseline;
        }

        total += delta;
        if (i < 3) {
            busy += delta;
        }
    }

    if (total) {
        /* Match Tengine: user + nice + system over the first seven fields. */
        info->cpu = (ngx_uint_t) ((long double) busy * 10000 / total);
        info->cpu_status = NGX_OK;
    }

baseline:

    info->cpu_previous = current;
    info->cpu_started = 1;
    return info->cpu_status;
}


ngx_int_t
ngx_http_sysguard_update_load(ngx_http_request_t *r,
    ngx_http_sysguard_conf_t *conf)
{
    ngx_http_sysguard_sysinfo_t  *info;

    info = &conf->sysinfo;

    if (info->load_expires <= ngx_time()) {
        info->load_expires = ngx_time() + conf->interval;
        info->load_status = ngx_http_sysguard_getloadavg(&info->load,
                                                        r->connection->log);
    }

    return info->load_status;
}


ngx_int_t
ngx_http_sysguard_update_mem(ngx_http_request_t *r,
    ngx_http_sysguard_conf_t *conf)
{
    ngx_http_sysguard_meminfo_t  *mem;
    ngx_http_sysguard_sysinfo_t  *info;

    info = &conf->sysinfo;

    if (info->mem_expires > ngx_time()) {
        return info->mem_status;
    }

    info->mem_expires = ngx_time() + conf->interval;
    mem = &info->meminfo;
    info->mem_status = ngx_http_sysguard_getmeminfo(mem, r->connection->log);

    if (info->mem_status != NGX_OK) {
        return NGX_ERROR;
    }

    /* Fixed point, with two decimal places, matching swapratio=. */
    info->swap = mem->totalswap
                 ? (ngx_uint_t) ((long double) (mem->totalswap - mem->freeswap)
                                  * 10000 / mem->totalswap)
                 : 0;
    info->free = mem->freeram + mem->cachedram + mem->bufferram;

    return NGX_OK;
}


ngx_msec_t
ngx_http_sysguard_response_time(ngx_http_sysguard_conf_t *conf)
{
    return ngx_http_sysguard_rt_average(&conf->request_times,
                          (ngx_msec_t) conf->rt_period * 1000, conf->rt_method);
}


static ngx_http_sysguard_ctx_t *
ngx_http_sysguard_get_ctx(ngx_http_request_t *r, ngx_flag_t create)
{
    ngx_pool_cleanup_t       *cln;
    ngx_http_sysguard_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r->main, ngx_http_sysguard_module);
    if (ctx != NULL) {
        return ctx;
    }

    /* Internal redirects clear module contexts, but keep the request pool. */
    for (cln = r->main->pool->cleanup; cln; cln = cln->next) {
        if (cln->handler == ngx_http_sysguard_cleanup) {
            ngx_http_set_ctx(r->main, cln->data, ngx_http_sysguard_module);
            return cln->data;
        }
    }

    if (!create) {
        return NULL;
    }

    cln = ngx_pool_cleanup_add(r->main->pool, sizeof(ngx_http_sysguard_ctx_t));
    if (cln == NULL) {
        return NULL;
    }

    ctx = cln->data;
    ngx_memzero(ctx, sizeof(ngx_http_sysguard_ctx_t));
    cln->handler = ngx_http_sysguard_cleanup;
    ngx_http_set_ctx(r->main, ctx, ngx_http_sysguard_module);

    return ctx;
}


static void
ngx_http_sysguard_cleanup(void *data)
{
    /* The cleanup entry anchors state across internal redirects. */
}


static ngx_int_t
ngx_http_sysguard_test(ngx_http_request_t *r, ngx_http_sysguard_conf_t *conf,
    ngx_uint_t metric, double *value, double *limit, ngx_str_t **action)
{
    switch (metric) {

    case 0:
        if (conf->load == NGX_CONF_UNSET) {
            return NGX_DECLINED;
        }

        *action = &conf->load_action;
        if (ngx_http_sysguard_update_load(r, conf) != NGX_OK) {
            return NGX_DECLINED;
        }

        *value = conf->sysinfo.load / 1000.0;
        *limit = conf->load / 1000.0;
        break;

    case 4:
        if (conf->cpu == NGX_CONF_UNSET) {
            return NGX_DECLINED;
        }

        *action = &conf->cpu_action;
        if (ngx_http_sysguard_update_cpu(r, conf) != NGX_OK) {
            return NGX_DECLINED;
        }

        *value = conf->sysinfo.cpu / 100.0;
        *limit = conf->cpu / 100.0;
        break;

    case 1:
        if (conf->swap == NGX_CONF_UNSET) {
            return NGX_DECLINED;
        }

        *action = &conf->swap_action;
        if (ngx_http_sysguard_update_mem(r, conf) != NGX_OK
            || !conf->sysinfo.meminfo.swap_available)
        {
            return NGX_DECLINED;
        }

        *value = conf->sysinfo.swap / 100.0;
        *limit = conf->swap / 100.0;
        break;

    case 2:
        if (conf->free == NGX_CONF_UNSET_SIZE) {
            return NGX_DECLINED;
        }

        *action = &conf->free_action;
        if (ngx_http_sysguard_update_mem(r, conf) != NGX_OK) {
            return NGX_DECLINED;
        }

        *value = conf->sysinfo.free;
        *limit = conf->free;
        return *value < *limit ? NGX_OK : NGX_DECLINED;

    default:
        if (conf->rt == NGX_CONF_UNSET) {
            return NGX_DECLINED;
        }

        *action = &conf->rt_action;
        *value = ngx_http_sysguard_response_time(conf) / 1000.0;
        *limit = conf->rt / 1000.0;
        break;
    }

    return *value > *limit ? NGX_OK : NGX_DECLINED;
}


static ngx_int_t
ngx_http_sysguard_handler(ngx_http_request_t *r)
{
    double                          value, limit, last_value, last_limit;
    ngx_int_t                       rc;
    ngx_uint_t                      i, last;
    ngx_str_t                      *action, *path;
    ngx_http_sysguard_ctx_t         *ctx;
    ngx_http_sysguard_conf_t        *conf;
    ngx_http_sysguard_main_conf_t   *mcf;
    static char                    *names[] =
        { "load", "swap", "free", "rt", "cpu" };

    if (r != r->main) {
        return NGX_DECLINED;
    }

    conf = ngx_http_get_module_loc_conf(r, ngx_http_sysguard_module);
    mcf = ngx_http_get_module_main_conf(r, ngx_http_sysguard_module);

    if (!conf->enable && !mcf->rt_used) {
        return NGX_DECLINED;
    }

    ctx = ngx_http_sysguard_get_ctx(r, 1);
    if (ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ctx->checked) {
        return NGX_DECLINED;
    }

    if (ctx->conf == NULL) {
        ctx->conf = conf;
    }

    if (!conf->enable) {
        return NGX_DECLINED;
    }

    ctx->checked = 1;

    path = NULL;
    last = 0;
    last_value = 0;
    last_limit = 0;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        action = NULL;
        rc = ngx_http_sysguard_test(r, conf, i, &value, &limit, &action);

        if (action == NULL) {
            continue;
        }

        if (rc != NGX_OK) {
            if (conf->mode == NGX_HTTP_SYSGUARD_MODE_AND) {
                return NGX_DECLINED;
            }

            continue;
        }

        path = action;
        last = i;
        last_value = value;
        last_limit = limit;

        if (conf->mode == NGX_HTTP_SYSGUARD_MODE_OR) {
            break;
        }
    }

    if (path == NULL) {
        return NGX_DECLINED;
    }

    ctx->rejected = 1;
    ngx_log_error(conf->log_level, r->connection->log, 0,
                  "sysguard %s limited, current:%.3f conf:%.3f",
                  names[last], last_value, last_limit);

    if (path->len == 0) {
        return NGX_HTTP_SERVICE_UNAVAILABLE;
    }

    if (path->data[0] == '@') {
        (void) ngx_http_named_location(r, path);

    } else {
        (void) ngx_http_internal_redirect(r, path, &r->args);
    }

    ngx_http_finalize_request(r, NGX_DONE);
    return NGX_DONE;
}


static void *
ngx_http_sysguard_create_main_conf(ngx_conf_t *cf)
{
    return ngx_pcalloc(cf->pool, sizeof(ngx_http_sysguard_main_conf_t));
}


static void *
ngx_http_sysguard_create_conf(ngx_conf_t *cf)
{
    ngx_http_sysguard_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_sysguard_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;
    conf->cpu = NGX_CONF_UNSET;
    conf->cpu_period = NGX_CONF_UNSET;
    conf->load = NGX_CONF_UNSET;
    conf->swap = NGX_CONF_UNSET;
    conf->free = NGX_CONF_UNSET_SIZE;
    conf->rt = NGX_CONF_UNSET;
    conf->rt_period = NGX_CONF_UNSET;
    conf->rt_number = NGX_CONF_UNSET;
    conf->rt_method = NGX_CONF_UNSET;
    conf->interval = NGX_CONF_UNSET;
    conf->log_level = NGX_CONF_UNSET_UINT;
    conf->mode = NGX_CONF_UNSET_UINT;

    return conf;
}


static char *
ngx_http_sysguard_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_sysguard_conf_t *prev = parent;
    ngx_http_sysguard_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_str_value(conf->cpu_action, prev->cpu_action, "");
    ngx_conf_merge_value(conf->cpu, prev->cpu, NGX_CONF_UNSET);
    ngx_conf_merge_value(conf->cpu_period, prev->cpu_period, 3);
    ngx_conf_merge_str_value(conf->load_action, prev->load_action, "");
    ngx_conf_merge_str_value(conf->swap_action, prev->swap_action, "");
    ngx_conf_merge_str_value(conf->free_action, prev->free_action, "");
    ngx_conf_merge_str_value(conf->rt_action, prev->rt_action, "");
    ngx_conf_merge_value(conf->load, prev->load, NGX_CONF_UNSET);
    ngx_conf_merge_value(conf->swap, prev->swap, NGX_CONF_UNSET);
    ngx_conf_merge_size_value(conf->free, prev->free, NGX_CONF_UNSET_SIZE);
    ngx_conf_merge_value(conf->rt, prev->rt, NGX_CONF_UNSET);
    ngx_conf_merge_value(conf->rt_period, prev->rt_period, 1);
    ngx_conf_merge_value(conf->rt_number, prev->rt_number, NGX_CONF_UNSET);
    ngx_conf_merge_value(conf->rt_method, prev->rt_method,
                         NGX_HTTP_SYSGUARD_AVERAGE_AMM);
    ngx_conf_merge_value(conf->interval, prev->interval, 1);
    ngx_conf_merge_uint_value(conf->log_level, prev->log_level, NGX_LOG_ERR);
    ngx_conf_merge_uint_value(conf->mode, prev->mode,
                              NGX_HTTP_SYSGUARD_MODE_OR);

    if (conf->interval > NGX_MAX_INT_T_VALUE / 1000) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "sysguard_interval is too large");
        return NGX_CONF_ERROR;
    }

    conf->request_times.capacity = (conf->rt_number == NGX_CONF_UNSET)
                                  ? conf->rt_period : conf->rt_number;

    if (conf->request_times.capacity
        > NGX_MAX_SIZE_T_VALUE / sizeof(ngx_http_sysguard_sample_t))
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "too many sysguard RT samples");
        return NGX_CONF_ERROR;
    }

    conf->request_times.samples = ngx_pcalloc(cf->pool,
              conf->request_times.capacity
                         * sizeof(ngx_http_sysguard_sample_t));
    if (conf->request_times.samples == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_sysguard_action(ngx_str_t *value, ngx_str_t *action)
{
    if (value->len <= 7 || ngx_strncmp(value->data, "action=", 7) != 0
        || (value->data[7] != '/' && value->data[7] != '@')
        || (value->data[7] == '@' && value->len == 8))
    {
        return NGX_ERROR;
    }

    action->data = value->data + 7;
    action->len = value->len - 7;
    return NGX_OK;
}


static char *
ngx_http_sysguard_load(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_sysguard_conf_t *glcf = conf;

    ngx_str_t   *value, number;
    ngx_uint_t   i, scale;

    value = cf->args->elts;
    i = 1;
    scale = 1;

    if (glcf->load != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    if (value[i].len <= 5 || ngx_strncmp(value[i].data, "load=", 5) != 0) {
        goto invalid;
    }

    number.data = value[i].data + 5;
    number.len = value[i].len - 5;

    if (number.len > 5 && ngx_strncmp(number.data, "ncpu*", 5) == 0) {
        number.data += 5;
        number.len -= 5;
        scale = ngx_max(ngx_ncpu, 1);
    }

    glcf->load = ngx_atofp(number.data, number.len, 3);
    if (glcf->load == NGX_ERROR
        || (ngx_uint_t) glcf->load > NGX_MAX_INT_T_VALUE / scale)
    {
        goto invalid;
    }

    glcf->load *= scale;

    if (cf->args->nelts == 3) {
        i++;
        if (ngx_http_sysguard_action(&value[i], &glcf->load_action) != NGX_OK) {
            goto invalid;
        }
    }

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);
    return NGX_CONF_ERROR;
}


static char *
ngx_http_sysguard_cpu(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_sysguard_conf_t *glcf = conf;

    ngx_str_t   *value, period;
    ngx_uint_t   i;

    value = cf->args->elts;

    if (glcf->cpu != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    for (i = 1; i < cf->args->nelts; i++) {
        if (value[i].len > 6 && ngx_strncmp(value[i].data, "usage=", 6) == 0) {
            if (glcf->cpu != NGX_CONF_UNSET) {
                return "is duplicate";
            }

            glcf->cpu = ngx_atofp(value[i].data + 6, value[i].len - 6, 2);
            if (glcf->cpu == NGX_ERROR || glcf->cpu > 10000) {
                goto invalid;
            }

            continue;
        }

        if (value[i].len > 7 && ngx_strncmp(value[i].data, "period=", 7) == 0) {
            if (glcf->cpu_period != NGX_CONF_UNSET) {
                return "is duplicate";
            }

            period.data = value[i].data + 7;
            period.len = value[i].len - 7;
            glcf->cpu_period = ngx_parse_time(&period, 1);
            if (glcf->cpu_period <= 0
                || glcf->cpu_period > NGX_MAX_INT_T_VALUE / 1000)
            {
                goto invalid;
            }

            continue;
        }

        if (value[i].len > 7 && ngx_strncmp(value[i].data, "action=", 7) == 0) {
            if (glcf->cpu_action.data != NULL) {
                return "is duplicate";
            }

            if (ngx_http_sysguard_action(&value[i], &glcf->cpu_action)
                != NGX_OK)
            {
                goto invalid;
            }

            continue;
        }

        goto invalid;
    }

    if (glcf->cpu == NGX_CONF_UNSET) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "sysguard_cpu requires a usage= threshold");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);
    return NGX_CONF_ERROR;
}


static char *
ngx_http_sysguard_mem(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_sysguard_conf_t *glcf = conf;

    ngx_str_t   *value, size, *action;
    ngx_uint_t   i;

    value = cf->args->elts;
    i = 1;

    if (value[i].len > 11
        && ngx_strncmp(value[i].data, "swapratio=", 10) == 0
        && value[i].data[value[i].len - 1] == '%')
    {
        if (glcf->swap != NGX_CONF_UNSET) {
            return "is duplicate";
        }

        glcf->swap = ngx_atofp(value[i].data + 10, value[i].len - 11, 2);
        if (glcf->swap == NGX_ERROR || glcf->swap > 10000) {
            goto invalid;
        }

        action = &glcf->swap_action;

    } else if (value[i].len > 5
               && ngx_strncmp(value[i].data, "free=", 5) == 0)
    {
        if (glcf->free != NGX_CONF_UNSET_SIZE) {
            return "is duplicate";
        }

        size.data = value[i].data + 5;
        size.len = value[i].len - 5;
        glcf->free = ngx_parse_size(&size);
        if (glcf->free == (size_t) NGX_ERROR) {
            goto invalid;
        }

        action = &glcf->free_action;

    } else {
        goto invalid;
    }

    if (cf->args->nelts == 3) {
        i++;
        if (ngx_http_sysguard_action(&value[i], action) != NGX_OK) {
            goto invalid;
        }
    }

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);
    return NGX_CONF_ERROR;
}


static char *
ngx_http_sysguard_rt(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_sysguard_conf_t *glcf = conf;

    ngx_str_t   *value, s;
    ngx_uint_t   i;

    value = cf->args->elts;

    if (glcf->rt != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    for (i = 1; i < cf->args->nelts; i++) {
        if (value[i].len > 3 && ngx_strncmp(value[i].data, "rt=", 3) == 0) {
            if (glcf->rt != NGX_CONF_UNSET) {
                return "is duplicate";
            }

            glcf->rt = ngx_atofp(value[i].data + 3, value[i].len - 3, 3);
            if (glcf->rt == NGX_ERROR) {
                goto invalid;
            }

            continue;
        }

        if (value[i].len > 7 && ngx_strncmp(value[i].data, "period=", 7) == 0) {
            if (glcf->rt_period != NGX_CONF_UNSET) {
                return "is duplicate";
            }

            s.data = value[i].data + 7;
            s.len = value[i].len - 7;
            glcf->rt_period = ngx_parse_time(&s, 1);
            if (glcf->rt_period <= 0
                || glcf->rt_period > NGX_MAX_INT_T_VALUE / 1000)
            {
                goto invalid;
            }

            continue;
        }

        if (value[i].len >= 10
            && ngx_strncmp(value[i].data, "method=", 7) == 0)
        {
            if (glcf->rt_method != NGX_CONF_UNSET) {
                return "is duplicate";
            }

            s.data = value[i].data + 7;
            s.len = value[i].len - 7;
            if (ngx_strncmp(s.data, "AMM", 3) == 0) {
                glcf->rt_method = NGX_HTTP_SYSGUARD_AVERAGE_AMM;

            } else if (ngx_strncmp(s.data, "WMA", 3) == 0) {
                glcf->rt_method = NGX_HTTP_SYSGUARD_AVERAGE_WMA;

            } else {
                goto invalid;
            }

            if (s.len == 3) {
                continue;
            }

            if (s.data[3] != ':') {
                goto invalid;
            }

            glcf->rt_number = ngx_atoi(s.data + 4, s.len - 4);
            if (glcf->rt_number <= 0) {
                goto invalid;
            }

            continue;
        }

        if (value[i].len > 7 && ngx_strncmp(value[i].data, "action=", 7) == 0) {
            if (glcf->rt_action.data != NULL) {
                return "is duplicate";
            }

            if (ngx_http_sysguard_action(&value[i], &glcf->rt_action)
                != NGX_OK)
            {
                goto invalid;
            }

            continue;
        }

        goto invalid;
    }

    if (glcf->rt == NGX_CONF_UNSET) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "sysguard_rt requires an rt= threshold");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);
    return NGX_CONF_ERROR;
}


static ngx_int_t
ngx_http_sysguard_log_handler(ngx_http_request_t *r)
{
    ngx_time_t                      *tp;
    ngx_msec_int_t                   elapsed;
    ngx_http_sysguard_ctx_t         *ctx;
    ngx_http_sysguard_conf_t        *conf;
    ngx_http_sysguard_main_conf_t   *mcf;

    if (r != r->main) {
        return NGX_OK;
    }

    ctx = ngx_http_sysguard_get_ctx(r, 0);
    if (ctx != NULL && ctx->rejected) {
        return NGX_OK;
    }

    conf = ctx ? ctx->conf
               : ngx_http_get_module_loc_conf(r, ngx_http_sysguard_module);
    mcf = ngx_http_get_module_main_conf(r, ngx_http_sysguard_module);

    if (!mcf->rt_used && (!conf->enable || conf->rt == NGX_CONF_UNSET)) {
        return NGX_OK;
    }

    tp = ngx_timeofday();
    elapsed = (ngx_msec_int_t) ((tp->sec - r->start_sec) * 1000
                               + tp->msec - r->start_msec);
    ngx_http_sysguard_rt_add(&conf->request_times, ngx_max(elapsed, 0));

    return NGX_OK;
}


static ngx_int_t
ngx_http_sysguard_init(ngx_conf_t *cf)
{
    ngx_uint_t                      i;
    ngx_http_handler_pt            *h;
    ngx_http_variable_t            *v;
    ngx_http_core_main_conf_t      *cmcf;
    ngx_http_sysguard_main_conf_t  *mcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_sysguard_module);
    v = cmcf->variables.elts;

    for (i = 0; i < cmcf->variables.nelts; i++) {
        if (v[i].name.len == sizeof("sysguard_rt") - 1
            && ngx_strncmp(v[i].name.data, "sysguard_rt", v[i].name.len) == 0)
        {
            mcf->rt_used = 1;
            break;
        }
    }

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_sysguard_handler;

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_sysguard_log_handler;
    return NGX_OK;
}
