#!/usr/bin/perl

use strict;
use warnings;

use CRI::Test;

plan('no_plan');
setDesc("ALL Torque Regression Tests");

my $testbase = $props->get_property('test.base') . "/torque";

execute_tests(
    "$testbase/torque/reinstall.bat",
) or die("Torque reinstall test failed!");

execute_tests(
    "$testbase/commands/all.bat",
    "$testbase/ha/all.bat",
);