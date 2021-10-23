#!/usr/local/bin/jsish -u -echo true %s

function foo() { return true; }
function bar() { return false; }
function baz() { return true; }

puts('foo', (foo()?'passed':'failed'));
puts('bar', (bar()?'passed':'failed'));
puts('baz', (baz()?'passed':'failed'));

/*
=!EXPECTSTART!=
foo passed
bar passed
baz passed
=!EXPECTEND!=
*/
