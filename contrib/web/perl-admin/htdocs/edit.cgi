#!/usr/bin/perl -w

# Copyright (C) 2004 Morten K. Poulsen <morten at afdelingp.dk>
# Copyright (C) 2004 Christian Laursen <christian@pil.dk>
#
# $Id$
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

use strict;
use CGI;
use CGI::FastTemplate;
use HTML::Entities;

use vars qw($topdir $templatedir $list);

if (exists $ENV{CONFIG_PATH}) {
	require $ENV{CONFIG_PATH};
} else {
	require "../conf/config.pl";
}

my $tpl = new CGI::FastTemplate($templatedir);

my $q = new CGI;
$list = $q->param("list");

die "non-existent list" unless -d("$topdir/$list");

$tpl->define(main => "edit.html",
			 boolean => "edit_boolean.html",
			 string => "edit_string.html",
			 list => "edit_list.html");

$tpl->assign(LIST => encode_entities($list));

my $tunables_file = "../conf/tunables.pl";
if (exists $ENV{TUNABLES_PATH}) {
	$tunables_file = $ENV{TUNABLES_PATH};
}

do $tunables_file;

print "Content-type: text/html\n\n";
$tpl->parse(CONTENT => "main");
$tpl->print;

sub mlmmj_boolean {
	my ($name, $nicename, $text) = @_;
    my $checked = -f "$topdir/$list/control/$name";

	$tpl->assign(NAME => encode_entities($name));
	$tpl->assign(NICENAME => encode_entities($nicename));
	$tpl->assign(TEXT => encode_entities($text));
	$tpl->assign(CHECKED => $checked ? ' checked' : '');

	$tpl->parse(ROWS => ".boolean");
}

sub mlmmj_string {
	my ($name, $nicename, $text) = @_;
    my $file = "$topdir/$list/control/$name";
    my $value;

    if (! -f $file) {
        $value = "";
    } else {
        open(F, $file) or die("can't open $file");
        $value = <F>;
        close(F);
        chomp($value);
    }

	$tpl->assign(NAME => encode_entities($name));
	$tpl->assign(NICENAME => encode_entities($nicename));
	$tpl->assign(TEXT => encode_entities($text));
	$tpl->assign(VALUE => encode_entities($value));

	$tpl->parse(ROWS => ".string");
}

sub mlmmj_list {
	my ($name, $nicename, $text) = @_;
    my $file = "$topdir/$list/control/$name";
    my $value;

    if (! -f $file) {
        $value = "";
    } else {
        open(F, $file) or die("can't open $file");
        while (<F>) {
            $value .= $_;
        }
        close(F);
        chomp($value);
    }

	$tpl->assign(NAME => encode_entities($name));
	$tpl->assign(NICENAME => encode_entities($nicename));
	$tpl->assign(TEXT => encode_entities($text));
	$tpl->assign(VALUE => encode_entities($value));

	$tpl->parse(ROWS => ".list");
}

