#!/usr/bin/env python
# -*- coding: utf-8 -*-

#    Copyright 2011 Alexander Werner
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
This script takes a MIME-formatted email and does various transformations with it,
e.g. converts HTML-mails to plain text mails and strips attachements.
"""
__version__="1.0.0"
import HTMLParser, email, email.utils, sys
from optparse import OptionParser
# for archive-header
import hashlib
import base64
#-------------------------------------------------------------------------------
# Configuration Options start here

IGNORETAGS = ( "script", "head", "title", "link" )
MAP_STARTTAGS = {"li": "\n* ",
                 "p": "\n"}
MAP_ENDTAGS = { "p": "\n",
               "div": "\n",
               "h1": "\n==============================================================\n\n",
               "h2": "\n--------------------------------------------------------------\n\n",
               "h3": "\n",
               "h4": "\n",
               "h5": "\n",
               "h6": "\n"}
MAXNUMNEWLINES = 2

# No Configuration beneath this line
# -------------------------------------------------------------------------------

# Parse command line options
parser = OptionParser()
parser.add_option( "-i", "--input", dest = "input", default = "-",
                   help = "Where to read the mail from. Defaults to STDIN" )
parser.add_option( "-o", "--output", dest = "output", default = "-",
                   help = "Where to write the transformed mail. Defaults to STDOUT" )
parser.add_option( "-f", "--footer", dest = "footer", default = None,
                   help = "UTF-8 encoded footer to append to every mail." )
parser.add_option( "-k", "--keep-going", dest = "keep_going", action = "store_true", default = False,
                   help = "Ignore failures (ATM only missing footer file) as much as possible before failing." )
parser.add_option( "-a", "--archive-header", dest = "archive_header", action = "store_true", default = False,
                   help = "Add Archived-At header for mail-archive.com" )
parser.add_option( "-V", "--version", dest = "print_version", action = "store_true", default = False, help = "Show version and exit" )
options, args = parser.parse_args()


class StripHTML( HTMLParser.HTMLParser ):
    """
    This class provides the necessary logic to convert HTML to plain text.
    """
    def __init__( self ):
        self.reset()
        self.plain = []
        self.last_starttag = None
    def handle_starttag( self, tag, attributes ):
        self.last_starttag = tag
        if tag in MAP_STARTTAGS:
            self.plain.append( MAP_STARTTAGS[tag] )
    def handle_endtag( self, tag ):
        if tag in MAP_ENDTAGS.keys():
            self.plain.append( MAP_ENDTAGS[tag] )
    def handle_data( self, data ):
        if self.last_starttag not in IGNORETAGS:
            self.plain.append( data.strip() )
    def remove_whitespace( self ):
        # Split at newlines instead of tags
        self.plain = "".join( self.plain ).split( "\n" )
        numspace = 0
        # Copy the whole text
        oldplain = self.plain[:]
        self.plain = []
        for line in oldplain:
            if line.isspace() or line is "":
                numspace = numspace + 1
                if numspace <= MAXNUMNEWLINES:
                    # number of blank newlines is lower than limit, append line
                    self.plain.append( "\n" )
            else:
                numspace = 0
                # line is no blank newline, append line
                self.plain.append( line )
    def get_plain_data( self ):
        self.remove_whitespace()
        return "\n".join( self.plain )

class EMail( object ):
    """
    This class represents an email or a single payload of a multipart email.
    """
    def __init__( self, keep_going = False ):
        self.to_include = False
        self.keep_going = keep_going
    def feed( self, fp = None, string = None, message = None ):
        """
        Feeds the EMail object with data. If fp is supplied, a new message will be created using
        email.message_from_file, if string is supplied, a new message will be created using
        email.message_from_string, if message is supplied, the supplied message will be used.
        """
        if fp is not None:
            self.message = email.message_from_file( fp )
        elif string is not None:
            self.message = email.message_from_string( string )
        elif message is not None:
            self.message = message
        else:
            raise AttributeError
    def parse( self ):
        """
        Parses the supplied message object. EMail.feed must have been called before.
        """
        if self.message.is_multipart():
            self.parse_multipart()
        else:
            self.parse_singlepart()
    def parse_multipart( self ):
        """
        Parses a multipart message object.
        """
        mails = []
        #---------------------------------------------------------------------------
        # Test if the message has a text/plain and a text/html part
        has_plaintext = False
        has_html = False
        for part in self.message.walk():
            content_type = part.get_content_type()
            if content_type == "text/plain":
                has_plaintext = True
            if content_type == "text/html":
                has_html = True
        #---------------------------------------------------------------------------
        # If the message has both text/plain and text/html parts, use the existing text/plain part
        # and dismiss all other parts
        if has_plaintext and has_html:
            for part in self.message.walk():
                content_type = part.get_content_type()
                if content_type == "text/plain":
                    mails.append( part )
        #---------------------------------------------------------------------------
        # Parse every part of the message for inclusion
        else:
            for part in self.message.walk():
                # Avoid infinite recursion, message.walk also yields the parent message.
                if part.is_multipart():
                    continue
                # Parse the submessage
                mail = EMail()
                mail.feed( message = part )
                mail.parse()
                # If the submessage contains useful data, append it to the final list of submessages
                if mail.to_include:
                    mails.append( mail.message )
        #---------------------------------------------------------------------------
        # Test if at least one submessage survived the parsing
        if mails:
            # Delete the body from the message
            self.message.set_payload( None )
            # Only one submessage to be included in the body, a restructuring of the parent
            # message is necessary.
            if len( mails ) == 1:
                mail = mails[0]
                # Copy the Headers from the submessage to the parent message
                for key in mail.keys():
                    del self.message[key]
                    self.message[ key] = mail[key]
                # If the parent message is still multipart, the submessage didn't have a Content-Type
                # Header. Replace the old parent Content-Type with a default.
                if "multipart" in self.message.get_content_maintype():
                    del self.message["Content-Type"]
                    self.message["Content-Type"] = "text/plain"
                # Copy the body from the submessage to teh parent message
                self.message.set_payload( mails[0].get_payload() )
            # Multiple submessages are to be included, simply attach them.
            else:
                for mail in mails:
                    self.message.attach( mail )
        # No Message is to be included
        else:
            raise Exception
    def addArchiveHeader( self ):
        """
        Add mail-archive.com direct-link to archive http://www.mail-archive.com/faq.html#listserver
        """
        message_id = self.message['message-id']
        list_post = email.utils.parseaddr(self.message['to'])
        if ( message_id is not None ) and ( list_post[1] is not '' ):
            # remove < and > from msg-id
            sha = hashlib.sha1( message_id[1:-1] )
            sha.update( list_post[1] )
            hash = base64.urlsafe_b64encode( sha.digest() )
            url = "<http://go.mail-archive.com/%s>" % hash
            self.message['Archived-At'] = url
            # in case debugging is needed
            #self.message['X-Archived-At-msgid'] = message_id[1:-1]
            #self.message['X-Archived-At-list-post'] = list_post[1]

    def parse_singlepart( self ):
        """
        Parses a singlepart message object.
        """
        content_type = self.message.get_content_type()
        if content_type == "text/plain":
            # text/plain is fine, if I'm a submessage I want to be included
            self.to_include = True
        elif content_type == "text/html":
            # text/html must be stripped down to text/plain
            s = StripHTML()
            # Feed the HTML-Stripper with the body of the message
            s.feed( self.message.get_payload() )
            # And set the body of the message to the output of the HTML-Stripper 
            self.message.set_payload( s.get_plain_data() )
            # if I'm a submessage I also want to be included
            self.to_include = True
            # rewrite the Content-Type header, im no longer an evil HTML mail :)
            self.message.set_type( "text/plain" )
    def get_string( self ):
        """
        Returns the string representation of the supplied message.
        """
        return self.message.as_string()

    def append_footer_from_file( self, filename ):
        rawfooter = None
        footer = None
        orig_cs = self.message.get_content_charset()
        if orig_cs == None:
            cs = "iso-8859-15"
            orig_cs = "ascii"
        else:
            cs = orig_cs
        try:
            with open( filename ) as f:
                rawfooter = f.read()
        except:
            if not self.keep_going:
                raise
        if rawfooter:
            try:
                footer = rawfooter.decode( "utf-8" ).encode( cs )
            except:
                cs = "utf-8"
                footer = rawfooter.decode( "utf-8" ).encode( cs )
        if footer:
            sep = "\n"
            if footer.startswith( "\n" ):
                sep = ""
            payload = self.message.get_payload( decode = True ).decode( orig_cs ).encode( cs ) + sep + footer
            del self.message["MIME-Version"]
            del self.message["Content-Transfer-Encoding"]
            self.message.set_payload( payload, cs )


if __name__ == "__main__":
    if options.print_version:
        print("pymime "+__version__)
        exit()
    if options.input == "-":
        input = sys.stdin
    else:
        input = file( options.input )
    if options.output == "-":
        output = sys.stdout
    else:
        output = file( options.output, "w" )
    e = EMail( keep_going = options.keep_going )
    e.feed( fp = input )
    e.parse()
    if options.archive_header:
        e.addArchiveHeader()
    if options.footer:
        e.append_footer_from_file( options.footer )
    output.write( e.get_string() )
    output.close()
