#!/usr/bin/perl

# Copyright (C) 2004 Morten K. Poulsen <morten at afdelingp.dk>
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

use CGI qw(:standard);

require "config.pl";

$query = new CGI;
$list = $query->url(-relative=>1,-path=>1);
$list =~ s/^[^\/]*\/([^\/]+)\/$/$1/ or die('no list parameter');
#printf("list = '%s'<br/>\n", $list);

($query->request_method() eq 'POST') or die('wrong method');

@controls = $query->all_parameters();

foreach $param (keys %{$query->{'.fieldnames'}}) {
	#printf("B params=[%s]<br/>\n", $name);
	if (!defined $query->param($param)) {
		push(@controls, $param);
	}
}


foreach $control (@controls) {
	($control !~ /^[a-z]\+$/) or die("illegal control name '$control'");
        my $file = "$topdir/$list/control/$control";
	my $value = $query->param($control);
	#printf("A params=[%s -> '%s']<br/>\n", $control, $value);

	if (length($value) > 0) {
		open(FILE, ">$file") or die("unable to open '$file'");
		printf(FILE "%s\n", $query->param($control));
		close(FILE);
	} else {
		if (-f $file) {
			unlink($file) or die("unable to unlin '$file'");
		}
	}
}

print header;
print start_html('mlmmj config');
print "$list control values saved!";
print end_html;
