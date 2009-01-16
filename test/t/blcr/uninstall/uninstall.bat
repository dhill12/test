#!/usr/bin/perl

use strict;
use warnings;

use CRI::Test;

plan('no_plan');
setDesc("Uninstall 'Berkley Lab Checkpoint/Restart (BLCR) for Linux'");

my $testbase = $props->get_property('test.base') . "blcr/uninstall";

execute_tests(
        "$testbase/010_make_uninstall_blcr.t"
        );
