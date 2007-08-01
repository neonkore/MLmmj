#!/usr/bin/perl -w

# Copyright (C) 2007 Franky Van Liedekerke <liedekef@telenet.be>
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

# We might want some kind of validation of the values we are about to save,
# but that would require save.cgi to know about all kind of options that mlmmj
# accepts. I am not sure we want that.  -- mortenp 20040709

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

die "no list specified" unless $list;
die "non-existent list" unless -d("$topdir/$list");

$tpl->define(main => "save_text.html");
$tpl->assign(LIST => encode_entities($list));

my $textdir = "$topdir/$list/text";
my @files;
opendir(DIR, $textdir ) || die "can't opendir $textdir: $!";
@files = grep { !/^\./ && -f "$textdir/$_" } readdir(DIR);
closedir DIR;

for my $textfile (sort @files) {
   mlmmj_text($textfile);
}

print "Content-type: text/html\n\n";
$tpl->parse(CONTENT => "main");
$tpl->print;

sub mlmmj_text {
	my ($name, $nicename, $text) = @_;

	my $file = "$textdir/$name";

	my $value = $q->param($name);

	open (FILE, ">$file") or die "Couldn't open $file for writing: $!";
	print FILE $value;
	close FILE;
}
