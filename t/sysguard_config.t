#!/usr/bin/perl

# Copyright (C) Hanada

use warnings;
use strict;
use Test::More;
use Test::Nginx;

plan 'no_plan';

my @invalid = (
    'sysguard_interval 9223372036854775807;',
    'sysguard_cpu usage=100.01;',
    'sysguard_cpu usage=-1;',
    'sysguard_cpu usage=50%;',
    'sysguard_cpu period=1;',
    'sysguard_cpu usage=50 period=0;',
    'sysguard_cpu usage=50 period=9223372036854775807;',
    'sysguard_cpu usage=50 actoin=/limited;',
    'sysguard_cpu usage=50 usage=60;',
    'sysguard_cpu usage=50 period=1 period=2;',
    'sysguard_cpu usage=50 action=/a action=/b;',
    'sysguard_cpu usage=50; sysguard_cpu usage=60;',
    'sysguard_rt rt=0.1 period=0;',
    'sysguard_rt rt=0.1 method=AMM:0;',
    'sysguard_rt rt=0.1 method=WMA:0;',
    'sysguard_rt rt=0.1 method=AMM:;',
    'sysguard_rt rt=0.1 method=AMMfoo;',
    'sysguard_rt rt=0.1 method=WMA:2:3;',
    'sysguard_rt rt=0.1 actoin=/limit;',
    'sysguard_rt rt=0.1 action=;',
    'sysguard_rt rt=0.1 action=@;',
    'sysguard_rt rt=0.1 method=AMM:9223372036854775807;',
    'sysguard_rt rt=0.1 action=limit;',
    'sysguard_rt period=2;',
    'sysguard_rt rt=1 rt=2;',
    'sysguard_rt rt=1 period=2 period=3;',
    'sysguard_rt rt=1 method=AMM:2 method=WMA:3;',
    'sysguard_rt rt=1 action=/a action=/b;',
    'sysguard_rt rt=1; sysguard_rt rt=2;',
    'sysguard_mem swapratio=100.01%;',
    'sysguard_mem swapratio=-1%;',
    'sysguard_mem swapratio=20;',
    'sysguard_mem swapratio=20% actoin=/limit;',
    'sysguard_load load=ncpu*9999999999999999999;',
    'sysguard_mode on;',
);
my @valid = (
    'sysguard_cpu usage=0;',
    'sysguard_cpu usage=100;',
    'sysguard_cpu usage=50.25 period=1 action=@limited;',
    'sysguard_rt rt=0.1;',
    'sysguard_rt rt=0.1 method=AMM:1;',
    'sysguard_rt rt=0.1 method=WMA:1;',
    'sysguard_rt rt=0 period=1 method=AMM;',
    'sysguard_rt action=@limit method=WMA:10 period=2 rt=0.1;',
    'sysguard_mem swapratio=0%;',
    'sysguard_mem swapratio=100%;',
    'sysguard_mem swapratio=20.25%;',
    'sysguard_load load=ncpu*1.5;',
    'sysguard_mode and;',
    'sysguard_mode or;',
);

for my $case ((map { [$_, 0] } @invalid), (map { [$_, 1] } @valid)) {
    my ($directive, $valid) = @$case;
    my $t = Test::Nginx->new();
    my $dir = $t->testdir();
    $t->write_file_expand('nginx.conf', <<EOF);
%%TEST_GLOBALS%%
events {}
http {
    %%TEST_GLOBALS_HTTP%%
    server { listen 127.0.0.1:8080; $directive }
}
EOF
    my $pid = fork();
    die "fork: $!" unless defined $pid;
    if (!$pid) {
        open STDOUT, '>', "$dir/config-test.log" or die $!;
        open STDERR, '>&', \*STDOUT or die $!;
        exec $Test::Nginx::NGINX, '-t', '-p', "$dir/", '-c', 'nginx.conf', '-e', 'error.log';
        die "exec: $!";
    }
    waitpid($pid, 0);
    is($? == 0 ? 1 : 0, $valid, $directive);
    if (!$valid) {
        like($t->read_file('config-test.log'), qr/invalid parameter|duplicate|requires|invalid value|too many sysguard RT samples|sysguard_interval is too large/,
            'rejected for the directive error');
    }
}
