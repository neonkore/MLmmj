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

use CGI qw(:standard);

require "config.pl";


# You might want to customize this function if you are not running the web
# server on the same host as the mail server running the lists, or if your
# lists are not in $topdir/list-name.
sub mlmmj_check_list {
	my $list = shift;

	if ($list !~ /^([a-z0-9-.]+)\@/) {
		return false;
	}

	if (!-f "$topdir/$1/control/listaddress") {
		return false;
	}

	open(FILE, "$topdir/$1/control/listaddress") or die('unable to open control/listaddress');
	$listaddr = readline(FILE);
	chomp($listaddr);
	if ($list ne $listaddr) {
		return false;
	}

	return true;
}


sub mlmmj_mail {
	my $from = shift;
	my $to = shift;
	my $subject = shift;
	my $body = shift;
	my $date = `/bin/date -R`;

	$mail = "Received: from " . $query->remote_addr()
		. " by " .  $query->server_name() . " witn HTTP;\n"
		. "\t$date"
		. "X-Originating-IP: " . $query->remote_addr() . "\n"
		. "X-Mailer: mlmmj-webinterface powered by Perl\n"
		. "Date: $date"
		. "From: $from\n"
		. "To: $to\n"
		. "Cc: $from\n"
		. "Subject: $subject\n"
		. "\n"
		. "$body\n";
	open(P, "|$sendmail -i -t") or die('unable to send mail');
	print(P $mail);
	close(P);
}


sub mlmmj_gen_to {
	my $list = shift;
	my $job = shift;

	if (($job ne 'subscribe') && ($job ne 'unsubscribe')) {
		return false;
	}
	($user, $domain) = split(/@/, $list);

	return sprintf("%s%s%s@%s", $user, $delimiter, $job, $domain);
}


$query = new CGI;

$list = $query->param('mailinglist');
$job = $query->param('job');
$redirect_failure = $query->param('redirect_failure');
$redirect_success = $query->param('redirect_success');
$email = $query->param('email');

print header;
print $list;

if (mlmmj_check_list($list) ne false) {
	$to = mlmmj_gen_to($list, $job);
	if ($to ne false) {
		mlmmj_mail($email, $to, "$job to $list", $job);

		print $query->redirect($redirect_success);
		exit(0);
	}
}

print $query->redirect($redirect_failure);
