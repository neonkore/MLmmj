<?php

mlmmj_boolean("closedlist",
	      "Closed list",
	      "Is the list is open or closed. If it's closed subscribtion ".
	      "and unsubscription via mail is disabled. Also note that ".
	      "confirmation is disabled too, so the -C option to mlmmj-sub ".
	      "and mlmmj-unsub is of no use with a closed list.");

mlmmj_boolean("moderated",
	      "Moderated",
	      "If this option is set, the emailaddresses in the file listdir".
	      "/control/moderators will act as moderators for the list.");

mlmmj_list("moderators",
	   "Moderators",
	   "If the list is moderated, this is the list of moderators.");

mlmmj_boolean("tocc",
	      "To: Cc:",
	      "If this option is set, the list address does not have to be ".
	      "in the To: or Cc: header of the email to the list.");

mlmmj_boolean("subonlypost",
	      "Subscribers only post",
	      "If this option is set, only people who are subscribed to the ".
	      "list, are allowed to post to it. The check is made against ".
	      "the \"From:\" header.");

mlmmj_string("prefix",
	     "Prefix",
	     "The prefix for the Subject: line of mails to the list. This ".
	     "will alter the Subject: line, and add a prefix if it's not ".
	     "present elsewhere.");

mlmmj_list("owner",
	   "Owner",
	   "The emailaddresses in this list will get mails to ".
	   htmlentities($list)."+owner@listdomain.tld");

mlmmj_list("customheaders",
	   "Custom headers",
	   "These headers are added to every mail coming through. This is ".
	   "the place you want to add Reply-To: header in case you want ".
	   "such.");

mlmmj_list("delheaders",
	   "Delete headers",
	   "In this file is specified *ONE* headertoken to match pr. line. ".
	   "If the file consists of: Received: Message-ID: Then all ".
	   "occurences of these headers in incoming list mail will be ".
	   "deleted. \"From\" and \"Return-Path:\" are deleted no matter ".
	   "what.");

mlmmj_list("access",
	   "Access",
	   "If this option is set, all headers of a post to the list is ".
	   "matched against the rules. The first rule to match wins. See ".
	   "README.access for syntax and examples.");

mlmmj_string("memorymailsize",
	     "Memory mail size",
	     "Here is specified in bytes how big a mail can be and still be ".
	     "prepared for sending in memory. It's greatly reducing the ".
	     "amount of write system calls to prepare it in memory before ".
	     "sending it, but can also lead to denial of service attacks. ".
	     "Default is 16k (16384 bytes).");

mlmmj_boolean("addtohdr",
	      "Add To: header",
	      "If this option is set, a To: header including the recipients ".
	      "emailaddress will be added to outgoing mail. Recommended ".
	      "usage is to remove existing To: headers with delheaders ".
	      "(see above) first.");

mlmmj_string("relayhost",
	     "Relay host",
	     "The host specified (IP address og domainname, both works) in ".
	     "this file will be used for relaying the mail sent to the list. ".
	     "Defaults to 127.0.0.1.");

mlmmj_string("smtpport",
	     "SMTP port",
	     "In this file a port other than port 25 for connecting to the ".
	     "relayhost can be specified.");

mlmmj_boolean("notifysub",
	      "Notify subscribers",
	      "If this option is set, the owner(s) will get a mail with the ".
	      "address of someone sub/unsubscribing to a mailinglist.");

mlmmj_string("digestinterval",
	     "Digest interval",
	     "This option specifies how many seconds will pass before the ".
	     "next digest is sent. Defaults to 604800 seconds, which is 7 ".
	     "days.");

mlmmj_string("digestmaxmails",
	     "Max. digest mails",
	     "This option specifies how many mails can accumulate before ".
	     "digest sending is triggered. Defaults to 50 mails, meaning ".
	     "that if 50 mails arrive to the list before digestinterval have ".
	     "passed, the digest is delivered.");

mlmmj_string("bouncelife",
	     "Bouncing lifetime",
	     "Here is specified for how long time in seconds an address can ".
	     "bounce before it's unsubscribed. Defaults to 432000 seconds, ".
	     "which is 5 days.");

mlmmj_boolean("noarchive",
	      "No archive",
	      "If this option is set, the mails won't be saved in the ".
	      "archive but simply deleted");

mlmmj_boolean("nosubconfirm",
	      "No confirmation on subscription",
	      "If this option is set, no mail confirmation is needed to ".
	      "subscribe to the list. This should in principle never ever be ".
	      "used, but there is times on local lists etc. where this is ".
	      "useful. HANDLE WITH CARE!");

mlmmj_boolean("noget",
	      "No get",
	      "If this option is set, listname+get-INDEX is turned off.");

mlmmj_boolean("notoccdenymails",
	      "No To: Cc: deny mails",
	      "This switch turns off whether mlmmj sends out notification ".
	      "about postings being denied due to the listaddress not being ".
	      "in To: or Cc: (see 'tocc').");

mlmmj_boolean("noaccessdenymails",
	      "No access deny mails",
	      "This switch turns off whether mlmmj sends out notification ".
	      "about postings being rejected due to an access rule (see ".
	      "'access').");

mlmmj_boolean("nosubonlydenymails",
	      "No subscribers only deny mails",
	      "This switch turns off whether mlmmj sends out notification ".
	      "about postings being rejected due to a subscribers only ".
	      "posting list (see 'subonlypost').");


?>
