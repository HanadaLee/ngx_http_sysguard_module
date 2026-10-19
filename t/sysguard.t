#!/usr/bin/perl

# Copyright (C) Hanada

use warnings;
use strict;
use Test::More;
use Test::Nginx;

plan 'no_plan';

my $t = Test::Nginx->new();
$t->write_file_expand('nginx.conf', <<'EOF');
%%TEST_GLOBALS%%
daemon off;
events {}
http {
    %%TEST_GLOBALS_HTTP%%
    default_type text/plain;
    server {
        listen 127.0.0.1:8080;
        root %%TESTDIR%%;
        location /variables {
            return 200 "$sysguard_free|$sysguard_meminfo_totalram|$sysguard_load";
        }
        location /off {
            sysguard off;
            sysguard_mem free=1048576M;
        }
        location /and {
            sysguard on;
            sysguard_mode and;
            sysguard_load load=1000000;
            sysguard_mem free=1048576M;
        }
        location /or {
            sysguard on;
            sysguard_mode or;
            sysguard_load load=1000000;
            sysguard_mem free=1048576M;
        }
        location /ssi {
            ssi on;
            ssi_types text/plain;
        }
        location /sub {
            sysguard on;
            sysguard_mem free=1048576M;
        }
    }
    server {
        listen 127.0.0.1:8081;
        root %%TESTDIR%%;
        sysguard on;
        sysguard_mem free=1048576M action=/limited;
        location / {}
        location /named {
            sysguard_mem free=1048576M action=@limited;
        }
        location @limited {
            try_files /limited =404;
        }
        location /error {
            sysguard_mem free=1048576M;
            error_page 503 = /limited;
        }
    }
}
EOF

$t->write_file($_, "ok\n") for qw/off and sub/;
$t->write_file('limited', "limited\n");
$t->write_file('ssi', '<!--# include virtual="/sub" -->');
$t->run();

my $r = http_get('/variables');
like($r, qr/200 OK/, 'variables work without enabling protection');
like($r, qr/\r\n\r\n[1-9]\d*\|[1-9]\d*\|\d+$/, 'variables collect data in rewrite');
like(http_get('/off'), qr/200 OK/, 'disabled protection');
like(http_get('/and'), qr/200 OK/, 'and requires all thresholds');
like(http_get('/or'), qr/503 Service/, 'or requires one threshold');
like(http_get('/ssi'), qr/ok/, 'subrequest is not guarded again');

for my $path (qw/probe named error/) {
    my $response = http_get("/$path", PeerAddr => '127.0.0.1:' . port(8081));
    like($response, qr/200 OK/, "$path survives internal redirect");
    like($response, qr/limited\n$/, "$path serves action target");
}

$t->stop();
unlike($t->read_file('error.log'), qr/internal redirection cycle|exited on signal|\[alert\]|Sanitizer/,
    'no redirect cycle or worker crash');
