#!/usr/bin/perl

# Copyright (C) Hanada

use warnings;
use strict;
use IO::Socket::INET;
use Time::HiRes qw/sleep/;
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
    server {
        listen 127.0.0.1:8080;
        root %%TESTDIR%%;
        sysguard on;
        add_header X-RT $sysguard_rt always;
        location /default/ {
            sysguard_rt rt=0.1;
            proxy_pass http://127.0.0.1:8081/;
        }
        location /one/ {
            sysguard_rt rt=0.1 method=WMA:1;
            proxy_pass http://127.0.0.1:8081/;
        }
        location /amm/ {
            sysguard_rt rt=100 period=10 method=AMM:2;
            proxy_pass http://127.0.0.1:8081/;
        }
        location /wma/ {
            sysguard_rt rt=100 period=10 method=WMA:2;
            proxy_pass http://127.0.0.1:8081/;
        }
        location /origin/ {
            sysguard_rt rt=0.1 action=/limited;
            try_files /missing /backend/300;
        }
        location /backend/ {
            proxy_pass http://127.0.0.1:8081/;
        }
        location /limited {}
        location /variables/ {
            sysguard off;
            proxy_pass http://127.0.0.1:8081/;
        }
    }
}
EOF
$t->write_file('limited', "limited\n");
$t->run_daemon(\&backend);
$t->waitforsocket('127.0.0.1:' . port(8081));
$t->run();

for my $location (qw/default one/) {
    like(http_get("/$location/300"), qr/200 OK/, "$location first request");
    like(http_get("/$location/0"), qr/503 Service/, "$location one valid sample triggers protection");
    sleep 1.15;
    like(http_get("/$location/0"), qr/200 OK/, "$location recovers after expiry");
}

for my $method (qw/amm wma/) {
    like(http_get("/$method/150"), qr/X-RT: 0\r/i, "$method starts empty");
    my $r = http_get("/$method/450");
    my ($first) = $r =~ /X-RT: (\d+)/i;
    cmp_ok($first // 0, '>=', 130, "$method does not dilute a partial ring");
    $r = http_get("/$method/0");
    my ($avg) = $r =~ /X-RT: (\d+)/i;
    my $expected = $method eq 'amm' ? 300 : 350;
    cmp_ok($avg // 0, '>=', $expected - 25, "$method average lower bound");
    cmp_ok($avg // 9999, '<=', $expected + 100, "$method average upper bound");
    http_get("/$method/0");
    $r = http_get("/$method/0");
    my ($recent) = $r =~ /X-RT: (\d+)/i;
    cmp_ok($recent // 9999, '<', 100, "$method retains exactly the last two samples");
}

like(http_get('/origin/start'), qr/ok\n$/, 'accepted redirect reaches upstream');
like(http_get('/origin/start'), qr/limited\n$/, 'RT is attributed to original location');
like(http_get('/origin/start'), qr/limited\n$/, 'rejected responses do not replace slow samples');
sleep 1.15;
like(http_get('/origin/start'), qr/ok\n$/, 'action redirect does not leave RT protection stuck');

http_get('/variables/250');
my ($rt) = http_get('/variables/0') =~ /X-RT: (\d+)/i;
cmp_ok($rt // 0, '>=', 230, 'RT variable collects without protection');
$t->stop();
unlike($t->read_file('error.log'), qr/exited on signal|internal redirection cycle|\[alert\]|Sanitizer/,
    'no worker crash or redirect loop');

sub backend {
    my $server = IO::Socket::INET->new(
        LocalAddr => '127.0.0.1', LocalPort => port(8081),
        Listen => 16, ReuseAddr => 1) or die $!;
    while (my $client = $server->accept()) {
        $client->autoflush(1);
        my $line = <$client>;
        next unless defined $line;
        my ($delay) = $line =~ m{ /([0-9]+)};
        while (defined($line = <$client>) && $line !~ /^\r?\n$/) {}
        sleep(($delay // 0) / 1000);
        print $client "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nok\n";
        close $client;
    }
}
