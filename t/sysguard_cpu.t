#!/usr/bin/perl

# Copyright (C) Hanada

use warnings;
use strict;
use Time::HiRes qw/sleep/;
use Test::More;
use Test::Nginx;

my $fixtures = $ENV{TEST_SYSGUARD_FIXTURES};
plan(skip_all => 'requires isolated proc fixtures') unless $fixtures;
plan 'no_plan';

my $t = Test::Nginx->new();
stat_data('cpu 100 0 0 100 0 0 0 0 0 0');
$t->write_file_expand('nginx.conf', <<'EOF');
%%TEST_GLOBALS%%
daemon off;
events {}
http {
    %%TEST_GLOBALS_HTTP%%
    server {
        listen 127.0.0.1:8080;
        root %%TESTDIR%%;
        sysguard on;
        sysguard_cpu usage=50 period=1;
        add_header X-CPU $sysguard_cpu always;
        location /cpu {}
        location /and {
            sysguard_mode and;
            sysguard_mem free=1K;
        }
        location /and_trigger {
            sysguard_mode and;
            sysguard_mem free=1048576M;
        }
        location /or {
            sysguard_mode or;
            sysguard_mem free=1K;
        }
        location /action {
            sysguard_cpu usage=50 period=1 action=/limited;
        }
        location /limited {}
        location /off {
            sysguard off;
        }
        location /boundary {
            sysguard_cpu usage=80 period=1;
        }
    }
    server {
        listen 127.0.0.1:8081;
        location / {
            return 200 "[$sysguard_cpu]";
        }
    }
}
EOF
$t->write_file($_, "ok\n") for qw/cpu and and_trigger or action off boundary/;
$t->write_file('limited', "limited\n");
$t->run();

for my $path (qw/cpu and and_trigger or action off boundary/) {
    my $r = http_get("/$path");
    like($r, qr/200 OK/, "$path warm-up fails open");
    unlike($r, qr/X-CPU:/i, "$path has no invented warm-up value");
}
like(variable(), qr/\[\]$/, 'CPU variable alone starts sampling');
stat_data('cpu 180 0 0 120 0 0 0 0 0 0');
sleep 1.1;
my $r = http_get('/cpu');
like($r, qr/503 Service/, 'CPU threshold triggers at 80 percent');
like($r, qr/X-CPU: 80\.00\r/i, 'CPU variable uses two decimal places');
like(http_get('/and'), qr/200 OK/, 'CPU respects and mode');
like(http_get('/and_trigger'), qr/503 Service/, 'CPU and memory thresholds both match');
like(http_get('/or'), qr/503 Service/, 'CPU alone can satisfy or mode');
like(http_get('/action'), qr/limited\n$/, 'CPU action does not loop under inherited protection');
like(http_get('/off'), qr/200 OK/, 'CPU protection can be disabled');
like(http_get('/boundary'), qr/200 OK/, 'equal CPU threshold does not trigger');
sleep 2.1;
like(variable(), qr/\[80\.00\]$/, 'CPU variable refreshes without a threshold');

stat_data('cpu 190 0 0 210 0 0 0');
sleep 1.1;
$r = http_get('/cpu');
like($r, qr/200 OK/, 'CPU protection recovers at 10 percent');
like($r, qr/X-CPU: 10\.00\r/i, 'new CPU delta replaces previous value');
like(http_get('/and_trigger'), qr/200 OK/, 'and mode recovers when CPU is below threshold');
like(http_get('/or'), qr/200 OK/, 'or mode recovers when neither threshold matches');
sleep 1.1;
unlike(http_get('/cpu'), qr/X-CPU:/i, 'unchanged counters are unavailable');

stat_data('cpu 10 0 0 10 0 0 0');
sleep 1.1;
unlike(http_get('/cpu'), qr/X-CPU:/i, 'counter regression resets the baseline');
stat_data('cpu 90 0 0 30 0 0 0');
sleep 1.1;
like(http_get('/cpu'), qr/X-CPU: 80\.00\r/i, 'sampling recovers from counter regression');

for my $bad (
    'cpu0 1 2 3 4 5 6 7',
    'cpu 1 2 3',
    'cpu 1 2 -3 4 5 6 7',
    'cpu 18446744073709551616 0 0 0 0 0 0',
    'cpu 1x 2 3 4 5 6 7',
) {
    stat_data($bad);
    sleep 1.1;
    $r = http_get('/cpu');
    like($r, qr/200 OK/, 'invalid CPU counters fail open');
    unlike($r, qr/X-CPU:/i, 'invalid CPU data is not published as stale usage');
}
stat_data('cpu 4294967400 0 0 4294967400 0 0 0');
sleep 1.1;
unlike(http_get('/cpu'), qr/X-CPU:/i, 'valid data after an error establishes a baseline');
stat_data('cpu 4294967480 0 0 4294967420 0 0 0');
sleep 1.1;
like(http_get('/cpu'), qr/X-CPU: 80\.00\r/i, 'CPU counters support values beyond 32 bits');

$t->stop();
unlike($t->read_file('error.log'), qr/exited on signal|\[alert\]|Sanitizer/, 'no worker or sanitizer errors');

sub stat_data {
    open my $file, '>', "$fixtures/stat" or die $!;
    print $file $_[0], "\ncpu0 1 2 3 4 5 6 7\n";
    close $file or die $!;
}

sub variable {
    return http_get('/', PeerAddr => '127.0.0.1:' . port(8081));
}
