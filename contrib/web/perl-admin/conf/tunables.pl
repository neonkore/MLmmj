mlmmj_boolean("closedlist",
			  "Closed list",
			  "If this option is set, subscribtion and unsubscription via mail is disabled.");

mlmmj_boolean("moderated",
			  "Moderated",
			  "If this option is set, the emailaddresses in the file listdir/control/moderators will act as moderators for the list.");

mlmmj_list("moderators",
		   "Moderators",
		   "If the list is moderated, this is the list of moderators.");

mlmmj_boolean("tocc",
			  "To: Cc:",
			  "If this option is set, the list address does not have to be in the To: or Cc: header of the email to the list.");

mlmmj_boolean("addtohdr",
			  "Add To: header",
			  "If this option is set, a To: header including the recipients emailaddress will be added to outgoing mail. ".
			  "Recommended usage is to remove existing To: headers with delheaders (see below) first.");

mlmmj_boolean("subonlypost",
			  "Subscribers only post",
			  "If this option is set, only people who are subscribed to the list, are allowed to post to it. ".
			  "The check is made against the \"From:\" header.");

mlmmj_string("prefix",
			 "Prefix",
			 "The prefix for the Subject: line of mails to the list. This will alter the Subject: line, ".
			 "and add a prefix if it's not present elsewhere.");

mlmmj_list("owner",
		   "Owner",
		   "The emailaddresses in this list will get mails to ".encode_entities($list)."+owner");

mlmmj_list("delheaders",
		   "Delete headers",
		   "In this file is specified *ONE* headertoken to match pr. line. ".
		   "If the file consists of: Received: Message-ID: Then all occurences of these headers in incoming list mail will be deleted. ".
		   "\"From \" and \"Return-Path:\" are deleted no matter what.");

mlmmj_list("access",
		   "Access",
		   "If this option is set, all headers of a post to the list is matched against the rules. The first rule to match wins. ".
		   "See README.access for syntax and examples. NOTE: If this field is empty access control is *disabled*, ".
		   "unlike having an empty control/access file.");

mlmmj_string("memorymailsize",
			 "Memory mail size",
			 "Here is specified in bytes how big a mail can be and still be prepared for sending in memory. ".
			 "It's greatly reducing the amount of write system calls to prepare it in memory before sending it, ".
			 "but can also lead to denial of service attacks. Default is 16k (16384 bytes).");

mlmmj_string("relayhost",
			 "Relay host",
			 "The host specified (IP address og domainname, both works) in this file will be used for relaying the mail sent to the list. ".
			 "Defaults to 127.0.0.1.");

mlmmj_boolean("notifysub",
			  "Notify subscribers",
			  "If this option is set, the owner(s) will get a mail with the address of someone sub/unsubscribing to a mailinglist.");
