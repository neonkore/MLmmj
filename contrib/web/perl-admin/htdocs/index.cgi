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
use URI::Escape;
use HTML::Entities;
use CGI::FastTemplate;

use vars qw($topdir $templatedir);

if (exists $ENV{CONFIG_PATH}) {
	require $ENV{CONFIG_PATH};
} else {
	require "../conf/config.pl";
}

my $tpl = new CGI::FastTemplate($templatedir);

$tpl->define(main => "index.html",
			 row => "index_row.html");

my $lists = "";
opendir(DIR, $topdir) or die "Couldn't open $topdir for reading: $!";
while (my $list = readdir(DIR)) {
    next if $list =~ /^\./;
	$tpl->assign(LIST => encode_entities($list));
	$tpl->assign(ULIST => uri_escape($list));
	$tpl->parse(LISTS => '.row');
}
closedir(DIR);

print "Content-type: text/html\n\n";

$tpl->parse(CONTENT => "main");
$tpl->print;
