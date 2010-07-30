mlmmj_list("listaddress",
			  "List address",
			  "This option contains all addresses which mlmmj sees as listaddresses (see ".
			  "tocc below). The first one is the one used as the primary one, when mlmmj ".
			  "sends out mail.");

mlmmj_boolean("closedlist",
			  "Closed list",
			  "If the list is open or closed. If it's closed subscription ".
			  "and unsubscription via mail is disabled.");

mlmmj_boolean("closedlistsub",
			  "Closed for subscription",
			  "Closed for subscription. Unsubscription is possible.");

mlmmj_boolean("nosubconfirm",
			  "No subscribe confirmation",
			  "If this option is set, the user is not required to confirm when subscribing or unsubscribing.");

mlmmj_boolean("moderated",
			  "Moderated",
			  "If this option is set, the emailaddresses in the file listdir/control/moderators will act as moderators for the list.");

mlmmj_list("moderators",
		   "Moderators",
		   "If the list is moderated, this is the list of moderators.");

mlmmj_list("submod",
		   "Subscription moderators",
		   "This is the list of moderators that will approve subscriptions.");

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

mlmmj_boolean("modnonsubposts",
			  "Moderate non-subscriber posts",
			  "If this option is set and subonlypost is enabled, all postings from ".
			  "people who are not subscribed to the list will be moderated.");

mlmmj_string("prefix",
			 "Prefix",
			 "The prefix for the Subject: line of mails to the list. This will alter the Subject: line, ".
			 "and add a prefix if it's not present elsewhere.");

mlmmj_list("owner",
		   "Owner",
		   "The emailaddresses in this list will get mails to ".encode_entities($list)."+owner");

mlmmj_list("customheaders",
		   "Custom headers",
		   "These headers are added to every mail coming through. This is ".
		   "the place you want to add Reply-To: header in case you want ".
		   "such. ".
		   "If a header should not occur twice in the mail it should be listed in the 'Delete headers' box too.");

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
			 "The host specified (IP address or domainname, both works) in this file will be used for relaying the mail sent to the list. ".
			 "Defaults to 127.0.0.1.");

mlmmj_string("smtpport",
			 "SMTP port",
			 "In this file a port other than port 25 for connecting to the relayhost can be specified.");

mlmmj_string("delimiter",
			  "Delimiter",
			  "This specifies what to use as recipient delimiter for the list.".
			  "Default is '+'.");

mlmmj_boolean("notifysub",
			  "Notify subscribers",
			  "If this option is set, the owner(s) will get a mail with the address of someone sub/unsubscribing to a mailinglist.");

mlmmj_boolean("notifymod",
			  "Notify moderation",
			  "If this option is set, the poster (based on the envelope from) will ".
			  "get a mail when their post is being moderation.");

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
			 "Here is specified for how long time in seconds an address can bounce before it's unsubscribed. Defaults ".
			 "to 432000 seconds, which is 5 days.");

mlmmj_boolean("noarchive",
			  "No archive",
			  "If this option is set, the mails won't be saved in the ".
			  "archive but simply deleted");

mlmmj_boolean("noget",
			  "No get",
			  "If this option is set, listname+get-INDEX is turned off.");

mlmmj_boolean("subonlyget",
			  "Subscribers only get",
			  "If this option is set, retrieving old posts with +get-N is only possible for subscribers.");

mlmmj_string("verp",
			  "VERP",
			  "Enable VERP support. Anything added in this variable will be appended the ".
			  "MAIL FROM: line. If 'postfix' is put in the file, it'll make postfix use ".
			  "VERP by adding XVERP=-= to the MAIL FROM: line.");

mlmmj_string("maxverprecips",
			  "Maximum VERP recipients",
			  "How many recipients pr. mail delivered to the smtp server. Defaults to 100.");

mlmmj_boolean("notoccdenymails",
			  "No To: Cc: deny mails",
			  "This switch turns off whether mlmmj sends out notification about postings ".
			  "being denied due to the listaddress not being in To: or Cc: (see 'tocc').");

mlmmj_boolean("noaccessdenymails",
			  "No access deny mails",
			  "This switch turns off whether mlmmj sends out notification about postings ".
			  "being rejected due to an access rule (see 'access').");

mlmmj_boolean("nosubonlydenymails",
			  "No subscribers only deny mails",
			  "This switch turns off whether mlmmj sends out notification about postings ".
			  "being rejected due to a subscribers only posting list (see 'subonlypost').");

mlmmj_boolean("nosubmodmails",
			  "No subscription moderated mails",
			  "This switch turns off whether mlmmj sends out notification about ".
			  "subscription being moderated to the person requesting subscription".
			  "(see 'submod').");

mlmmj_boolean("nodigesttext",
			  "No digest text summary",
			  "This switch turns off whether digest mails will have a text part with a thread ".
			  "summary.");

mlmmj_boolean("nodigestsub",
			  "No digest subscribers",
			  "If this option is set, subscription to the digest version of the mailinglist ".
			  "will be denied. (Useful if you don't want to allow digests and notify users ".
			  "about it).");

mlmmj_boolean("nonomailsub",
			  "No nomail subscribers",
			  "If this option is set, subscription to the nomail version of the mailinglist ".
			  "will be denied. (Useful if you don't want to allow nomail and notify users ".
			  "about it).");

mlmmj_string("maxmailsize",
			 "Max. mail size",
			 "With this option the maximal allowed size of incoming mails can be specified.");

mlmmj_boolean("nomaxmailsizedenymails",
			  "No max. mail size deny mails",
			  "If this is set, no reject notifications caused by violation of maxmailsize ".
			  "will be sent.");

mlmmj_boolean("nolistsubsemail",
			  "No list subscribers email",
			  "If this is set, the LISTNAME+list\@ functionality for requesting an ".
			  "email with the subscribers for owner is disabled.");

mlmmj_string("staticbounceaddr",
			 "Static bounce address",
			 "If this is set to something\@example.org, the bounce address (Return-Path:) ".
			 "will be fixed to something+listname-bounces-and-so-on\@example.org ".
			 "in case you need to disable automatic bounce handling.");

mlmmj_boolean("ifmodsendonlymodmoderate",
			  "If moderator send only moderator moderate",
			  "If this is set, then mlmmj in case of moderation checks the ".
			  "envelope from, to see if the sender is a moderator, and in that case ".
			  "only send the moderation mails to that address. In practice this means that ".
			  "a moderator sending mail to the list won't bother all the other moderators ".
			  "with his mail.");

mlmmj_list("footer",
			  "Footer",
			  "The content of this option is appended to mail sent to the list.");

mlmmj_boolean("notmetoo",
			  "Not me too",
			  "If this is set, mlmmj attempts to exclude the sender of a post ".
			  "from the distribution list for that post so people don't receive copies ".
			  "of their own posts.");

