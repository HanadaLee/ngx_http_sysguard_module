Nginx sysguard module
==========

[![License](http://img.shields.io/badge/license-BSD-brightgreen.svg)](https://github.com/HanadaLee/ngx_http_sysguard_module/blob/master/LICENSE)

Nginx sysguard module

Table of Contents
=================

* [Version](#version)
* [Dependencies](#dependencies)
* [Compatibility](#compatibility)
* [Installation](#installation)
* [Synopsis](#synopsis)
* [Description](#description)
* [Embedded Variables](#embedded-variables)
* [Directives](#directives)
  * [sysguard](#sysguard)
  * [sysguard_load](#sysguard_load)
  * [sysguard_cpu](#sysguard_cpu)
  * [sysguard_mem](#sysguard_mem)
  * [sysguard_rt](#sysguard_rt)
  * [sysguard_mode](#sysguard_mode)
  * [sysguard_interval](#sysguard_interval)
  * [sysguard_log_level](#sysguard_log_level)
* [Testing](#testing)
* [See Also](#see-also)
* [TODO](#todo)
* [Donation](#donation)
* [Author](#Author)

## Version
This document describes ngx_http_sysguard_module `0.2.0`.

## Dependencies
* nginx
* getloadavg(3) or sysinfo(2)
* Linux /proc/meminfo and /proc/stat for memory and CPU protection

## Compatibility
Linux is covered by the nginx 1.31.7 static, dynamic, sanitizer, and nginx-ext CI builds.
FreeBSD load and memory support is best effort. Optional memory counters may be
unavailable, and CPU usage and swap monitoring are not implemented on FreeBSD.
FreeBSD is not part of the tested CI matrix.

## Installation

1. Clone the git repository.

  ```
  shell> git clone --branch 0.2.0 https://git.hanada.info/hanada/ngx_http_sysguard_module.git
  ```

2. Add the module to the build configuration by adding
  ```
  --add-module=/path/to/ngx_http_sysguard_module
  ```

3. Build the nginx binary.

4. Install the nginx binary.

## Synopsis

```Nginx
http {

    ...

    server {

        ...

        sysguard on;
        sysguard_mode or;

        sysguard_load load=10.5 action=/loadlimit;
        sysguard_mem swapratio=20% action=/swaplimit;
        sysguard_mem free=100M action=/freelimit;
        sysguard_rt rt=0.01 period=5s method=AMM:10 action=/rtlimit;

        location /loadlimit {
            return 503;
        }

        location /swaplimit {
            return 503;
        }

        location /freelimit {
            return 503;
        }

        location /rtlimit {
            return 503;
        }
    }

    ...

    server {

        ...

        location /api {
            sysguard on;
            sysguard_mode or;
            sysguard_load load=20 action=/limit;
            sysguard_mem swapratio=10% action=/limit;
            sysguard_rt rt=2.01 period=5s method=WMA:10 action=/limit;

            ... 

        }

        location /images {
            sysguard on;
            sysguard_mode and;
            sysguard_load load=20 action=/limit;
            sysguard_mem swapratio=10% action=/limit;
            sysguard_rt rt=2.01 period=5s method=WMA:10 action=/limit;

            ...

        }

        location /limit {
            return 503;
        }
    }

}
```

## Description
This module can be used to protect your server in case system load, memory use goes too high or requests are responded too slow.
This standalone module is derived from [vozlt/nginx-module-sysguard](https://github.com/vozlt/nginx-module-sysguard) and [Tengine sysguard](https://github.com/alibaba/tengine/tree/master/modules/ngx_http_sysguard_module). It builds against nginx without a core patch.

Protection runs once per main request. Subrequests and action redirects are not
protected again. RT samples belong to the location where the main request first
reaches the protection phase; rejected requests are excluded from RT samples.

System metrics and RT samples are local to each worker and effective location
configuration. They are not aggregated across workers. Linux resource values are
system values, not cgroup quota measurements. Failed or unavailable measurements
do not trigger protection; in `and` mode every configured condition must match.

## Embedded Variables
System variables collect their own data when read, even with `sysguard off` or
without a corresponding threshold. `sysguard_interval` controls load and memory
refreshes. CPU samples use the CPU `period` (3 seconds by default) and need two
valid snapshots; unavailable values, including CPU warm-up, are empty strings.
Referencing `$sysguard_rt` enables RT collection without enabling protection.

The following embedded variables are provided:

* **$sysguard_load**
  * The load of system. If `$sysguard_load`'s value is 100, then load is 0.1 (100/1000). This is load multiplied by 1000, not a time unit.
* **$sysguard_cpu**
  * CPU usage as a percentage with two decimal places, for example `80.25`.
* **$sysguard_swapstat**
  * Used swap as a whole-number percentage (0 to 100), preserving the original variable unit. Threshold comparisons retain two decimal places.
* **$sysguard_free**
  * The real free space of memory. (/byte)
* **$sysguard_rt**
  * The average of request processing times. If `$sysguard_rt`'s value is 100, then response time is 0.1sec(100/1000). (/msec)
* **$sysguard_meminfo_totalram**
  * The total memory of meminfo. (/byte)
* **$sysguard_meminfo_freeram**
  * The free memory of meminfo. (/byte)
* **$sysguard_meminfo_bufferram**
  * The buffer memory of meminfo. (/byte)
* **$sysguard_meminfo_cachedram**
  * The cached memory of meminfo. (/byte)
* **$sysguard_meminfo_totalswap**
  * The total swap of meminfo. (/byte)
* **$sysguard_meminfo_freeswap**
  * The free swap of meminfo. (/byte)


## Directives

### sysguard

| -   | - |
| --- | --- |
| **Syntax**  | **sysguard** \<on\|off\> |
| **Default** | off |
| **Context** | http, server, location |

`Description:` Enables or disables request protection. Reading diagnostic variables still collects metrics when protection is disabled.

### sysguard_load

| -   | - |
| --- | --- |
| **Syntax**  | **sysguard_load** load=*number* [action=*/url*] |
| **Default** | - |
| **Context** | http, server, location |

`Description:` Specify the load threshold. When the system load exceeds this threshold, all subsequent requests will be redirected to the URL specified by the 'action' parameter. It will return 503 if there's no 'action' URL defined. Use `load=ncpu*1.5` to multiply the threshold by the number of CPUs.

### sysguard_cpu

| - | - |
| --- | --- |
| **Syntax** | **sysguard_cpu** usage=*number* [period=*time*] [action=*/url*] |
| **Default** | no threshold; period=3s |
| **Context** | http, server, location |

Sets the Linux CPU usage threshold from 0 to 100, with up to two decimal places.
For example, `sysguard_cpu usage=80 period=3s action=/limit;` protects above 80%.
The period must be positive. The first read establishes a baseline; a later read
after the period calculates usage. Counter resets, invalid data, and intervals
with no counter changes are unavailable rather than treated as zero usage.

The calculation follows Tengine: the change in user + nice + system time divided
by the change in user + nice + system + idle + iowait + irq + softirq time.
The `action` and `sysguard_mode` behavior is the same as other thresholds.

### sysguard_mem

| -   | - |
| --- | --- |
| **Syntax**  | **sysguard_mem** swapratio=*ratio*% [action=*/url*]<br>**sysguard_mem** free=*size* [action=*/url*] |
| **Default** | - |
| **Context** | http, server, location |

`Description:` Specify the used swap memory or free memory threshold. When the swap memory use ratio exceeds this threshold or memory free less than the size, all subsequent requests will be redirected to the URL specified by the 'action' parameter. It will return 503 if there's no 'action' URL. Sysguard uses this strategy to calculate memory free: "memfree = free + buffered + cached"

Swap thresholds range from 0% to 100% with up to two decimal places. Configure
swap and free-memory limits with separate directives.

### sysguard_rt

| -   | - |
| --- | --- |
| **Syntax**  | **sysguard_rt** rt=*second* [period=*time*] [method=\<AMM\|WMA\>:*number*] [action=*/url*] |
| **Default** | - |
| **Context** | http, server, location |

`Description:` Specify the response time threshold.
Parameter rt is used to set a threshold of the average response time, in second.
Parameter period is used to specify the period of the statistics cycle.
If the average response time of the system exceeds the threshold specified by the user,
the incoming request will be redirected to a specified url which is defined by parameter 'action'.
If no 'action' is presented, the request will be responsed with 503 error directly.
The `method` is a formula that calculate the average of response processing times.
The `number` in method is the number of samples to calculate the average.
The period defaults to 1 second and the default sample count equals the period
in seconds, unless an explicit count is inherited. A count of 1 is valid. Counts
and periods must be positive. `method=AMM:N` and `method=WMA:N` retain exactly N
completed requests, excluding samples older than the period. AMM divides by the
actual number of valid samples. WMA assigns weights 1 through that number, from
oldest to newest. An empty or expired window has RT zero and permits recovery.
Unknown parameters, including misspelled `action`, are configuration errors.

* **AMM**
  * The AMM is the [arithmetic mean](https://en.wikipedia.org/wiki/Arithmetic_mean).
* **WMA**
  * THE WMA is the [weighted moving average](https://en.wikipedia.org/wiki/Moving_average#Weighted_moving_average).

### sysguard_mode

| -   | - |
| --- | --- |
| **Syntax**  | **sysguard_mode** \<and\|or\> |
| **Default** | or |
| **Context** | http, server, location |

`Description:` If there are more than one type of monitor, this directive is used to specified the relations among all the monitors which are: 'and' for all matching and 'or' for any matching.

### sysguard_interval

| -   | - |
| --- | --- |
| **Syntax**  | **sysguard_interval** *time* |
| **Default** | 1s |
| **Context** | http, server, location |

`Description:` Specify the time interval to update your system information.
The default value is one second, which means sysguard updates the server status once a second.

### sysguard_log_level

| -   | - |
| --- | --- |
| **Syntax**  | **sysguard_log_level** \<info\|notice\|warn\|error\> |
| **Default** | error |
| **Context** | http, server, location |

`Description:` Specify the log level of sysguard.

## Testing

The GitHub Actions workflow builds static and dynamic modules, nginx-ext, and an
AddressSanitizer/UndefinedBehaviorSanitizer variant. Tests use the official
`nginx/nginx-tests` Perl harness and send requests to real nginx processes.
Linux memory and CPU cases use fixed `/proc` data inside a private mount namespace;
they do not create host load or consume swap.

To reproduce CI, place the pinned nginx, nginx-tests, and optional ngx_expr_module
checkouts from `.github/workflows/test.yml` under `.ci/`, then run:

```sh
sudo bash util/run-tests.sh
```

Set `TEST_MODULE_LINK=dynamic`, `TEST_SANITIZE=enabled`, or `TEST_EXPR=enabled`
for the corresponding matrix variant. Logs and phase exit codes are saved under
`.ci/test-output/`. The fixture suite requires root and mount-namespace support;
CI fails if it cannot run. Leak detection is disabled for nginx worker teardown;
address and undefined-behavior diagnostics remain fatal.

## See Also
* [nginx-module-vts](https://github.com/vozlt/nginx-module-vts)
* [nginx-module-sts](https://github.com/vozlt/nginx-module-sts)

## TODO

## Donation
[![License](http://img.shields.io/badge/PAYPAL-DONATE-yellow.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=PWWSYKQ9VKH38&lc=KR&currency_code=USD&bn=PP%2dDonationsBF%3abtn_donateCC_LG%2egif%3aNonHosted)

## Author 
* Copyright (C) 2010-2015 Alibaba Group Holding Limited
* Copyright (C) 2017, YoungJoo.Kim \<vozltx@gmail.com\>
* Copyright (C) Hanada
