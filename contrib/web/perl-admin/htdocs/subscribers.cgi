#!/usr/bin/perl -w

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
use CGI;
use CGI::FastTemplate;
use Digest::MD5;

use vars qw($topdir $templatedir $list);

if (exists $ENV{CONFIG_PATH}) {
	require $ENV{CONFIG_PATH};
} else {
	require "../conf/config.pl";
}

my $mlmmjsub = "/usr/local/bin/mlmmj-sub";
my $mlmmjunsub = "/usr/local/bin/mlmmj-unsub";

my $tpl = new CGI::FastTemplate($templatedir);

my $q = new CGI;
$list = $q->param("list");
my $subscribe = $q->param("subscribe");
my $unsubscribe = $q->param("unsubscribe");

die "no list specified" unless $list;
die "non-existent list" unless -d("$topdir/$list");

$tpl->define(main => "subscribers.html",
			 row => "subscribers_row.html");

my $action = '';

my $subscribers;

if (defined $subscribe) {
	my $email = $q->param("email");
	if ($email =~ /^[a-z0-9\.\-_\@]+$/i) {
		system "$mlmmjsub -L $topdir/$list -a $email -U";
		if (is_subscribed($email)) {
			$action = "$email has been subscribed.";
		} else {
			$action = "$email was not subscribed.";
		}
	} else {
		$action = '"'.encode_entities($email).'" is not a valid email address.';
	}
} elsif (defined $unsubscribe) {
	my $maxid = $q->param("maxid");
	for (my $i = 0; $i < $maxid; ++$i) {
		my $email = $q->param("email$i");
		if (defined $email) {
			if ($email =~ /^[a-z0-9\.\-_\@]+$/i) {
				system "$mlmmjunsub -L $topdir/$list -a $email";
				if (!is_subscribed($email)) {
					$action .= "$email has been unsubscribed.<br>\n";
				} else {
					$action .= "$email was not unsubscribed.<br>\n";
				}
			} else {
				$action .= '"'.encode_entities($email).'" is not a valid email address.'."<br>\n";
			}
		}
	}
}

$tpl->assign(ACTION => $action);

$subscribers = get_subscribers();

for (my $i = 0; $i < @$subscribers; ++$i) {
	$tpl->assign(EMAIL => $subscribers->[$i],
				 ID => $i);
	$tpl->parse(ROWS => '.row');
}
if (@$subscribers == 0) {
	$tpl->assign(ROWS => '');
}

$tpl->assign(LIST => encode_entities($list),
			 MAXID => scalar(@$subscribers));

print "Content-type: text/html\n\n";

$tpl->parse(CONTENT => "main");
$tpl->print;

sub get_subscribers {
	my @subscribers = ();

	opendir (DIR, "$topdir/$list/subscribers.d") or die "Couldn't read dir $topdir/$list/subscribers.d: $!";
	my @files = grep(/^.$/, readdir(DIR));
	closedir DIR;
	for my $file (@files) {
		my $filename = "$topdir/$list/subscribers.d/$file";
		if (-f $filename) {
			open (FILE, $filename) or die "Couldn't open $filename for reading: $!";
			while (<FILE>) {
				chomp;
				push @subscribers, $_;
			}
			close FILE;
		}
	}

	@subscribers = sort @subscribers;

	return \@subscribers;
}

sub is_subscribed {
	my ($email) = @_;

	opendir (DIR, "$topdir/$list/subscribers.d") or die "Couldn't read dir $topdir/$list/subscribers.d: $!";
	my @files = grep(/^.$/, readdir(DIR));
	closedir DIR;

	for my $file (@files) {
		my $filename = "$topdir/$list/subscribers.d/$file";
		if (-f $filename) {
			open (FILE, $filename) or die "Couldn't open $filename for reading: $!";
			while (<FILE>) {
				chomp;
				if ($email eq $_) {
					return 1;
				}
			}
			close FILE;
		}
	}

	return 0;
}
