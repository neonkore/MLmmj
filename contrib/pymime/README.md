pymime - A MIME formatter in python
===================================

This program is designed to take a MIME-formatted email and do the following actions:
- convert HTML-parts of singlepart-mails to plaintext
- strip HTML-parts of multipart-mails
- strip attachments
- optionally append a footer to the mail
- optionally add an X-Archived-At header for mail-archive.com

Requirements
------------

Tested with python 2.6 and 2.7.
Currently not compatible with python 3.X, as this would require dropping 2.6 support.

Usage
-----

	Usage: pymime.py [options]
	Options:
	  -h, --help
	            show this help message and exit
	  -i INPUT, --input=INPUT
	            Where to read the mail from. Defaults to STDIN
	  -o OUTPUT, --output=OUTPUT
	            Where to write the transformed mail. Defaults to STDOUT
	  -f FOOTER, --footer=FOOTER
	            UTF-8 encoded footer to append to every mail.
	  -k, --keep-going      
	            Ignore failures (ATM only missing footer file) as much
	            as possible before failing.
	  -a, --archive-header      
	            Add Archived-At header for mail-archive.com
	

License
-------

GPLv3

Source
------

https://github.com/tdf/pymime

Bugs
----

https://github.com/tdf/pymime/issues

Contact
-------

alex@documentfoundation.org