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
my $update = $q->param("update");

die "no list specified" unless $list;
die "non-existent list" unless -d("$topdir/$list");

$tpl->define(main => "subscribers.html",
			 row => "subscribers_row.html");

my $action = '';

my $subscribers;

if (defined $subscribe) {
	my $email = $q->param("email");
	my $subscriber = $q->param("subscriber");
	my $digester = $q->param("digester");
	my $nomailsub = $q->param("nomailsub");
	if ($email =~ /^[a-z0-9\.\-_\@]+$/i) {
		if ($subscriber) {
			system "$mlmmjsub -L $topdir/$list -a $email -U";
		}
		if ($digester) {
			system "$mlmmjsub -L $topdir/$list -a $email -Ud";
		}
		if ($nomailsub) {
			system "$mlmmjsub -L $topdir/$list -a $email -Un";
		}
		$action = "$email has been subscribed.";
	} else {
		$action = '"'.encode_entities($email).'" is not a valid email address.';
	}
} elsif (defined $update) {
	my $maxid = $q->param("maxid");
	$subscribers = get_subscribers();
	for (my $i = 0; $i < $maxid; ++$i) {
		my $email = $q->param("email$i");
		if (defined $email) {
			if ($email =~ /^[a-z0-9\.\-_\@]+$/i) {
				my $updated = 0;

				my @actions = ();

				push @actions, {oldstatus => exists $subscribers->{$email}->{subscriber},
								newstatus => defined $q->param("subscriber$i"),
								action => ''};
				push @actions, {oldstatus => exists $subscribers->{$email}->{digester},
								newstatus => defined $q->param("digester$i"),
								action => '-d'};
				push @actions, {oldstatus => exists $subscribers->{$email}->{nomailsub},
								newstatus => defined $q->param("nomailsub$i"),
								action => '-n'};

				for my $action (@actions) {
					if ($action->{oldstatus} && !$action->{newstatus}) {
						system "$mlmmjunsub -L $topdir/$list -a $email $action->{action}";
						$updated = 1;
					} elsif (!$action->{oldstatus} && $action->{newstatus}) {
						system "$mlmmjsub -L $topdir/$list -a $email $action->{action}";
						$updated = 1;
					}
				}

				if ($updated) {
					$action .= "Subscription for $email has been updated.<br>\n";
				}
			} else {
				$action .= '"'.encode_entities($email).'" is not a valid email address.'."<br>\n";
			}
		}
	}
}

$tpl->assign(ACTION => $action);

$subscribers = get_subscribers();

my $i = 0;
for my $address (sort {lc $a cmp lc $b} keys %$subscribers) {
	$tpl->assign(EMAIL => $address,
				 ID => $i++,
				 SCHECKED => $subscribers->{$address}->{subscriber} ? 'checked' : '',
				 DCHECKED => $subscribers->{$address}->{digester} ? 'checked' : '',
				 NCHECKED => $subscribers->{$address}->{nomailsub} ? 'checked' : '');
	$tpl->parse(ROWS => '.row');
}
if (keys %$subscribers == 0) {
	$tpl->assign(ROWS => '');
}

$tpl->assign(LIST => encode_entities($list),
			 MAXID => scalar(keys %$subscribers));

print "Content-type: text/html\n\n";

$tpl->parse(CONTENT => "main");
$tpl->print;

sub get_subscribers {
	my %subscribers = ();

	my @subscribers = `/usr/local/bin/mlmmj-list -L $topdir/$list`;
	my @digesters = `/usr/local/bin/mlmmj-list -L $topdir/$list -d`;
	my @nomailsubs = `/usr/local/bin/mlmmj-list -L $topdir/$list -n`;

	chomp @subscribers;
	chomp @digesters;
	chomp @nomailsubs;

	for my $address (@subscribers) {
		$subscribers{$address}->{subscriber} = 1;
	}

	for my $address (@digesters) {
		$subscribers{$address}->{digester} = 1;
	}

	for my $address (@nomailsubs) {
		$subscribers{$address}->{nomailsub} = 1;
	}

	return \%subscribers;
}
