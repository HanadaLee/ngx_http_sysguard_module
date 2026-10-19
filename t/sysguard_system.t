#!/usr/bin/perl

# Copyright (C) Hanada

use warnings;
use strict;
use Test::More;
use Test::Nginx;

my $fixtures = $ENV{TEST_SYSGUARD_FIXTURES};
plan(skip_all => 'requires isolated proc fixtures') unless $fixtures;
plan 'no_plan';

my $t = Test::Nginx->new();
memory(7975);
$t->write_file_expand('nginx.conf', <<'EOF');
%%TEST_GLOBALS%%
daemon off;
events {}
http {
    %%TEST_GLOBALS_HTTP%%
    server {
        listen 127.0.0.1:8080;
        root %%TESTDIR%%;
        sysguard_interval 0;
        location /metrics {
            return 200 "$sysguard_swapstat|$sysguard_free|$sysguard_meminfo_totalram|$sysguard_meminfo_freeswap";
        }
        location /swap {
            sysguard on;
            sysguard_mem swapratio=20.00%;
        }
        location /boundary {
            sysguard on;
            sysguard_mem swapratio=20.25%;
        }
        location /free {
            sysguard on;
            sysguard_mem free=200K;
        }
    }
}
EOF
$t->write_file($_, "ok\n") for qw/swap boundary free/;
$t->run();

like(http_get('/metrics'), qr/20\|179200\|1048576\|8166400$/, 'variables collect precise fixture data');
like(http_get('/swap'), qr/503 Service/, '20.25 percent exceeds 20.00 percent');
like(http_get('/boundary'), qr/200 OK/, 'equal swap threshold does not trigger');
like(http_get('/free'), qr/503 Service/, 'free memory uses free plus buffers plus cache');
memory(8000);
like(http_get('/swap'), qr/200 OK/, '20.00 percent equals threshold');
memory(10000);
like(http_get('/metrics'), qr/0\|179200\|1048576\|10240000$/, 'variables refresh without enabled thresholds');
memory(0, 0);
like(http_get('/swap'), qr/200 OK/, 'no swap is zero usage');

for my $bad (
    "MemTotal: 1 kB\n",
    "MemTotal: -1 kB\n",
    "MemTotal: 999999999999999999999999999 kB\n",
    "MemTotal: 100 bananas\n",
    "MemTotal: 1024 kB\nMemTotal: 1024 kB\n",
) {
    write_fixture('meminfo', $bad);
    like(http_get('/free'), qr/200 OK/, 'invalid memory data fails open');
    like(http_get('/metrics'), qr/\r\n\r\n\|\|\|$/, 'unavailable values are empty, not stale');
}
memory(11000);
like(http_get('/swap'), qr/200 OK/, 'inconsistent swap counters fail open');
memory(7975);
like(http_get('/swap'), qr/503 Service/, 'sampling recovers after invalid data');

$t->stop();
unlike($t->read_file('error.log'), qr/exited on signal|\[alert\]|Sanitizer/, 'no worker or sanitizer errors');

sub write_fixture {
    my ($name, $data) = @_;
    open my $file, '>', "$fixtures/$name" or die $!;
    print $file $data;
    close $file or die $!;
}

sub memory {
    my ($free, $total) = @_;
    $total //= 10000;
    write_fixture('meminfo', "MemTotal: 1024 kB\nMemFree: 100 kB\nBuffers: 25 kB\nCached: 50 kB\nSwapTotal: $total kB\nSwapFree: $free kB\n");
}
