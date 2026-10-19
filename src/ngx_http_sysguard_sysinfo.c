/*
 * Copyright (C) 2010-2015 Alibaba Group Holding Limited
 * Copyright (C) YoungJoo Kim (vozlt)
 * Copyright (C) Hanada
 */


#include "ngx_http_sysguard_module.h"

#if (NGX_HAVE_SYSINFO)
#include <sys/sysinfo.h>
#endif

#if (NGX_FREEBSD)
#include <sys/sysctl.h>
#endif


ngx_int_t
ngx_http_sysguard_getloadavg(ngx_int_t *load, ngx_log_t *log)
{
#if (NGX_HAVE_GETLOADAVG)
    double  avg;

    if (getloadavg(&avg, 1) != 1 || avg < 0
        || avg > NGX_MAX_INT_T_VALUE / 1000)
    {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno, "getloadavg() failed");
        return NGX_ERROR;
    }

    *load = (ngx_int_t) (avg * 1000);
    return NGX_OK;

#elif (NGX_HAVE_SYSINFO)
    struct sysinfo  info;

    if (sysinfo(&info) == -1) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno, "sysinfo() failed");
        return NGX_ERROR;
    }

    *load = (ngx_int_t) ((double) info.loads[0] * 1000 / 65536);
    return NGX_OK;

#else
    return NGX_ERROR;
#endif
}


#if (NGX_HAVE_PROC_MEMINFO)

static ssize_t
ngx_http_sysguard_read_file(char *name, u_char *buf, size_t size,
    ngx_log_t *log)
{
    ssize_t   n;
    ngx_fd_t  fd;

    fd = ngx_open_file(name, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", name);
        return NGX_ERROR;
    }

    do {
        n = ngx_read_fd(fd, buf, size);
    } while (n == -1 && ngx_errno == NGX_EINTR);

    if (n == -1) {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno,
                      ngx_read_fd_n " \"%s\" failed", name);
    }

    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", name);
    }

    return n;
}


ngx_int_t
ngx_http_sysguard_getmeminfo(ngx_http_sysguard_meminfo_t *mem, ngx_log_t *log)
{
    u_char      *p, *last, *end, *value, *digits;
    size_t      *field;
    ssize_t      n, size;
    ngx_uint_t   i, seen;
    u_char       buf[4096];
    static struct {
        ngx_str_t  name;
        size_t     offset;
    } fields[] = {
        { ngx_string("MemTotal:"),
          offsetof(ngx_http_sysguard_meminfo_t, totalram) },
        { ngx_string("MemFree:"),
          offsetof(ngx_http_sysguard_meminfo_t, freeram) },
        { ngx_string("Buffers:"),
          offsetof(ngx_http_sysguard_meminfo_t, bufferram) },
        { ngx_string("Cached:"),
          offsetof(ngx_http_sysguard_meminfo_t, cachedram) },
        { ngx_string("SwapTotal:"),
          offsetof(ngx_http_sysguard_meminfo_t, totalswap) },
        { ngx_string("SwapFree:"),
          offsetof(ngx_http_sysguard_meminfo_t, freeswap) }
    };

    ngx_memzero(mem, sizeof(ngx_http_sysguard_meminfo_t));
    n = ngx_http_sysguard_read_file("/proc/meminfo", buf, sizeof(buf), log);
    if (n <= 0) {
        return NGX_ERROR;
    }

    seen = 0;
    last = buf + n;

    for (p = buf; p < last; p = end + 1) {
        end = ngx_strlchr(p, last, '\n');
        if (end == NULL) {
            break;
        }

        for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
            if ((size_t) (end - p) < fields[i].name.len
                || ngx_strncmp(p, fields[i].name.data, fields[i].name.len) != 0)
            {
                continue;
            }

            if (seen & (1 << i)) {
                goto invalid;
            }

            value = p + fields[i].name.len;
            while (value < end && (*value == ' ' || *value == '\t')) {
                value++;
            }

            digits = value;
            while (value < end && *value >= '0' && *value <= '9') {
                value++;
            }

            size = ngx_atosz(digits, value - digits);
            if (size == NGX_ERROR
                || (size_t) size > NGX_MAX_SIZE_T_VALUE / 1024)
            {
                goto invalid;
            }

            while (value < end && (*value == ' ' || *value == '\t')) {
                value++;
            }

            if (end - value != 2 || ngx_strncmp(value, "kB", 2) != 0) {
                goto invalid;
            }

            field = (size_t *) ((u_char *) mem + fields[i].offset);
            *field = (size_t) size * 1024;
            seen |= 1 << i;
            break;
        }
    }

    if (seen != 63 || mem->freeswap > mem->totalswap
        || mem->cachedram > NGX_MAX_SIZE_T_VALUE - mem->freeram
        || mem->bufferram
           > NGX_MAX_SIZE_T_VALUE - mem->freeram - mem->cachedram)
    {
        goto invalid;
    }

    mem->swap_available = 1;
    return NGX_OK;

invalid:

    ngx_log_error(NGX_LOG_ERR, log, 0, "invalid /proc/meminfo data");
    return NGX_ERROR;
}


#elif (NGX_FREEBSD)

static ngx_int_t
ngx_http_sysguard_sysctl(char *name, size_t *value)
{
    size_t  len;
    union {
        uint32_t  u32;
        uint64_t  u64;
    } data;

    ngx_memzero(&data, sizeof(data));
    len = sizeof(data);

    if (sysctlbyname(name, &data, &len, NULL, 0) == -1) {
        return NGX_ERROR;
    }

    if (len == sizeof(uint32_t)) {
        *value = data.u32;

    } else if (len == sizeof(uint64_t) && data.u64 <= NGX_MAX_SIZE_T_VALUE) {
        *value = (size_t) data.u64;

    } else {
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_sysguard_getmeminfo(ngx_http_sysguard_meminfo_t *mem, ngx_log_t *log)
{
    ngx_memzero(mem, sizeof(ngx_http_sysguard_meminfo_t));

    if (ngx_http_sysguard_sysctl("hw.physmem", &mem->totalram) != NGX_OK
        || ngx_http_sysguard_sysctl("vm.stats.vm.v_free_count", &mem->freeram)
           != NGX_OK)
    {
        ngx_log_error(NGX_LOG_ERR, log, ngx_errno,
                      "sysctlbyname() failed to read memory information");
        return NGX_ERROR;
    }

    /* These optional counters are absent on some FreeBSD releases. */
    (void) ngx_http_sysguard_sysctl("vm.stats.vm.v_cache_count",
                                    &mem->cachedram);
    (void) ngx_http_sysguard_sysctl("vfs.bufspace", &mem->bufferram);

    if (mem->freeram > NGX_MAX_SIZE_T_VALUE / ngx_pagesize
        || mem->cachedram > NGX_MAX_SIZE_T_VALUE / ngx_pagesize)
    {
        return NGX_ERROR;
    }

    mem->freeram *= ngx_pagesize;
    mem->cachedram *= ngx_pagesize;

    if (mem->cachedram > NGX_MAX_SIZE_T_VALUE - mem->freeram
        || mem->bufferram
           > NGX_MAX_SIZE_T_VALUE - mem->freeram - mem->cachedram)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}


#else

ngx_int_t
ngx_http_sysguard_getmeminfo(ngx_http_sysguard_meminfo_t *mem, ngx_log_t *log)
{
    return NGX_ERROR;
}

#endif
