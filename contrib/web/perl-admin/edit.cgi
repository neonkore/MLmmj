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

sub mlmmj_boolean {
    my $name = shift;
    my $text = shift;
    my $checked = -f "$topdir/$list/control/$name";

    print("<tr>");
    print("<td>", $query->checkbox(-name=>$name,-checked=>$checked), "</td>\n");
    print("<td>$text</td>\n");
    print("</tr>");
}

sub mlmmj_string {
    my $name = shift;
    my $text = shift;
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
    print("<tr>");
    print("<td>$name ", $query->textfield(-name=>$name,-default=>$value),"</td>\n");
    print("<td>$text</td>\n");
    print("</tr>");
}

sub mlmmj_list {
    my $name = shift;
    my $text = shift;
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

    print("<tr>");
    print("<td>$name ", $query->textarea(-name=>$name,-default=>$value,-columns=>40), "</td>\n");
    print("<td>$text</td>\n");
    print("</tr>");
}

$query = new CGI;
$list = $query->url(-relative=>1,-path=>1);
$list =~ s/^[^\/]*\/([^\/]+)\/$/$1/ or die('no list parameter');
#printf("list = '%s'<br/>\n", $list);

print header;
print start_html('mlmmj config');

print($query->startform("POST","/save.cgi/$list/"));
print("<table>");
mlmmj_boolean("closedlist", "If this option is set, subscribtion and unsubscription via mail is disabled.");
mlmmj_boolean("moderated", "If this option is set, the emailaddresses in the file listdir/control/moderators will act as moderators for the list.");
mlmmj_boolean("tocc", "If this option is set, the list address does not have to be in the To: or Cc: header of the email to the list.");
mlmmj_boolean("addtohdr", "If this option is set, a To: header including the recipients emailaddress will be added to outgoing mail. Recommended usage is to remove existing To: headers with delheaders (see below) first.");
mlmmj_boolean("subonlypost", "If this option is set, only people who are subscribed to the list, are allowed to post to it. The check is made against the \"From:\" header.");
mlmmj_string("prefix", "The prefix for the Subject: line of mails to the list. This will alter the Subject: line, and add a prefix if it's not present elsewhere.");
mlmmj_list("owner", "The emailaddresses in this list will get mails to $list+owner");
mlmmj_list("delheaders", "In this file is specified *ONE* headertoken to match pr. line. If the file consists of: Received: Message-ID: Then all occurences of these headers in incoming list mail will be deleted. \"From \" and \"Return-Path:\" are deleted no matter what.");
mlmmj_list("access", "If this option is set, all headers of a post to the list is matched against the rules. The first rule to match wins. See README.access for syntax and examples. NOTE: If this field is empty access control is *disabled*, unlike having an empty control/access file.");
mlmmj_string("memorymailsize", "Here is specified in bytes how big a mail can be and still be prepared for sending in memory. It's greatly reducing the amount of write system calls to prepare it in memory before sending it, but can also lead to denial of service attacks. Default is 16k (16384 bytes).");
print("</table>");
print($query->submit());
print($query->endform());

print end_html;
