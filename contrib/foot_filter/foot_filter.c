/*

foot_filter.c

(C) 2010 Ben Schmidt

This Source Code Form is subject to the terms of the Mozilla Public License
Version 2.0. If a copy of the MPL was not distributed with this file, You can
obtain one at http://mozilla.org/MPL/2.0/.

*/

// Check out the -V option; it outputs this and more
#define FOOT_FILTER_VERSION "foot_filter version 1.2, (C) 2010 Ben Schmidt"

static const char * USAGE="\n\
usage: foot_filter [-p plain_footer_file] [-h html_footer_file]\n\
                   [{-P|-H} mime_footer_file] [-s]\n\
       foot_filter -V\n\
\n\
plain_footer_file, if present, will be appended to mails with plain text\n\
sections only. Similarly, html_footer_file. If mime_footer_file (either\n\
plain, -P, or HTML, -H) is given, it will be used when a mail with\n\
alternative formats is encountered, or if the footer for the relevant\n\
type of mail is not present; a new MIME section will be added.\n\
\n\
-s turns on smart mode which endeavours to remove included/quoted copies of\n\
the (or a similar) footer by surrounding the footer with patterns it later\n\
recognises. It also endeavours to strip 'padding' surrounding the old\n\
footers to make things as clean as possible. This includes whitespace\n\
(including '&nbsp;' and '<br>'), '>' quoting characters, various pairs of\n\
HTML tags (p, blockquote, div, span, font; it's naive, it doesn't check\n\
tags in between are balanced at all, so in '<p>prefix</p><p>suffix</p>' the\n\
first and last tags are paired), and even horizontal rules when inside\n\
paired tags (e.g. use '<div><hr/>footer</div>'). If the smart strings are\n\
found in the footer, they won't be added by the program, so you have the\n\
necessary control to do this.\n\
\n\
New footers are added prior to trailing whitespace and a few closing html\n\
tags (body, html) as well. You almost certainly want to begin your footer\n\
with an empty line because of this.\n\
\n\
Since these alterations, by their very nature, break signed mail,\n\
signatures are removed while processing. To keep some value from signatures,\n\
have the MTA verify them and add a header (or even supply an alternative\n\
footer to this program), and resign them to authenticate they came from the\n\
mailing list directly after the signature verification was done and recorded.\n\
Or don't use these kinds of transformations at all.\n\
\n\
-V shows the version and exits.\n\
\n\
Program is running now. Send EOF or interrupt to stop it. To avoid this usage\n\
message if wanting to run without arguments, use '--' as an argument.\n\
\n";

/*

This is a fairly simple program not expecting much extension. As such, some
liberties have been taken and some fun has been had by the author. Correctness
has been prioritised in design, but speed and efficiency have been taken into
consideration and prioritised above readability and modularity and other such
generally recommended programming practices. If making changes, great care
should be taken to understand how and where (everywhere) globals are used
before making them. Don't try to modify the program without understanding how
the whole thing works together or you will get burnt. You have been warned.

Relevant RFCs:
http://www.ietf.org/rfc/rfc2015.txt
http://www.ietf.org/rfc/rfc3851.txt
http://www.ietf.org/rfc/rfc2045.txt
http://www.ietf.org/rfc/rfc2046.txt
http://www.ietf.org/rfc/rfc822.txt
http://www.ietf.org/rfc/rfc2183.txt

For program configuration, see the 'constants' section below.

Also see code comments throughout.

Future possibilities:

- Saving copies of original mail in 'semi-temp' files for debugging.

- Stripping attachments and save them (e.g. in a location that can become a
  'files uploaded' section on a website). Replace them with links to the
  website, even.

- Making the prefixes, suffixes, replacements, padding, guts, pairs,
  configurable at runtime.

- Attaching signed mail, or wrapping in a multipart rather than removing
  signatures; wouldn't be hard if always using MIME footers.

- Following a script to allow various other header transformations (addition,
  removal, etc.), or other transformations.

- Prologues as well as or instead of footers.

*/

/* tag: includes */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>

/* tag: typedefs */

// splint has bools, but C doesn't!
#ifndef S_SPLINT_S
typedef int bool;
#define false (0)
#define true (1)
#endif

// This is mostly to be able to include splint annotations
typedef /*@null@*//*@observer@*/ const char * const_null_string;
typedef /*@null@*/ char * null_string;
typedef /*@null@*//*@owned@*/ char * owned_null_string;
typedef /*@null@*//*@dependent@*/ char * dependent_null_string;

// 'Callbacks'; they communicate primarily using globals, see below
typedef bool (*callback_t)();
typedef void (*function_t)();

// For fill()
typedef enum {
	echo,
	encode,
	shunt,
	discard,
	stop,
	fail
} when_full_t;

// Various places
typedef enum {
	unencoded,
	quoted_printable,
	base64
} encoding_t;

// For returning multiple characters, and a request to delete backlog
// when decoding
typedef struct {
	int r;
	int c1;
	int c2;
	int c3;
} decode_t;

/* tag: constants */

/* tag: header_constants */

// How many MIME Content- headers we expect, maximum, in a mail. If we have
// more than that, we won't be able to process MIME so well, but we won't fail
// catastrophically.
#define mime_headers_max 16

/* tag: footer_constants */

// Stuff for processing the footer's smart removal and (smart or not)
// insertion

static const char * plain_prefix = "------~----------";
static const char * plain_suffix = "------~~---------";
static const char * plain_replacement = "\r\n\r\n";
static const_null_string plain_tails[] = {
	" ","\t","\r","\n",
	NULL
};
static const_null_string plain_padding[] = {
	">"," ","\t","\r","\n",
	NULL
};
static const_null_string plain_guts[] = {
	NULL
};
static const_null_string plain_pairs[] = {
	NULL
};

static const char * html_prefix = "------~----------";
static const char * html_suffix = "------~~---------";
static const char * html_replacement = "\r\n<br/><br/>\r\n";
static const_null_string html_tails[] = {
	"</html>","</HTML>","</body>","</BODY>",
	" ","&nbsp;","&NBSP;","\t","\r","\n",
	"<br>","<BR>","<br/>","<BR/>","<br />","<BR />",
	NULL
};
static const_null_string html_padding[] = {
	"&gt;","&GT;",
	" ","&nbsp;","&NBSP;","\t","\r","\n",
	"<br>","<BR>","<br/>","<BR/>","<br />","<BR />",
	NULL
};
static const_null_string html_guts[] = {
	// These are removed in an attempt to make a pair
	"<hr>","<HR>","<hr/>","<HR/>","<hr />","<HR />",
	" ","&nbsp;","&NBSP;","\t","\r","\n",
	"<br>","<BR>","<br/>","<BR/>","<br />","<BR />",
	NULL
};
static const_null_string html_pairs[] = {
	// Closing part (or NULL to mark no more), end of opening part,
	// start of opening part, NULL
	// The search strategy is fairly naive; if it finds the closing part,
	// it checks for the end of the opening part; if it finds that, it
	// searches back for the first character of each of the opening part
	// variants, and if that character is found and is the beginning of the
	// whole variant, it removes the pair.
	"</p>",">","<p>","<p ",NULL,
	"</P>",">","<P>","<P ",NULL,
	"</blockquote>",">","<blockquote>","<blockquote ",NULL,
	"</BLOCKQUOTE>",">","<BLOCKQUOTE>","<BLOCKQUOTE ",NULL,
	"</div>",">","<div>","<div ",NULL,
	"</DIV>",">","<DIV>","<DIV ",NULL,
	"</span>",">","<span>","<span ",NULL,
	"</SPAN>",">","<SPAN>","<SPAN ",NULL,
	"</font>",">","<font>","<font ",NULL,
	"</FONT>",">","<FONT>","<FONT ",NULL,
	NULL
};

/* tag: buffer_constants */

// Also see comment at buffer_globals about how the buffer works.

// The buffer size limits how far back footers can be deleted; the
// section of mail from the closing boundary back this far will be
// searched for footers to remove.
#define mem_buffer_size 65536
// mem_buffer_keep is how much we keep in memory when shunting
// off to disk; as we must be able to shunt off at least something
// to disk each time we need to, this must be at least 2 bytes
// less than mem_buffer_size. This is how much we will be able to
// backtrack in memory, e.g. to strip whitespace. Something a little
// larger than the SMTP line length limit of 998 should keep it safe.
#define mem_buffer_keep 1024
// mem_buffer_margin is how much space we keep in memory in case
// a callback (decoding!) needs to use it. Must be at least 4 for
// decoding not to cause nasty corruption.
#define mem_buffer_margin 8
// The number of replacements we may wish to make; usually removing
// the MIME headers is the most complicated thing to be done, plus
// removing the MIME version header and the newline that ends the
// headers.
#define replacements_max (mime_headers_max+4)

/* tag: error_constants */

// Enable one or both of these
#define USE_SYSLOG
//#define USE_STDERR

/* tag: helper_constants */

// Disable this to use stdin/stdout with CRLF line endings as the spec
// and transports do; footer files are always expected to have UNIX line
// endings
#define UNIX_EOL

/* tag: globals */

/* tag: header_globals */

// Offsets into the buffer; only valid while it's still in there!
static int mime_header_starts[mime_headers_max]={0};
static int mime_header_ends[mime_headers_max]={0};
// String copies (until freed)
static owned_null_string saved_mime_headers[mime_headers_max]={NULL};
// Count applies to all header globals above
static int mime_headers_count = 0;

// Individual header strings which are processed to have comments
// removed and have normalised syntax for easy deduction of types,
// etc.. _header_body pointers point into the _header strings.
// Indexes are into the header arrays above.
// We store the version start and end separately as we want to delete it,
// but not reoutput it.
static /*@owned@*/ null_string version_header=NULL;
static /*@dependent@*/ null_string version_header_body=NULL;
static int version_header_start=0;
static int version_header_end=0;
static /*@owned@*/ null_string type_header=NULL;
static /*@dependent@*/ null_string type_header_body=NULL;
static int type_header_index=0;
static /*@owned@*/ null_string transfer_header=NULL;
static /*@dependent@*/ null_string transfer_header_body=NULL;
static int transfer_header_index=0;
static /*@owned@*/ null_string disposition_header=NULL;
static /*@dependent@*/ null_string disposition_header_body=NULL;
static int disposition_header_index=0;

// Flag that we had errors reading headers so not to do anything fancy
static bool mime_bad = false;

/* tag: footer_globals */

// Footer text; we just store \n but it is translated to CRLF
// The actual buffers
static /*@owned@*/ null_string plain_footer_buffer = NULL;
static /*@owned@*/ null_string html_footer_buffer = NULL;
static /*@owned@*/ null_string mime_footer_buffer = NULL;
// Pointer to the string we're actually using
static /*@dependent@*/ null_string plain_footer = NULL;
static /*@dependent@*/ null_string html_footer = NULL;
static /*@dependent@*/ null_string mime_footer = NULL;
static bool html_mime_footer = false;
// Whether to attempt deletion by surrounding the footer with special strings
static bool smart_footer = false;

/* tag: buffer_globals */

// We have a buffer that may be partly on disk (disk_buffer), then
// in memory (mem_buffer). The memory buffer wraps around as necessary.
// The disk buffer begins at disk_buffer_start in the file. The _filled
// variables say how much is in each part of the buffer, and the total
// is in buffer_filled. A portion of the buffer is considered to have
// been 'read'; before this is the lookbehind, and after this is the
// lookahead. The buffer may also be marked at a certain location,
// which is and should be almost always in the lookbehind (if in the
// lookahead this should only be very temporary).

static char mem_buffer_start[mem_buffer_size];
static char * mem_buffer_end = mem_buffer_start+mem_buffer_size;
static char * mem_buffer_next_empty = mem_buffer_start; // logical start
static char * mem_buffer_next_fill = mem_buffer_start; // logical end + 1
static int mem_buffer_filled=0;

static /*@null@*/ FILE * disk_buffer = NULL;
// Cannot pass mkstemp an unwritable string, so careful to declare this
// as an array, not a pointer which would observe the string constant.
static char disk_buffer_template[] = "foot_filter.XXXXXX";
static int disk_buffer_start=0; // offset into temp file for buffer
static int disk_buffer_filled=0;
// disk_buffer_sought: location we are at in the temp file
static int disk_buffer_sought=0;

// at all times, buffer_filled == mem_buffer_filled + disk_buffer_filled
static int buffer_filled=0;

static int buffer_read=0; // offset into buffer
static int buffer_mark=0; // offset into buffer; should be in lookbehind
static bool buffer_marked=false;

// The first and 'after last' characters to replace are stored
static int replacement_starts[replacements_max] = {0};
static int replacement_ends[replacements_max] = {0};
static const_null_string replacement_strings[replacements_max] = {NULL};
static int replacements_count=0;

/* tag: callback_globals */

// Used to communicate a character from the buffer to callbacks by the
// functions the callback is directly called by
static int buffer_char;

// Used to communicate information between callback functions and the
// functions that set up the callback (not the function that actually
// calls the callback), and possibly for internal callback state. The
// callbacks document how these should be used. Take special care to
// follow the instructions, or things will go bad and be hard to track
// down!
static /*@dependent@*/ null_string callback_save;
static const_null_string callback_compare;
static int callback_match;
static int callback_int;
static bool callback_bool;

/* tag: encoding_globals */

// Current modes
static encoding_t encoding;
static encoding_t decoding;

// State for the routines
static char encoding_buffer[4];
static int encoding_filled=0;
static int encoding_echoed=0;
static char decoding_buffer[4];
static int decoding_filled=0;
static int decoding_white=0;

/* tag: prototypes */

// Comments are made at function definitions where warranted.

/* tag: main_prototypes */

int main(int argc,char * argv[]);

static void load_footer(/*@out@*//*@shared@*/ char ** footer,
		/*@reldef@*/ char ** footer_buffer,
		char * file,
		/*@unique@*/ const_null_string prefix,
		/*@unique@*/ const_null_string suffix);

static void process_section(bool add_footer,
		bool can_reenvelope, /*@null@*/ bool * parent_needs_footer);

/* tag: header_prototypes */

static inline void read_and_save_mime_headers();
static char * remove_comments(/*@returned@*/ char * header,bool ext);
static inline bool delimiting(char c,bool ext);
static inline void remove_mime_headers();
static inline void output_saved_mime_headers();
static void free_saved_mime_headers();
static bool /*@falsewhennull@*/ is_multipart(/*@null@*//*@out@*/ char ** boundary);
static inline bool is_signed();
static inline bool is_alternative();
static inline bool is_mixed();
static inline bool is_html();
static inline bool is_plain();
static inline bool is_signature();
static inline bool is_attachment();
static inline void set_decoding_type();
static inline void change_to_mixed();
static inline void generate_boundary(/*@out@*/ char ** boundary);
static inline void output_mime_mixed_headers(const char * boundary);
static inline void output_prolog();
static void output_mime_footer(const char * boundary);
static inline void output_boundary(const char * boundary);
static inline void output_final_boundary(const char * boundary);
static bool at_final_boundary(char * boundary);

/* tag: footer_prototypes */

static inline void process_text_section(bool add_footer,
		/*@null@*/ const char * footer,
		const char * prefix, const char * suffix, const char * replacement,
		const_null_string * tails, const_null_string * padding,
		const_null_string * guts, const_null_string * pairs,
		char * boundary);
static inline void pad(const_null_string * padding,
		const_null_string * guts, const_null_string * pairs,
		int * prefix_pos, int * suffix_pos);
static inline void mark_tail(const_null_string * padding);
static inline void encode_footer(const char * footer);

/* tag: buffer_prototypes */

static inline void read_buffer();
static inline void echo_buffer();
static inline void skip_buffer();
static inline void echo_lookbehind();
static inline void encode_lookbehind();
static inline void encode_replacements();
static inline void make_replacements(callback_t one_char,
		callback_t start_marked);
static inline void encode_to_mark();
static inline void echo_disk_buffer();
static inline void skip_disk_buffer();
static inline void read_boundary(/*@out@*/ char ** boundary);
static inline void echo_to_boundary(const char * boundary);
static inline void skip_to_boundary(const char * boundary);
static inline void decode_and_read_to_boundary_encoding_when_full(
		const char * boundary);
static inline bool process_one_line_checking_boundary(callback_t n_chars,
		/*@null@*/ function_t process, callback_t processing,
		when_full_t when_full, const char * boundary);

static int pos_of(const char * text,int from,int to);

static bool look(callback_t callback,int from,bool read);
static bool lookback(callback_t callback,int from,bool mark);
static bool empty(callback_t callback);
static bool fill(callback_t callback, when_full_t when_full);

static inline void create_disk_buffer();
static void remove_disk_buffer();
static inline void shunt_to_disk(int n);

/* tag: callback_prototypes */

static bool one_char();
static bool echoing_one_char();
static bool encoding_one_char();
static bool n_chars();
static bool echoing_n_chars();
static bool encoding_n_chars();
static bool saving_n_chars();
static bool n_chars_until_match();
static bool until_eol();
// static bool echoing_until_eol();
static bool counting_until_eol();
static bool saving_until_eol();
static bool decoding_until_eol();
// static bool until_no_lookbehind();
static bool echoing_until_no_lookbehind();
static bool encoding_until_no_lookbehind();
static bool until_no_disk_buffer();
static bool echoing_until_no_disk_buffer();
static bool encoding_until_no_disk_buffer();
static bool until_no_buffer();
// static bool echoing_until_no_buffer();
static bool until_start_marked();
static bool echoing_until_start_marked();
static bool encoding_until_start_marked();
static bool until_match();
static bool comparing_head();
static bool case_insensitively_comparing_head();

/* tag: encoding_prototypes */

static inline void encode_string(const char * s);
static void encodechar(int c);
static inline void finish_encoding();
static /*@reldef@*/ decode_t decodechar(int c);
static void decode_lookahead();
static inline void finish_decoding();

static inline int decode_hex(int c);
static inline int decode_64(int c);
static inline void encode_hex_byte(unsigned int h);
static inline void encode_64(unsigned int b);

/* tag: error_prototypes */

static inline void * alloc_or_exit(size_t s) /*@allocates result@*/;
static inline void /*@noreturnwhentrue@*/
		resort_to_exit(bool when,const char * message,int status);
static inline void /*@noreturnwhentrue@*/
		resort_to_errno(bool when,const char * message,int status);
static inline void resort_to_warning(bool when,const char * message);
static inline void warning(const char * message);

/* tag: helper_prototypes */

static inline int get();
static inline int put(int c);
static inline int putstr(const char * s);

static inline bool case_insensitively_heads(const char * head,const char * buffer);

/* tag: functions */

/* tag: main_functions */

int main(int argc, char * argv[]) {
	int opt;
	bool show_version=false;
	null_string plain_footer_file=NULL;
	null_string html_footer_file=NULL;
	null_string mime_footer_file=NULL;
	// Initialise
	resort_to_errno(atexit(remove_disk_buffer)!=0,
			"cannot register exit function",EX_OSERR);
	srandom((unsigned int)(getpid()*time(NULL)));
	// Parse args
	while ((opt=getopt(argc,argv,"p:h:P:H:sV"))!=-1) {
		switch ((char)opt) {
		case 'p': plain_footer_file=optarg; break;
		case 'h': html_footer_file=optarg; break;
		case 'P': mime_footer_file=optarg; html_mime_footer=false; break;
		case 'H': mime_footer_file=optarg; html_mime_footer=true; break;
		case 's': smart_footer=true; break;
		case 'V': show_version=true; break;
		default: warning("unrecognised commandline option");
		}
	}
	if (show_version||argc<2) {
		printf("%s\n",FOOT_FILTER_VERSION);
#ifdef UNIX_EOL
		printf("   with UNIX line endings\n");
#else
		printf("   with DOS line endings\n");
#endif
		printf("   reporting errors to: ");
#ifdef USE_SYSLOG
		printf("syslog ");
#endif
#ifdef USE_STDERR
		printf("stderr ");
#endif
		printf("\n");
		if (argc<2) fprintf(stderr,"%s",USAGE);
		if (show_version) exit(EX_OK);
	}
	argc-=optind;
	argv+=optind;
	resort_to_warning(argc>0,"unexpected commandline argument");
	// Load footers
	if (plain_footer_file!=NULL)
			load_footer(&plain_footer,&plain_footer_buffer,
			plain_footer_file,
			smart_footer?plain_prefix:NULL,smart_footer?plain_suffix:NULL);
	if (html_footer_file!=NULL)
			load_footer(&html_footer,&html_footer_buffer,
			html_footer_file,
			smart_footer?html_prefix:NULL,smart_footer?html_suffix:NULL);
	if (mime_footer_file!=NULL)
			load_footer(&mime_footer,&mime_footer_buffer,
			mime_footer_file,NULL,NULL);
	// Do the job
	process_section(true,true,NULL);
	// Finish
	if (plain_footer_buffer!=NULL) free(plain_footer_buffer);
	if (html_footer_buffer!=NULL) free(html_footer_buffer);
	if (mime_footer_buffer!=NULL) free(mime_footer_buffer);
	exit(EX_OK);
}

static void load_footer(/*@out@*//*@shared@*/ char ** footer,
		/*@reldef@*/ char ** footer_buffer,
		char * file,
		/*@unique@*/ const_null_string prefix,
		/*@unique@*/ const_null_string suffix) {
	FILE * f;
	int prefixl=0, footerl=0, suffixl=0;
	char * ff;
	if (prefix!=NULL&&suffix!=NULL) {
		prefixl=(int)strlen(prefix);
		suffixl=(int)strlen(suffix);
	}
	f=fopen(file,"r");
	resort_to_errno(f==NULL,"error opening footer file",EX_NOINPUT);
	resort_to_errno(fseek(f,0,SEEK_END)!=0,
			"error seeking end of footer file",EX_IOERR);
	resort_to_errno((footerl=(int)ftell(f))==-1,
			"error finding footer length",EX_IOERR);
	resort_to_errno(fseek(f,0,SEEK_SET)!=0,
			"error seeking in footer file",EX_IOERR);
	// prefix, \n, footer, \n, suffix, \0
	*footer_buffer=alloc_or_exit(sizeof(char)*(prefixl+footerl+suffixl+3));
	*footer=*footer_buffer;
	*footer+=prefixl+1;
	resort_to_errno(fread(*footer,1,(size_t)footerl,f)<(size_t)footerl,
			"error reading footer",EX_IOERR);
	// We strip off a single trailing newline to keep them from accumulating
	// but to allow the user the option of adding them if desired
	if ((*footer)[footerl-1]=='\n') --footerl;
	(*footer)[footerl]='\0';
	if (prefix==NULL||suffix==NULL) return;
	// Put in the prefix and suffix as necessary
	ff=strstr(*footer,prefix);
	if (ff!=NULL) {
		ff=strstr(ff,suffix);
		if (ff!=NULL) return;
		(*footer)[footerl]='\n';
		++footerl;
		strcpy(*footer+footerl,suffix);
		(*footer)[footerl+suffixl]='\0';
	} else {
		ff=strstr(*footer,suffix);
		if (ff==NULL) {
			(*footer)[footerl]='\n';
			++footerl;
			strcpy(*footer+footerl,suffix);
			(*footer)[footerl+suffixl]='\0';
		}
		*footer-=prefixl+1;
		strcpy(*footer,prefix);
		(*footer)[prefixl]='\n';
	}
}

// Should be called with the boundary for the section as lookahead
// in the buffer, but nothing more, and no lookbehind.
static void process_section(bool add_footer,
		bool can_reenvelope, /*@null@*/ bool * parent_needs_footer) {
	char * external=NULL;
	char * internal=NULL;
	char * generated=NULL;
	bool reenveloping=false;
	bool child_needed_footer=false;
	bool needs_footer=false;
	bool unsigning=false;
	if (parent_needs_footer!=NULL) *parent_needs_footer=false;
	// The headers must be read, saved and echoed before making any
	// recursive calls, as I'm naughty and using globals.
	read_boundary(&external);
	read_and_save_mime_headers();
	if (mime_bad) {
		// If an error, just resort to echoing
		echo_buffer(); // Boundary and headers
		// End headers with the extra line break
		resort_to_errno(putstr("\r\n")==EOF,
				"error echoing string",EX_IOERR);
		free_saved_mime_headers();
		// Body
		echo_to_boundary(external);
		free(external);
		return;
	}
	// Headers determining we skip this section
	if (is_signature()) {
		skip_buffer(); // Boundary and headers
		skip_to_boundary(external);
		return;
	}
	// Header processing
	if (is_signed()) unsigning=true;
	if (unsigning) change_to_mixed();
	if (add_footer&&mime_footer!=NULL&&(
			is_alternative()||(is_multipart(NULL)&&!is_mixed())||
			(is_plain()&&plain_footer==NULL)||
			(is_html()&&html_footer==NULL)
			)) {
		add_footer=false;
		if (can_reenvelope) {
			reenveloping=true;
			remove_mime_headers();
		} else if (parent_needs_footer!=NULL) *parent_needs_footer=true;
	}
	// Headers
	echo_buffer(); // Boundary and possibly modified headers
	if (reenveloping) {
		generate_boundary(&generated);
		output_mime_mixed_headers(generated);
		output_prolog();
		output_boundary(generated);
		output_saved_mime_headers();
	}
	// End the headers with the extra line break
	resort_to_errno(putstr("\r\n")==EOF,
			"error echoing string",EX_IOERR);
	// Body processing
	if (is_multipart(&internal)) {
		// This branch frees the MIME headers before recursing.
		// Don't include the prolog if it used to be signed;
		// it usually says something like 'this message is signed'
		if (unsigning) {
			skip_to_boundary(internal);
			resort_to_errno(putstr("\r\n")==EOF,
					"error echoing string",EX_IOERR);
		} else {
			echo_to_boundary(internal);
		}
		// The recursive call needs these globals
		free_saved_mime_headers();
		while (!at_final_boundary(internal)) {
			process_section(add_footer,false,&child_needed_footer);
			if (child_needed_footer) needs_footer=true;
		}
		if (needs_footer) output_mime_footer(internal);
		free(internal);
		echo_to_boundary(external);
	} else {
		// This branch frees the MIME headers at the end
		if (!is_attachment()&&(
				(is_plain()&&plain_footer!=NULL)||
				(is_html()&&html_footer!=NULL))) {
		// alternatively
		// if (!is_attachment()&&(
		//		(is_plain()&&((add_footer&&plain_footer!=NULL)||smart_footer))||
		//		(is_html()&&((add_footer&&html_footer!=NULL)||smart_footer)))) {
			if (is_plain()) {
				process_text_section(add_footer,plain_footer,
						plain_prefix,plain_suffix,plain_replacement,
						plain_tails,plain_padding,plain_guts,plain_pairs,external);
			} else {
				process_text_section(add_footer,html_footer,
						html_prefix,html_suffix,html_replacement,
						html_tails,html_padding,html_guts,html_pairs,external);
			}
		} else {
			echo_to_boundary(external);
		}
		free_saved_mime_headers();
	}
	// MIME stuff is freed now; take care not to use it.
	/*@-branchstate@*/
	if (reenveloping) {
		// We ensure generated is not null in another if(reenveloping)
		// conditional above
		/*@-nullpass@*/
		output_mime_footer(generated);
		output_final_boundary(generated);
		free(generated);
		/*@=nullpass@*/
	}
	/*@=branchstate@*/
	free(external);
}

/* tag: header_functions */

static inline void read_and_save_mime_headers() {
	/*@-mustfreeonly@*/
	mime_bad=false;
	// Mark current end of buffer
	buffer_mark=buffer_read;
	buffer_marked=true;
	for (;;) {
		do {
			// Extend current header until beginning of next
			callback_bool=false;
			(void)fill(until_eol,shunt);
			if (buffer_filled==buffer_read) {
				// We probably hit EOF; just get out, and the whole
				// mail will end up echoed out
				warning("unexpected end of input");
				break;
			}
			(void)look(one_char,buffer_read,false);
			if (callback_int==(int)' '||callback_int==(int)'\t') {
				// Continuation of previous header; read it
				read_buffer();
				continue;
			}
			// Start of new header; don't read it; process the old one
			// (from the mark to the end of the lookbehind)
			break;
		} while (true);
		// Process the old header, if there is one
		if (buffer_mark<buffer_read) {
			do {
				callback_compare="MIME-Version:";
				(void)look(case_insensitively_comparing_head,buffer_mark,false);
				if (callback_bool) {
					// MIME version header
					version_header_start=buffer_mark;
					version_header_end=buffer_read;
					version_header=alloc_or_exit(sizeof(char)*(buffer_read-buffer_mark+1));
					callback_save=version_header;
					callback_int=buffer_read-buffer_mark;
					callback_save[callback_int]='\0';
					(void)look(saving_n_chars,buffer_mark,false);
					callback_save=NULL;
					version_header_body=remove_comments(version_header,true);
					if (!case_insensitively_heads("1.0",version_header_body)) {
						mime_bad=true;
					}
					break;
				}
				callback_compare="Content-";
				(void)look(case_insensitively_comparing_head,buffer_mark,false);
				if (!callback_bool) break;
				// Another MIME header
				if (mime_headers_count==mime_headers_max) {
					warning("too many MIME headers");
					mime_bad=true;
					break;
				}
				mime_header_starts[mime_headers_count]=buffer_mark;
				mime_header_ends[mime_headers_count]=buffer_read;
				saved_mime_headers[mime_headers_count]=
						alloc_or_exit(sizeof(char)*(buffer_read-buffer_mark)+1);
				saved_mime_headers[mime_headers_count][0]='\0';
				callback_save=saved_mime_headers[mime_headers_count];
				callback_int=buffer_read-buffer_mark;
				callback_save[callback_int]='\0';
				(void)look(saving_n_chars,buffer_mark,false);
				callback_compare="Content-Type:";
				(void)look(case_insensitively_comparing_head,buffer_mark,false);
				if (callback_bool) {
					type_header=alloc_or_exit(sizeof(char)*(buffer_read-buffer_mark+1));
					strcpy(type_header,saved_mime_headers[mime_headers_count]);
					type_header_body=remove_comments(type_header,true);
					type_header_index=mime_headers_count;
					++mime_headers_count;
					break;
				}
				callback_compare="Content-Transfer-Encoding:";
				(void)look(case_insensitively_comparing_head,buffer_mark,false);
				if (callback_bool) {
					transfer_header=alloc_or_exit(sizeof(char)*(buffer_read-buffer_mark+1));
					strcpy(transfer_header,saved_mime_headers[mime_headers_count]);
					transfer_header_body=remove_comments(transfer_header,true);
					transfer_header_index=mime_headers_count;
					++mime_headers_count;
					break;
				}
				callback_compare="Content-Disposition:";
				(void)look(case_insensitively_comparing_head,buffer_mark,false);
				if (callback_bool) {
					disposition_header=alloc_or_exit(sizeof(char)*(buffer_read-buffer_mark+1));
					strcpy(disposition_header,saved_mime_headers[mime_headers_count]);
					disposition_header_body=remove_comments(disposition_header,true);
					disposition_header_index=mime_headers_count;
					++mime_headers_count;
					break;
				}
				++mime_headers_count;
			} while (false);
		}
		// Mark the new header
		buffer_mark=buffer_read;
		if (buffer_read==buffer_filled) {
			// EOF; return
			return;
		}
		// Read the first part of the new header; loop to read rest
		read_buffer();
		callback_compare="\r\n";
		(void)look(comparing_head,buffer_mark,false);
		if (callback_bool) {
			// End of headers; strip the extra line; return
			resort_to_exit(replacements_count==replacements_max,
					"internal error: too many replacements",EX_SOFTWARE);
			replacement_starts[replacements_count]=buffer_read-2;
			replacement_ends[replacements_count]=buffer_read;
			replacement_strings[replacements_count]=NULL;
			++replacements_count;
			return;
		}
	}
	/*@=mustfreeonly@*/
}
// Returns a pointer to the body part of the header field
static char * remove_comments(/*@returned@*/ char * header,bool ext) {
	// This removes comments and any superfluous whitespace in the
	// header (a structured header, that is, RFC822); it fiddles with
	// the quoted strings in such a way that backslash escaping means
	// simply take the next character literally, rather than needing
	// to do funny things with folded strings. The result is not suitable
	// for output.
	char * h=header;
	char * hh;
	char * hhh;
	char * body=NULL;
	char close;
	int levels=0;
	while (*h!=':') ++h;
	++h;
	if (*h==' '||*h=='\t') ++h;
	body=h;
	hh=h;
	while (*h!='\0') {
		if (*h=='\r'&&*(h+1)=='\n') {
			h+=2;
			continue;
		} else if ((*h==' '||*h=='\t')&&delimiting(*(hh-1),ext)) {
			++h;
			continue;
		} else if (delimiting(*h,ext)&&(*(hh-1)==' '||*(hh-1)=='\t')) {
			if (hh!=body) --hh;
		}
		if (*h=='(') {
			++h;
			levels=1;
			while (levels>0) {
				if (*h=='\0') break;
				if (*h=='\\') {
					++h;
					if (*h=='\0') break;
				}
				else if (*h=='(') ++levels;
				else if (*h==')') --levels;
				++h;
			}
			if (!delimiting(*h,ext)&&!delimiting(*(hh-1),ext)) {
				// Put in some whitespace if something delimiting isn't
				// coming and hasn't just been
				*hh=' ';
				++hh;
			}
			continue;
		} else if (*h=='"'||*h=='[') {
			if (*h=='[') close=']';
			else close='"';
			*hh=*h;
			++h; ++hh;
			hhh=hh;
			while (*h!='\0'&&*h!=close) {
				if (*h=='\\') {
					*hh=*h;
					++hh; ++h;
					if (*h=='\0') break;
					if (*h=='\r'&&*(h+1)=='\n') {
						*hh=*h; ++hh; ++h;
						*hh=*h; ++hh; ++h;
						if (*h=='\0') break;
						++hh; ++h;
						continue;
					}
				} else if (*h==(char)8) {
					--hh; ++h;
					if (hh<hhh) hh=hhh;
					continue;
				} else if (*h=='\r'&&*(h+1)=='\n') {
					h+=2;
					continue;
					// alternatively
					// *hh=*h; ++hh; ++h;
					// *hh=*h; ++hh; ++h;
					// if (*h=='\0') break;
					// ++h;
					// continue;
					// or perhaps even (the spec is a bit ambiguous)
					// h+=2;
					// if (*h=='\0') break;
					// ++h;
					// continue;
				}
				*hh=*h;
				++h; ++hh;
			}
			if (*h=='\0') break;
		}
		*hh=*h;
		++h; ++hh;
	}
	if (*(hh-1)==' ') *(hh-1)='\0';
	else *hh='\0';
	return body;
}
static inline bool delimiting(char c,bool ext) {
	// 'ext' (extended) delimiters include '/', '?' and '=' and assist
	// by removing whitespace surrounding those, as these are
	// delimiters in the MIME header fields, even though not RFC822;
	// note that MIME doesn't use '.' but we still do.
	return (c==' '||c=='\t'||c=='<'||c=='>'||c=='@'||
			c==','||c==';'||c==':'||c=='\\'||c=='"'||
			c=='.'||c=='['||c==']'||
			(ext&&(c=='/'||c=='='||c=='?')));
}
static inline void remove_mime_headers() {
	int h;
	for (h=0;h<mime_headers_count;++h) {
		resort_to_exit(replacements_count==replacements_max,
				"internal error: too many replacements",EX_SOFTWARE);
		replacement_starts[replacements_count]=mime_header_starts[h];
		replacement_ends[replacements_count]=mime_header_ends[h];
		replacement_strings[replacements_count]=NULL;
		++replacements_count;
	}
	if (version_header!=NULL) {
		resort_to_exit(replacements_count==replacements_max,
				"internal error: too many replacements",EX_SOFTWARE);
		replacement_starts[replacements_count]=version_header_start;
		replacement_ends[replacements_count]=version_header_end;
		replacement_strings[replacements_count]=NULL;
		++replacements_count;
	}
}
static inline void output_saved_mime_headers() {
	int h;
	for (h=0;h<mime_headers_count;++h) {
		// The header includes its terminating CRLF
		/*@-nullpass@*/
		resort_to_errno(putstr(saved_mime_headers[h])==EOF,
				"error echoing string",EX_IOERR);
		/*@=nullpass@*/
	}
}
static void free_saved_mime_headers() {
	int h;
	for (h=0;h<mime_headers_count;++h) {
		/*@-nullpass@*/
		free(saved_mime_headers[h]);
		/*@=nullpass@*/
	}
	mime_headers_count=0;
	if (version_header!=NULL) free(version_header);
	version_header=NULL;
	version_header_body=NULL;
	if (type_header!=NULL) free(type_header);
	type_header=NULL;
	type_header_body=NULL;
	if (transfer_header!=NULL) free(transfer_header);
	transfer_header=NULL;
	transfer_header_body=NULL;
	if (disposition_header!=NULL) free(disposition_header);
	disposition_header=NULL;
	disposition_header_body=NULL;
}
static bool /*@falsewhennull@*/ is_multipart(/*@null@*//*@out@*/ char ** boundary) {
	char * b;
	/*@dependent@*/ char * bb;
	int l=0;
	if (type_header_body==NULL||
			!case_insensitively_heads("multipart/",type_header_body)) {
		if (boundary!=NULL) *boundary=NULL;
		return false;
	}
	b=type_header_body+10;
	for (;;) {
		while (*b!='\0'&&*b!=';') ++b;
		if (*b=='\0') {
			warning("no boundary given");
			if (boundary!=NULL) *boundary=NULL;
			return false;
		}
		++b;
		if (case_insensitively_heads("boundary=",b)) break;
	}
	b+=9;
	if (*b=='"') {
		for (bb=b+1;*bb!='\0'&&*bb!='"';++bb) {
			if (*bb=='\\') {
				++bb;
				if (*bb=='\0') break;
			}
			++l;
		}
		if (*bb=='\0') {
			warning("error in boundary syntax");
			if (boundary!=NULL) *boundary=NULL;
			return false;
		}
	} else {
		// MIME tokens can include '.'
		for (bb=b;*bb!='\0'&&(!delimiting(*bb,true)||*bb=='.');++bb) ++l;
	}
	/*@-mustdefine@*/
	if (boundary==NULL) return true;
	/*@=mustdefine@*/
	// Room for leading and trailing '--', and terminator
	*boundary=alloc_or_exit(sizeof(char)*(l+5));
	bb=*boundary;
	*bb++='-';
	*bb++='-';
	if (*b=='"') {
		++b;
		while (*b!='\0'&&*b!='"') {
			if (*b=='\\') ++b;
			*bb++=*b++;
		}
	} else {
		// MIME tokens can include '.'
		while (*b!='\0'&&(!delimiting(*b,true)||*b=='.')) *bb++=*b++;
	}
	*bb='\0';
	return true;
}
static inline bool is_signed() {
	return type_header_body!=NULL&&
			case_insensitively_heads("multipart/signed",type_header_body);
}
static inline bool is_alternative() {
	return type_header_body!=NULL&&
			case_insensitively_heads("multipart/alternative",type_header_body);
}
static inline bool is_mixed() {
	return type_header_body!=NULL&&
			case_insensitively_heads("multipart/mixed",type_header_body);
}
static inline bool is_html() {
	return type_header_body!=NULL&&
			case_insensitively_heads("text/html",type_header_body);
}
static inline bool is_plain() {
	return type_header_body==NULL||
			case_insensitively_heads("text/plain",type_header_body);
}
static inline bool is_signature() {
	return type_header_body!=NULL&&(
			case_insensitively_heads("application/x-pkcs7-signature",
			type_header_body)||
			case_insensitively_heads("application/pgp-signature",
			type_header_body));
}
static inline bool is_attachment() {
	return disposition_header_body!=NULL&&
			case_insensitively_heads("attachment",disposition_header_body);
}
static inline void set_decoding_type() {
	if (transfer_header_body==NULL) {
		decoding=unencoded;
		return;
	}
	if (case_insensitively_heads("quoted-printable",transfer_header_body)) {
		decoding=quoted_printable;
		return;
	}
	if (case_insensitively_heads("base64",transfer_header_body)) {
		decoding=base64;
		return;
	}
	decoding=unencoded;
	if (case_insensitively_heads("7bit",transfer_header_body)) return;
	if (case_insensitively_heads("8bit",transfer_header_body)) return;
	if (case_insensitively_heads("binary",transfer_header_body)) return;
	warning("unrecognised transfer encoding");
}
static inline void change_to_mixed() {
	char * boundary=NULL;
	char * header;
	char * b, * h;
	int l=0;
	if (!is_multipart(&boundary)) {
		warning("internal error: changing non-multipart to mixed");
		return;
	}
	boundary+=2;
	// The special cases should never happen, as '\', '"', '\r' and '\n'
	// aren't allowed in boundaries...but...just in case...
	for (b=boundary;*b!='\0';++b) {
		if (*b=='\r'||*b=='\n') {
			warning("boundary with newline");
			return;
		}
		if (*b=='"'||*b=='\\') ++l;
		++l;
	}
	header=alloc_or_exit(sizeof(char)*(50+l)); // Play it safe
	strcpy(header,"Content-Type: multipart/mixed;\r\n boundary=\"");
	for (h=header+43,b=boundary;*b!='\0';++h,++b) {
		if (*b=='"'||*b=='\\') { *h='\\'; ++h; }
		*h=*b;
	}
	*h++='"'; *h++='\r'; *h++='\n'; *h++='\0';
	free(boundary-2);
	/*@-nullpass@*/
	free(saved_mime_headers[type_header_index]);
	/*@=nullpass@*/
	saved_mime_headers[type_header_index]=header;
	resort_to_exit(replacements_count==replacements_max,
			"internal error: too many replacements",EX_SOFTWARE);
	replacement_starts[replacements_count]=
			mime_header_starts[type_header_index];
	replacement_ends[replacements_count]=
			mime_header_ends[type_header_index];
	replacement_strings[replacements_count]=
			saved_mime_headers[type_header_index];
	++replacements_count;
}
static inline void generate_boundary(/*@out@*/ char ** boundary) {
	int r;
	*boundary=alloc_or_exit(sizeof(char)*42); // Life, the universe and everything
	strcpy(*boundary,"--=_foot_filter_boundary_0123456789012_=");
	for (r=25;r<38;++r) {
		(*boundary)[r]=(char)((int)'A'+(random()%16));
	}
}
static inline void output_mime_mixed_headers(const char * boundary) {
	resort_to_errno(
			putstr("MIME-Version: 1.0\r\n")==EOF,
			"error echoing string",EX_IOERR);
	resort_to_errno(
			putstr("Content-Type: multipart/mixed;\r\n boundary=\"")==EOF,
			"error echoing string",EX_IOERR);
	resort_to_errno(putstr(boundary+2)==EOF,
			"error echoing string",EX_IOERR);
	// I put an extra CRLF just in case some mail reader expects the
	// initial boundary to include one that is separate from the one that
	// ends the headers.
	resort_to_errno(
			putstr("\"\r\n\r\n\r\n")==EOF,
			"error echoing string",EX_IOERR);
}
static inline void output_prolog() {
	// Deliberately empty; we don't need any prolog
}
static void output_mime_footer(const char * boundary) {
	output_boundary(boundary);
	if (html_mime_footer) {
		resort_to_errno(
				putstr("Content-Type: text/html; charset=UTF-8\r\n")==EOF,
				"error echoing string",EX_IOERR);
	} else {
		resort_to_errno(
				putstr("Content-Type: text/plain; charset=UTF-8\r\n")==EOF,
				"error echoing string",EX_IOERR);
	}
	resort_to_errno(
			putstr("Content-Transfer-Encoding: quoted-printable\r\n")==EOF,
			"error echoing string",EX_IOERR);
	encoding=quoted_printable;
	resort_to_errno(putstr("\r\n")==EOF,
			"error echoing string",EX_IOERR);
	/*@-nullpass@*/
	encode_footer(mime_footer);
	/*@=nullpass@*/
	encodechar((int)'\r');
	encodechar((int)'\n');
	finish_encoding();
	// This is logically part of the boundary that is about to come
	resort_to_errno(putstr("\r\n")==EOF,
			"error echoing string",EX_IOERR);
}
static inline void output_boundary(const char * boundary) {
	resort_to_errno(putstr(boundary)==EOF,"error echoing string",EX_IOERR);
	resort_to_errno(putstr("\r\n")==EOF,"error echoing string",EX_IOERR);
}
static inline void output_final_boundary(const char * boundary) {
	resort_to_errno(putstr(boundary)==EOF,"error echoing string",EX_IOERR);
	resort_to_errno(putstr("--\r\n")==EOF,"error echoing string",EX_IOERR);
}
static bool at_final_boundary(char * boundary) {
	int l;
	// If no lookahead, we probably hit EOF, so we primarily just need to get
	// out of loops and exit
	if (buffer_filled-buffer_read==0) {
		warning("probably unexpected end of input");
		return true;
	}
	l=(int)strlen(boundary);
	boundary[l]='-';
	boundary[l+1]='-';
	boundary[l+2]='\0';
	/*@-temptrans@*/
	callback_compare=boundary;
	/*@=temptrans@*/
	(void)look(comparing_head,buffer_read,false);
	callback_compare=NULL;
	boundary[l]='\0';
	return callback_bool;
}

/* tag: footer_functions */

static inline void process_text_section(bool add_footer,
		/*@null@*/ const char * footer,
		const char * prefix, const char * suffix, const char * replacement,
		const_null_string * tails, const_null_string * padding,
		const_null_string * guts, const_null_string * pairs,
		char * boundary) {
	int prefix_pos;
	int later_prefix_pos;
	int suffix_pos;
	bool removed_footers=false;
	bool removed_newline=false;
	bool boundary_newline=false;
	int prefixl=(int)strlen(prefix);
	int suffixl=(int)strlen(suffix);
	resort_to_exit(buffer_filled>0,
			"internal error: unexpected data in buffer",EX_SOFTWARE);
	set_decoding_type();
	encoding=decoding;
	decode_and_read_to_boundary_encoding_when_full(boundary);
	if (smart_footer&&footer!=NULL) {
	// alternatively
	// if (smart_footer) {
		for (;;) {
			prefix_pos=pos_of(prefix,0,buffer_read);
			if (prefix_pos==EOF) break;
			suffix_pos=pos_of(suffix,prefix_pos,buffer_read);
			if (suffix_pos==EOF) break;
			for (;;) {
				later_prefix_pos=
						pos_of(prefix,prefix_pos+prefixl,suffix_pos-prefixl);
				if (later_prefix_pos!=EOF) prefix_pos=later_prefix_pos;
				else break;
			}
			suffix_pos+=suffixl;
			pad(padding,guts,pairs,&prefix_pos,&suffix_pos);
			replacement_starts[replacements_count]=prefix_pos;
			replacement_ends[replacements_count]=suffix_pos;
			// We may not want the last replacement so replace
			// with nothing first
			replacement_strings[replacements_count]=NULL;
			++replacements_count;
			// We want the last replacement; encode it now before
			// doing any more encoding
			if (removed_footers) encode_string(replacement);
			encode_replacements();
			removed_footers=true;
		}
	}
	if (*boundary!='\0'&&(decoding==quoted_printable||decoding==unencoded)) {
		// If we're not using base64 encoding, and we're in multipart, there
		// will be a final CRLF that is part of the input but logically part of
		// the boundary, not the text. Removing the footer may have already
		// removed it, so we need to check if it's here or not.
		if (buffer_read>1) {
			callback_compare="\r\n";
			(void)look(comparing_head,buffer_read-2,false);
			callback_compare=NULL;
			if (callback_bool) boundary_newline=true;
		}
	}
	if (add_footer&&footer!=NULL) {
		// This will skip past the boundary newline
		mark_tail(tails);
		if (removed_footers&&buffer_mark==0) {
			// The last replacement coincides with where the footer
			// is going to go; don't use the replacement text.
			removed_footers=false;
		}
	}
	if (removed_footers) encode_string(replacement);
	if (add_footer&&footer!=NULL) {
		if (buffer_mark<buffer_read-2||
				(buffer_mark==buffer_read-2&&!boundary_newline)) {
			// If we tailed back past a newline (that wasn't the boundary
			// newline which isn't really there) we don't bother appending
			// a new one to the footer
			callback_compare="\r\n";
			(void)look(comparing_head,buffer_mark,false);
			if (callback_bool) removed_newline=true;
		}
		encode_to_mark();
		encodechar((int)'\r');
		encodechar((int)'\n');
		encode_footer(footer);
		if (!removed_newline) {
			encodechar((int)'\r');
			encodechar((int)'\n');
		}
	}
	if (boundary_newline) {
		// Actually remove the boundary newline now
		if (replacements_count==replacements_max) {
			warning("internal error: too many replacements");
		} else {
			replacement_starts[replacements_count]=buffer_read-2;
			replacement_ends[replacements_count]=buffer_read;
			replacement_strings[replacements_count]=NULL;
			++replacements_count;
		}
	}
	encode_lookbehind();
	finish_encoding();
	if (*boundary!='\0') {
		// This is logically part of the boundary
		resort_to_errno(putstr("\r\n")==EOF,"error echoing string",EX_IOERR);
	}
}
static inline void pad(const_null_string * padding,
		const_null_string * guts, const_null_string * pairs,
		int * prefix_pos, int * suffix_pos) {
	const char ** run;
	const char ** test;
	const char ** opening;
	const char ** closing;
	int saved_prefix_pos;
	int definite_prefix_pos;
	int definite_suffix_pos;
	bool pair_succeeded;
	bool pad_succeeded;
	// Could generate lengths at init time for speed
	int l;
	int ll;
	// If we succeed for one thing, we try it again straight away,
	// as a number of types of padding are likely to occur in multiples.
	// Then we keep trying the whole lot until nothing is left to do.
	do {
		// Try each piece of padding (or guts)
		definite_prefix_pos=EOF;
		definite_suffix_pos=EOF;
		run=padding;
		do {
			do {
				pad_succeeded=false;
				test=run;
				while (*test!=NULL) {
					l=(int)strlen(*test);
					for (;;) {
						// Check for padding at tail
						if (buffer_filled-*suffix_pos<l) break;
						callback_compare=*test;
						(void)look(comparing_head,*suffix_pos,false);
						if (!callback_bool) break;
						*suffix_pos+=l;
						pad_succeeded=true;
					}
					for (;;) {
						// Check for padding at head
						if (*prefix_pos-l<0) break;
						callback_compare=*test;
						(void)look(comparing_head,*prefix_pos-l,false);
						if (!callback_bool) break;
						*prefix_pos-=l;
						pad_succeeded=true;
					}
					++test;
				}
			} while (pad_succeeded);
			if (definite_prefix_pos==EOF) {
				// Do a second run to deal with guts in a more tentative way
				definite_prefix_pos=*prefix_pos;
				definite_suffix_pos=*suffix_pos;
				run=guts;
			} else break;
		} while (true);
		// Try each pair
		pair_succeeded=false;
		closing=pairs;
		while (*closing!=NULL) {
			l=(int)strlen(*closing);
			opening=closing+1;
			// This loop is just to be broken out of; we don't try
			// pairs twice when they succeed as they aren't too likely
			// to nest.
			do {
				// Check for closing part
				if (buffer_filled-*suffix_pos<l) break;
				callback_compare=*closing;
				(void)look(comparing_head,*suffix_pos,false);
				if (!callback_bool) break;
				// Check for end of opening part
				ll=(int)strlen(*opening);
				if (*prefix_pos-ll<0) break;
				callback_compare=*opening;
				(void)look(comparing_head,*prefix_pos-ll,false);
				if (!callback_bool) break;
				saved_prefix_pos=*prefix_pos;
				// Try each variant
				for (++opening;*opening!=NULL;
						*prefix_pos=saved_prefix_pos,++opening) {
					// Search back for first character
					ll=(int)strlen(*opening);
					if (*prefix_pos-ll<0) continue;
					*prefix_pos-=ll;
					callback_match=(int)**opening;
					if (lookback(until_match,*prefix_pos,true)) {
						// Check if this is the variant
						callback_compare=*opening;
						(void)look(comparing_head,*prefix_pos,false);
						if (!callback_bool) continue;
						*suffix_pos+=l;
						pair_succeeded=true;
						break;
					}
				}
			} while (false);
			// Skip all the variants if not skipped already
			while (*opening!=NULL) ++opening;
			closing=opening+1;
		}
		if (!pair_succeeded) {
			// If the pair didn't succeed, we don't remove the guts
			*prefix_pos=definite_prefix_pos;
			*suffix_pos=definite_suffix_pos;
		}
	} while (pair_succeeded);
}
static inline void mark_tail(const_null_string * padding) {
	// This is basically pad() with deletions, but adding the two lines
	// at the start to initially place the mark. Comments at pad() apply.
	bool something_changed;
	const char ** test;
	int l;
	buffer_mark=buffer_read;
	buffer_marked=true;
	do {
		something_changed=false;
		// Try each piece of padding
		test=padding;
		while (*test!=NULL) {
			l=(int)strlen(*test);
			for (;;) {
				// Check for padding at head
				if (buffer_mark-l<0) break;
				callback_compare=*test;
				(void)look(comparing_head,buffer_mark-l,false);
				if (!callback_bool) break;
				buffer_mark-=l;
				something_changed=true;
			}
			++test;
		}
	} while (something_changed);
}
static inline void encode_footer(const char * footer) {
	while (*footer!='\0') {
		if (*footer=='\n') encodechar((int)'\r');
		encodechar((int)(unsigned int)*footer);
		footer++;
	}
}

/* tag: buffer_functions */

// The first few of these handle replacements, the rest not.
static inline void read_buffer() {
	buffer_read=buffer_filled;
}
static inline void echo_buffer() {
	read_buffer();
	echo_lookbehind();
}
static inline void skip_buffer() {
	if (buffer_filled>0) (void)empty(until_no_buffer);
}
static inline void echo_lookbehind() {
	make_replacements(echoing_one_char,echoing_until_start_marked);
	if (buffer_read>0) (void)empty(echoing_until_no_lookbehind);
}
static inline void encode_lookbehind() {
	make_replacements(encoding_one_char,encoding_until_start_marked);
	if (buffer_read>0) (void)empty(encoding_until_no_lookbehind);
}
static inline void encode_replacements() {
	make_replacements(encoding_one_char,encoding_until_start_marked);
}
static inline void make_replacements(callback_t one_char,
		callback_t start_marked) {
	int r, minr=0;
	const char * c;
	if (buffer_read==0) return;
	buffer_marked=false;
	while (replacements_count>0) {
		for (r=0;r<replacements_count;++r) {
			if (!buffer_marked||replacement_starts[r]<buffer_mark) {
				buffer_marked=true;
				minr=r;
				buffer_mark=replacement_starts[r];
			}
		}
		for (r=0;r<replacements_count;++r) {
			replacement_starts[r]-=buffer_mark;
			replacement_ends[r]-=buffer_mark;
		}
		if (buffer_mark>0) (void)empty(start_marked);
		c = replacement_strings[minr];
		if (c!=NULL) {
			while (*c!='\0') {
				buffer_char=(int)(unsigned int)*c;
				(void)(*one_char)();
				++c;
			}
		}
		buffer_marked=true;
		buffer_mark=replacement_ends[minr];
		for (r=0;r<replacements_count;++r) {
			replacement_starts[r]-=buffer_mark;
			replacement_ends[r]-=buffer_mark;
		}
		if (buffer_mark>0) (void)empty(until_start_marked);
		for (r=minr;r<replacements_count-1;++r) {
			replacement_starts[r]=replacement_starts[r+1];
			replacement_ends[r]=replacement_ends[r+1];
			replacement_strings[r]=replacement_strings[r+1];
		}
		--replacements_count;
		buffer_marked=false;
	}
}
static inline void encode_to_mark() {
	if (buffer_mark>0) (void)empty(encoding_until_start_marked);
}
static inline void echo_disk_buffer() {
	if (disk_buffer_filled>0) (void)empty(echoing_until_no_disk_buffer);
}
static inline void encode_disk_buffer() {
	if (disk_buffer_filled>0) (void)empty(encoding_until_no_disk_buffer);
}
static inline void skip_disk_buffer() {
	if (disk_buffer_filled>0) (void)empty(until_no_disk_buffer);
}
static inline void read_boundary(/*@out@*/ char ** boundary) {
	int l=0;
	if (buffer_filled>buffer_read) {
		callback_bool=false;
		callback_int=0;
		resort_to_exit(!look(counting_until_eol,buffer_read,false),
				"internal error: missing eol at section boundary",EX_SOFTWARE);
		l=callback_int-2; // remove the CRLF, but keep the leading '--'
	}
	// Leave room to append a trailing '--' for testing final boundary;
	// the CRLF will be written in this space by saving_until_eol too.
	*boundary = alloc_or_exit(sizeof(char)*(l+3));
	if (buffer_filled>buffer_read) {
		callback_bool=false;
		callback_save=*boundary;
		(void)look(saving_until_eol,buffer_read,false);
		callback_save=NULL;
	}
	(*boundary)[l]='\0';
	if (buffer_filled>buffer_read) {
		callback_bool=false;
		(void)look(until_eol,buffer_read,true);
	}
}
static inline void echo_to_boundary(const char * boundary) {
	do {
		echo_buffer();
	} while (!process_one_line_checking_boundary(
			echoing_n_chars,NULL,until_eol,echo,boundary));
}
static inline void skip_to_boundary(const char * boundary) {
	do {
		skip_buffer();
	} while (!process_one_line_checking_boundary(
			n_chars,NULL,until_eol,discard,boundary));
}
static inline void decode_and_read_to_boundary_encoding_when_full(
		const char * boundary) {
	do {
		read_buffer();
	} while (!process_one_line_checking_boundary(
			encoding_n_chars,decode_lookahead,
			decoding_until_eol,encode,boundary));
	finish_decoding(); // This just sets state, doesn't change data
}
static inline bool process_one_line_checking_boundary(callback_t n_chars,
		/*@null@*/ function_t process, callback_t processing,
		when_full_t when_full, const char * boundary) {
	bool stopped_by_design;
	if (feof(stdin)!=0) {
		// We're done! Call it a boundary (even if it isn't--we need to
		// get out of loops cleanly and tidy up as best we can).
		return true;
	}
	// Empty until enough space for boundary
	if (mem_buffer_size-mem_buffer_filled<80) {
		callback_int=80-(mem_buffer_size-mem_buffer_filled);
		(void)empty(n_chars);
	}
	callback_bool=false;
	stopped_by_design=fill(until_eol,stop);
	if (stopped_by_design||feof(stdin)!=0) {
		if (buffer_filled-buffer_read==0) {
			return *boundary=='\0';
		}
		callback_bool=false;
		if (*boundary!='\0') {
			// Can only be at a boundary without being at EOF if there
			// really is a boundary
			/*@-temptrans@*/
			callback_compare=boundary;
			/*@=temptrans@*/
			(void)look(comparing_head,buffer_read,false);
			callback_compare=NULL;
		}
		if (!callback_bool&&process!=NULL) (*process)();
		return callback_bool;
	} else {
		// Line is too long to be a boundary, so must be decoded
		if (process!=NULL) (*process)();
		callback_bool=false;
		(void)fill(processing,when_full);
		return false;
	}
}

// Return the position of text whose start may occur in the buffer
// anywhere between from and (just before) to. Use EOF for from to
// go from current location; use EOF for to to read indefinitely;
// EOF is returned if text is not found.
static int pos_of(const char * text,int from,int to) {
	int saved_buffer_read;
	int pos=EOF;
	if (*text=='\0') return from;
	saved_buffer_read=buffer_read;
	if (from!=EOF) buffer_read=from;
	callback_match=(int)(unsigned int)*text;
	for (;;) {
		if (to!=EOF) {
			callback_int=to-buffer_read;
			if (!look(n_chars_until_match,buffer_read,true)) break;
		} else {
			if (!look(until_match,buffer_read,true)) break;
		}
		if (!callback_bool) break;
		/*@-temptrans@*/
		callback_compare=text+1;
		/*@=temptrans@*/
		(void)look(comparing_head,buffer_read,false);
		callback_compare=NULL;
		if (callback_bool) {
			// Include the first character
			pos=buffer_read-1;
			break;
		}
	}
	buffer_read=saved_buffer_read;
	return pos;
}

// Look at characters in the buffer, starting at offset from,
// 'reading' if so indicated (and looking at that location).
// The callback is called after updating the reading pointer
// and placing the character in the buffer. The character is
// also passed by means of the buffer_char global.
// EOF is sent to the callback when we run out of data.
// There is no automatic attempt to fill the buffer.
// The callback should return a boolean indicating whether
// to continue. This function will return true if the callback
// indicated to stop (including if it so indicated on EOF), or
// false if it stopped for EOF.
// We always call the callback at least once, so don't call
// this function at all unless you definitely want to look
// at something.
static bool look(callback_t callback,int from,bool read) {
	int pos=from;
	int disk_buffer_pos;
	char * mem_buffer_pos;
	if (pos<disk_buffer_filled) {
		disk_buffer_pos=disk_buffer_start+pos;
		if (disk_buffer_sought!=disk_buffer_pos) {
			/*@-nullpass@*/
			resort_to_errno(fseek(disk_buffer,disk_buffer_pos,SEEK_SET)!=0,
					"error seeking in temporary file",EX_IOERR);
			/*@=nullpass@*/
			disk_buffer_sought=disk_buffer_pos;
		}
		while (pos<disk_buffer_filled) {
			/*@-nullpass@*/
			buffer_char=getc(disk_buffer);
			/*@=nullpass@*/
			resort_to_errno(buffer_char==EOF,
					"error reading temporary file",EX_IOERR);
			// ++disk_buffer_pos; logically this happens, but it is unnecessary
			++disk_buffer_sought;
			if (read&&pos==buffer_read) ++buffer_read;
			++pos;
			if (!(*callback)()) return true;
		}
	}
	if (pos<buffer_filled) {
		mem_buffer_pos=mem_buffer_next_empty+(pos-disk_buffer_filled);
		if (mem_buffer_pos>=mem_buffer_end) mem_buffer_pos-=mem_buffer_size;
		while (pos<buffer_filled) {
			buffer_char=(int)(unsigned int)*mem_buffer_pos;
			++mem_buffer_pos;
			if (mem_buffer_pos==mem_buffer_end) mem_buffer_pos=mem_buffer_start;
			if (read&&pos==buffer_read) ++buffer_read;
			++pos;
			if (!(*callback)()) return true;
		}
	}
	buffer_char=EOF;
	if (!(*callback)()) return true;
	return false;
}
// Does the same backwards, moving the mark if so indicated (and
// looking at that location). The callback is called before the
// mark is moved, though, and if it returns false, the mark is
// not moved, but the function returns true immediately.
// There is no call to the callback with EOF when we get to the
// start of the buffer, so the function always returns false in
// that case, and unmarks the buffer. Again, the callback is
// always called at least once.
static bool lookback(callback_t callback,int from,bool mark) {
	int pos=from;
	int disk_buffer_pos;
	char * mem_buffer_pos;
	if (pos>=disk_buffer_filled) {
		mem_buffer_pos=mem_buffer_next_empty+(pos-disk_buffer_filled);
		if (mem_buffer_pos>=mem_buffer_end) mem_buffer_pos-=mem_buffer_size;
		while (pos>=disk_buffer_filled) {
			buffer_char=(int)(unsigned int)*mem_buffer_pos;
			if (!(*callback)()) return true;
			--mem_buffer_pos;
			if (mem_buffer_pos==mem_buffer_start-1)
					mem_buffer_pos=mem_buffer_end-1;
			if (mark&&pos==buffer_mark) --buffer_mark;
			--pos;
		}
	}
	if (pos>=0&&disk_buffer_filled>0) {
		disk_buffer_pos=disk_buffer_start+pos;
		// Reading backwards in the disk buffer is potentially very nasty;
		// hopefully it never actually happens
		while (pos>=0) {
			/*@-nullpass@*/
			resort_to_errno(fseek(disk_buffer,disk_buffer_pos,SEEK_SET)!=0,
					"error seeking in temporary file",EX_IOERR);
			disk_buffer_sought=disk_buffer_pos;
			buffer_char=getc(disk_buffer);
			/*@=nullpass@*/
			resort_to_errno(buffer_char==EOF,
					"error reading temporary file",EX_IOERR);
			++disk_buffer_sought;
			if (!(*callback)()) return true;
			--disk_buffer_pos;
			if (mark&&pos==buffer_mark) --buffer_mark;
			--pos;
		}
	}
	if (mark&&buffer_mark==-1) {
		buffer_mark=0;
		buffer_marked=false;
	}
	// We don't call the callback on EOF when going backwards
	// buffer_char=EOF;
	// (void)(*callback)();
	return false;
}
// Remove characters from the (beginning of the) buffer. The same
// general principles as for look() apply. The callback is called
// after the character is removed and all accounting has been done, so
// perhaps the only place you can reliably find the character is in
// the buffer_char global. Again the callback gets an EOF call if
// there's nothing more to empty, and no automatic filling is done.
// The callback and function return values are as for look() and
// again, the callback is always called at least once; this means at
// least one character is always removed from the buffer, so only call
// the function if something definitely should be removed.
static bool empty(callback_t callback) {
	if (disk_buffer_filled>0) {
		if (disk_buffer_sought!=disk_buffer_start) {
			/*@-nullpass@*/
			resort_to_errno(fseek(disk_buffer,disk_buffer_start,SEEK_SET)!=0,
					"error seeking in temporary file",EX_IOERR);
			/*@=nullpass@*/
			disk_buffer_sought=disk_buffer_start;
		}
		while (disk_buffer_filled>0) {
			/*@-nullpass@*/
			buffer_char=getc(disk_buffer);
			/*@=nullpass@*/
			resort_to_errno(buffer_char==EOF,
					"error reading temporary file",EX_IOERR);
			++disk_buffer_sought;
			++disk_buffer_start;
			--disk_buffer_filled;
			--buffer_filled;
			if (buffer_read>0) --buffer_read;
			if (buffer_marked) {
				if (buffer_mark>0) --buffer_mark;
				else buffer_marked=false;
			}
			if (!(*callback)()) return true;
		}
	}
	while (mem_buffer_filled>0) {
		buffer_char=(int)(unsigned int)*mem_buffer_next_empty;
		++mem_buffer_next_empty;
		if (mem_buffer_next_empty==mem_buffer_end) mem_buffer_next_empty=mem_buffer_start;
		--mem_buffer_filled;
		--buffer_filled;
		if (buffer_read>0) --buffer_read;
		if (buffer_marked) {
			if (buffer_mark>0) --buffer_mark;
			else buffer_marked=false;
		}
		if (!(*callback)()) return true;
	}
	buffer_char=EOF;
	if (!(*callback)()) return true;
	return false;
}
// Get more characters into the (end of the) buffer. The same
// general principles as for look() apply. The callback is called
// after the character is added and all accounting has been done,
// gets the character via buffer_char, including an EOF when no more
// input is available (EOF on stdin). It should return whether to get
// more characters, and this function will return whether its exit was
// requested by the callback or not (the callback may signal EOF is
// an appropriate place to stop and we still return true).
// When the buffer is full there are a number of automatic options
// echo old the data to stdout or call encodechar for it one character
// at a time; shunt a block off to disk, keeping mem_buffer_keep in
// memory, discard it a character at a time, stop (and return false;
// no EOF call is made), or fail (exit). Here 'full' is defined as
// less than mem_buffer_margin of space after adding the most recent
// character, so there is always a bit of space for callbacks to do
// input transformations. Again, at least one character is always
// added (if possible), and thus consumed from stdin, so only call this
// if you really want to do that.
static bool fill(callback_t callback, when_full_t when_full) {
	if (feof(stdin)!=0) {
		buffer_char=EOF;
		if (!(*callback)()) return true;
		return false;
	}
	for (;;) {
		/*@-infloops@*/
		while (mem_buffer_filled>=mem_buffer_size-mem_buffer_margin) {
			switch (when_full) {
			case echo:
				if (disk_buffer_filled>0) echo_disk_buffer();
				(void)empty(echoing_one_char);
				break;
			case encode:
				if (disk_buffer_filled>0) encode_disk_buffer();
				(void)empty(encoding_one_char);
				break;
			case discard:
				if (disk_buffer_filled>0) skip_disk_buffer();
				(void)empty(one_char);
				break;
			case shunt:
				shunt_to_disk(mem_buffer_filled-mem_buffer_keep);
				break;
			case stop:
				return false;
			case fail: default:
				resort_to_exit(true,"buffer full",EX_SOFTWARE);
			}
		}
		/*@=infloops@*/
		buffer_char=get();
		if (buffer_char==EOF) {
			resort_to_errno(ferror(stdin)!=0,"error reading input",EX_IOERR);
			if (!(*callback)()) return true;
			return false;
		}
		*mem_buffer_next_fill=(char)buffer_char;
		++mem_buffer_next_fill;
		if (mem_buffer_next_fill==mem_buffer_end) mem_buffer_next_fill=mem_buffer_start;
		++mem_buffer_filled;
		++buffer_filled;
		if (!(*callback)()) return true;
	}
}

static inline void create_disk_buffer() {
	int fildes;
	fildes=mkstemp(disk_buffer_template);
	resort_to_errno(fildes==-1,
			"cannot create temporary file",EX_CANTCREAT);
	disk_buffer=fdopen(fildes,"rw");
	resort_to_errno(disk_buffer==NULL,
			"cannot create temporary stream",EX_CANTCREAT);
}
static void remove_disk_buffer() {
	if (disk_buffer!=NULL) {
		resort_to_warning(fclose(disk_buffer)!=0,
				"error closing temporary file");
		disk_buffer=NULL;
		resort_to_warning(unlink(disk_buffer_template)!=0,
				"error removing temporary file");
	}
}
static inline void shunt_to_disk(int n) {
	if (disk_buffer==NULL) create_disk_buffer();
	if (disk_buffer_sought!=disk_buffer_start+disk_buffer_filled) {
		disk_buffer_sought=disk_buffer_start+disk_buffer_filled;
		/*@-nullpass@*/
		resort_to_errno(fseek(disk_buffer,
				disk_buffer_start+disk_buffer_filled,SEEK_SET)!=0,
				"cannot seek to end of temporary file",EX_IOERR);
		/*@=nullpass@*/
	}
	while (n>0) {
		resort_to_exit(mem_buffer_filled==0,
				"internal error: shunting too much to disk",EX_SOFTWARE);
		/*@-nullpass@*/
		resort_to_errno(putc(*mem_buffer_next_empty,disk_buffer)==EOF,
				"error writing to temporary file",EX_IOERR);
		/*@=nullpass@*/
		++disk_buffer_sought;
		++disk_buffer_filled;
		++mem_buffer_next_empty;
		if (mem_buffer_next_empty==mem_buffer_end) mem_buffer_next_empty=mem_buffer_start;
		--mem_buffer_filled;
		--n;
	}
}

/* tag: callback_functions */

static bool one_char() {
	callback_int=buffer_char;
	return false;
}
static bool echoing_one_char() {
	if (buffer_char!=EOF) {
		resort_to_errno(put(buffer_char)==EOF,"error echoing",EX_IOERR);
	}
	callback_int=buffer_char;
	return false;
}
static bool encoding_one_char() {
	if (buffer_char!=EOF) encodechar(buffer_char);
	callback_int=buffer_char;
	return false;
}
// Set up callback_int before using this.
static bool n_chars() {
	return --callback_int>0;
}
// Set up callback_int before using this.
static bool echoing_n_chars() {
	if (buffer_char!=EOF) {
		resort_to_errno(put(buffer_char)==EOF,"error echoing",EX_IOERR);
	}
	return --callback_int>0;
}
// Set up callback_int before using this.
static bool encoding_n_chars() {
	if (buffer_char!=EOF) encodechar(buffer_char);
	return --callback_int>0;
}
// Set up callback_int and callback_save before using this.
static bool saving_n_chars() {
	if (buffer_char!=EOF) *callback_save++=(char)buffer_char;
	// We don't actually need this, though it's a good idea, really!
	// *callback_save='\0';
	return --callback_int>0;
}
// Set up callback_int and callback_match before using this.
static bool n_chars_until_match() {
	callback_bool=buffer_char==callback_match;
	return --callback_int>0&&buffer_char!=callback_match;
}
// Do callback_bool=false before using this.
static bool until_eol() {
	if (buffer_char==(int)'\n') return !callback_bool;
	callback_bool=buffer_char==(int)'\r';
	return true;
}
// Do callback_bool=false before using this.
/*static bool echoing_until_eol() {
	if (buffer_char!=EOF) {
		resort_to_errno(put(buffer_char)==EOF,"error echoing",EX_IOERR);
	}
	if (buffer_char==(int)'\n') return !callback_bool;
	callback_bool=buffer_char==(int)'\r';
	return true;
}*/
// Do callback_bool=false, callback_int=0 before using this.
static bool counting_until_eol() {
	if (buffer_char!=EOF) ++callback_int;
	if (buffer_char==(int)'\n') return !callback_bool;
	callback_bool=buffer_char==(int)'\r';
	return true;
}
// Do callback_bool=false and set up callback_save before using this.
static bool saving_until_eol() {
	if (buffer_char!=EOF) *callback_save++=(char)buffer_char;
	// We don't actually need this, though it's a good idea, really!
	// *callback_save='\0';
	if (buffer_char==(int)'\n') return !callback_bool;
	callback_bool=buffer_char==(int)'\r';
	return true;
}
// Do callback_bool=false before using this.
static bool decoding_until_eol() {
	// We decode as we fill and work directly in the buffer to make
	// the transformation. We are guaranteed enough space to do this by
	// mem_buffer_margin.
	decode_t decoded;
	decoded=decodechar(buffer_char);
	// We always remove the latest undecoded character from the
	// buffer.
	++decoded.r;
	if (decoded.r>mem_buffer_filled) {
		// This will only happen for quoted-printable decoding
		// whitespace stripping, and we can just live with it
		// if we can't get rid of it all; with sensible constants
		// something really is disobeying MIME and probably SMTP
		// about line length anyway if this happens.
		warning("unable to strip all whitespace; not enough in memory");
		decoded.r=mem_buffer_filled;
	}
	if (buffer_filled-decoded.r<buffer_read) {
		// We should always be working in lookahead when this happens,
		// but better safe than sorry!
		warning("unable to strip all whitespace; not enough unread");
		decoded.r=buffer_filled-buffer_read;
	}
	if (buffer_marked&&buffer_filled-decoded.r<buffer_mark) {
		// Marks should be in lookbehind too, but again,
		// better safe than sorry! We unmark. Filling often
		// does that anyway.
		buffer_marked=false;
		buffer_mark=0;
	}
	mem_buffer_next_fill-=decoded.r;
	if (mem_buffer_next_fill<mem_buffer_start)
			mem_buffer_next_fill+=mem_buffer_size;
	mem_buffer_filled-=decoded.r;
	buffer_filled-=decoded.r;
	if (decoded.c1!=EOF) {
		*mem_buffer_next_fill=(char)decoded.c1;
		++mem_buffer_next_fill;
		if (mem_buffer_next_fill==mem_buffer_end) mem_buffer_next_fill=mem_buffer_start;
		++mem_buffer_filled;
		++buffer_filled;
		if (decoded.c2!=EOF) {
			*mem_buffer_next_fill=(char)decoded.c2;
			++mem_buffer_next_fill;
			if (mem_buffer_next_fill==mem_buffer_end) mem_buffer_next_fill=mem_buffer_start;
			++mem_buffer_filled;
			++buffer_filled;
			if (decoded.c3!=EOF) {
				*mem_buffer_next_fill=(char)decoded.c3;
				++mem_buffer_next_fill;
				if (mem_buffer_next_fill==mem_buffer_end) mem_buffer_next_fill=mem_buffer_start;
				++mem_buffer_filled;
				++buffer_filled;
			}
		}
	}
	// We check for eol using the input stream, not the decoded
	// stream, as it's all about the upcoming boundary
	if (buffer_char==(int)'\n') return !callback_bool;
	callback_bool=buffer_char==(int)'\r';
	return true;
}
/*static bool until_no_lookbehind() {
	return buffer_read!=0;
}*/
static bool echoing_until_no_lookbehind() {
	if (buffer_char!=EOF) {
		resort_to_errno(put(buffer_char)==EOF,"error echoing",EX_IOERR);
	}
	return buffer_read!=0;
}
static bool encoding_until_no_lookbehind() {
	if (buffer_char!=EOF) encodechar(buffer_char);
	return buffer_read!=0;
}
static bool until_no_disk_buffer() {
	return disk_buffer_filled!=0;
}
static bool echoing_until_no_disk_buffer() {
	if (buffer_char!=EOF) {
		resort_to_errno(put(buffer_char)==EOF,"error echoing",EX_IOERR);
	}
	return disk_buffer_filled!=0;
}
static bool encoding_until_no_disk_buffer() {
	if (buffer_char!=EOF) encodechar(buffer_char);
	return disk_buffer_filled!=0;
}
static bool until_no_buffer() {
	return buffer_filled!=0;
}
/*static bool echoing_until_no_buffer() {
	if (buffer_char!=EOF) {
		resort_to_errno(put(buffer_char)==EOF,"error echoing",EX_IOERR);
	}
	return buffer_filled!=0;
}*/
static bool until_start_marked() {
	return !(buffer_marked?buffer_mark==0:buffer_read==0);
}
static bool echoing_until_start_marked() {
	if (buffer_char!=EOF) {
		resort_to_errno(put(buffer_char)==EOF,"error echoing",EX_IOERR);
	}
	return !(buffer_marked?buffer_mark==0:buffer_read==0);
}
static bool encoding_until_start_marked() {
	if (buffer_char!=EOF) encodechar(buffer_char);
	return !(buffer_marked?buffer_mark==0:buffer_read==0);
}
// Set up callback_match before using this.
static bool until_match() {
	return buffer_char!=callback_match;
}
// Set up callback_compare before using this.
static bool comparing_head() {
	/*@-nullderef@*/
	if (buffer_char!=(int)(unsigned int)*callback_compare) {
		callback_bool=false;
		return false;
	}
	/*@-modobserver@*/
	++callback_compare;
	/*@=modobserver@*/
	if (*callback_compare=='\0') {
		callback_bool=true;
		return false;
	}
	return true;
	/*@=nullderef@*/
}
// Set up callback_compare before using this.
static bool case_insensitively_comparing_head() {
	/*@-nullderef@*/
	int c1=(int)(unsigned int)*callback_compare;
	int c2=buffer_char;
	if (c1!=c2&&
			(c1<(int)'A'||c1>(int)'Z'||c2!=c1-(int)'A'+(int)'a')&&
			(c2<(int)'A'||c2>(int)'Z'||c1!=c2-(int)'A'+(int)'a')) {
		callback_bool=false;
		return false;
	}
	/*@-modobserver@*/
	++callback_compare;
	/*@=modobserver@*/
	if (*callback_compare=='\0') {
		callback_bool=true;
		return false;
	}
	return true;
	/*@=nullderef@*/
}

/* tag: encoding_functions */

static inline void encode_string(const char * s) {
	while (*s!='\0') {
		encodechar((int)(unsigned int)*s);
		s++;
	}
}
static void encodechar(int c) {
	if (encoding==unencoded) {
		if (c!=EOF) resort_to_errno(put(c)==EOF,"error encoding",EX_IOERR);
		return;
	} else if (encoding==quoted_printable) {
		if (encoding_echoed>=68) {
			// We need a soft line break, or are close enough to needing
			// one (76 chars max; unclear whether that counts the CRLF; and
			// we may output two 3 character sequences which we don't want
			// to follow with an unescaped CRLF). This scheme will probably
			// make mail look a bit awful, but that's fairly standard anyway,
			// and it shouldn't degrade.
			resort_to_errno(putstr("=\r\n")==EOF,
					"error encoding string",EX_IOERR);
			encoding_echoed=0;
		}
		if (encoding_filled==1) {
			// Whatever happens, we'll deal with this now
			encoding_filled=0;
			if (encoding_buffer[0]=='\r') {
				if (c==(int)'\n') {
					// Output them as is and we're done for now
					resort_to_errno(putstr("\r\n")==EOF,
							"error encoding string",EX_IOERR);
					encoding_echoed=0;
					return;
				} else {
					// Must encode the bare CR and continue as normal
					resort_to_errno(put((int)'=')==EOF,"error encoding",EX_IOERR);
					encode_hex_byte((unsigned int)'\r');
					encoding_echoed+=3;
				}
			} else {
				// encoding_buffer[0] must be whitespace
				if (c==EOF||c==(int)'\r') {
					// Must encode it
					resort_to_errno(put((int)'=')==EOF,"error encoding",EX_IOERR);
					encode_hex_byte((unsigned int)encoding_buffer[0]);
					encoding_echoed+=3;
				} else {
					// It is fine to output it now as something else is coming
					resort_to_errno(put(
							(int)(unsigned int)encoding_buffer[0])==EOF,
							"error encoding",EX_IOERR);
					encoding_echoed+=1;
				}
			}
		}
		if ((c>=33&&c<=60)||(c>=62&&c<=126)) {
			resort_to_errno(put(c)==EOF,"error encoding",EX_IOERR);
			++encoding_echoed;
		} else if (c==(int)' '||c==(int)'\t') {
			if (encoding_echoed>=55) {
				// My concession to readability; since it's likely to be
				// a big mess with a 68 character width, we might as well
				// break a bit earlier on a nice word boundary. And it'll
				// in fact look better if we break with roughly equal size
				// lines, assuming they come in at close to 76 characters
				// wide, so we might as well make a nice skinny column.
				// rather than a ragged one that uses the same amount of
				// space. Compromising between the two, then, as some
				// formats, like HTML, don't have many hard line breaks
				// anyway, is what we get.
				resort_to_errno(put(c)==EOF,"error encoding",EX_IOERR);
				resort_to_errno(putstr("=\r\n")==EOF,
						"error encoding string",EX_IOERR);
				encoding_echoed=0;
			} else {
				// Store it; we may need to encode it if it's at end of line
				encoding_filled=1;
				encoding_buffer[0]=(char)c;
			}
		} else if (c==(int)'\r') {
			// Store it; '\n' may be coming up
			encoding_filled=1;
			encoding_buffer[0]='\r';
		} else if (c==EOF) {
			// No buffer, and we're done! Reset for another run.
			encoding_echoed=0;
		} else {
			// Anything else must be encoded as a sequence.
			resort_to_errno(put((int)'=')==EOF,"error encoding",EX_IOERR);
			encode_hex_byte((unsigned int)c);
			encoding_echoed+=3;
		}
	} else if (encoding==base64) {
		if (c==EOF) {
			// Reset for next run; we won't need it here
			encoding_echoed=0;
			if (encoding_filled==0) return;
			encoding_buffer[encoding_filled]='\0';
		} else {
			encoding_buffer[encoding_filled++]=(char)c;
		}
		if (encoding_filled==3||c==EOF) {
			encode_64((((unsigned int)encoding_buffer[0]>>2)&0x3f));
			encode_64((((unsigned int)encoding_buffer[0]&0x03)<<4)|
					(((unsigned int)encoding_buffer[1]>>4)&0x0f));
			if (encoding_filled==1) {
				resort_to_errno(put((int)'=')==EOF,"error encoding",EX_IOERR);
				resort_to_errno(put((int)'=')==EOF,"error encoding",EX_IOERR);
				// Reset for next run
				encoding_filled=0;
				return;
			}
			encode_64((((unsigned int)encoding_buffer[1]&0x0f)<<2)|
					(((unsigned int)encoding_buffer[2]>>6)&0x03));
			if (encoding_filled==2) {
				resort_to_errno(put((int)'=')==EOF,"error encoding",EX_IOERR);
				// Reset for next run
				encoding_filled=0;
				return;
			}
			encode_64((((unsigned int)encoding_buffer[2]&0x3f)));
			encoding_echoed+=4;
			if (encoding_echoed>=72) {
				resort_to_errno(putstr("\r\n")==EOF,
						"error encoding string",EX_IOERR);
				encoding_echoed=0;
			}
			encoding_filled=0;
		}
	} else {
		resort_to_exit(true,"internal error: unknown encoding",EX_SOFTWARE);
	}
}
static inline void finish_encoding() {
	encodechar(EOF);
}
// The function takes an input character c and returns up to four output
// characters (a character will be EOF to indicate no further characters
// to store; note that this doesn't mean there will be no more ever; only
// if EOF is returned when EOF was input does it meant this), and a number
// of characters to remove before adding the aforementioned characters.
static decode_t decodechar(int c) {
	int h;
	unsigned int b1, b2, b3, b4;
	decode_t o;
	o.r=0; o.c1=EOF; o.c2=EOF; o.c3=EOF;
	if (decoding==unencoded) {
		o.c1=c;
		return o;
	} else if (decoding==quoted_printable) {
		// decoding_buffer may hold '=' and maybe a hex digit or a CR.
		if (decoding_filled==2) {
			// Whatever happens, it's all settled now.
			decoding_filled=0;
			if (decoding_buffer[1]=='\r') {
				if (c==(int)'\n') { return o; }
				// Invalid; leave as is--will be encoded later.
				o.c1=(int)'='; o.c2=(int)'\r'; o.c3=c;
				return o;
			}
			h=decode_hex(c);
			if (h==EOF) {
				// Invalid; leave as is--will be encoded later.
				o.c1=(int)'='; o.c2=(int)(unsigned int)decoding_buffer[1]; o.c3=c;
				return o;
			}
			// We have a full sequence representing a single character.
			o.c1=decode_hex((int)(unsigned int)decoding_buffer[1])*16+h;
			return o;
		} else if (decoding_filled==1) {
			if (c==(int)'\r'||decode_hex(c)!=EOF) {
				// Valid character after =
				decoding_filled=2;
				decoding_buffer[1]=(char)c;
				return o;
			}
			// Invalid; leave as is--will be encoded later.
			decoding_filled=0;
			o.c1=(int)'='; o.c2=c;
			return o;
		} else if (decoding_filled==0) {
			if (c==(int)'=') {
				// The first character can only ever be '=' so we
				// don't actually bother to store it; just say it's there.
				decoding_white=0;
				decoding_filled=1;
				return o;
			}
			// Keep track of whitespace.
			if (c==(int)' '||c==(int)'\t') ++decoding_white;
			else decoding_white=0;
			// Remove trailing whitespace.
			if (c==EOF||c==(int)'\r') { o.r=decoding_white; decoding_white=0; }
			// Otherwise we just keep it. If it's EOF, we're done.
			o.c1=c;
			return o;
		} else {
			warning("internal error: decoding buffer too full");
			return o;
		}
	} else if (decoding==base64) {
		if (c==EOF) {
			// Just in case it was corrupted, make sure we're reset
			decoding_filled=0;
			return o;
		}
		if (c==(int)'='||decode_64(c)!=EOF)
			decoding_buffer[decoding_filled++]=(char)c;
		if (decoding_filled==4) {
			// We empty it whatever happens here
			decoding_filled=0;
			b1=(unsigned int)decode_64((int)decoding_buffer[0]);
			b2=(unsigned int)decode_64((int)decoding_buffer[1]);
			o.c1=(int)(((b1&0x3f)<<2)|((b2>>4)&0x03));
			if (decoding_buffer[2]=='=') return o;
			b3=(unsigned int)decode_64((int)decoding_buffer[2]);
			o.c2=(int)(((b2&0x0f)<<4)|((b3>>2)&0x0f));
			if (decoding_buffer[3]=='=') return o;
			b4=(unsigned int)decode_64((int)decoding_buffer[3]);
			o.c3=(int)(((b3&0x03)<<6)|(b4&0x3f));
		}
		return o;
	} else {
		resort_to_exit(true,"internal error: unknown encoding",EX_SOFTWARE);
		// Never reached
		return o;
	}
}
static void decode_lookahead() {
	// Decoding will always shrink, so this is quite easy
	char * c;
	char * cc;
	decode_t decoded;
	int pos=buffer_read;
	int decpos=buffer_read;
	resort_to_exit(buffer_read<disk_buffer_filled,
			"internal error: decoding from disk",EX_SOFTWARE);
	c=mem_buffer_next_empty+pos-disk_buffer_filled;
	if (c>=mem_buffer_end) c-=mem_buffer_size;
	cc=c;
	while (pos<buffer_filled) {
		decoded=decodechar((int)(unsigned int)*c);
		if (decoded.r>0) {
			resort_to_exit(decpos-decoded.r<buffer_read,
					"internal error: removing more than was decoded",EX_SOFTWARE);
			decpos-=decoded.r;
			cc-=decoded.r;
			if (cc<mem_buffer_start) cc+=mem_buffer_size;
		}
		if (decoded.c1!=EOF) {
			*cc=(char)decoded.c1;
			++decpos; ++cc;
			if (cc==mem_buffer_end) cc=mem_buffer_start;
			if (decoded.c2!=EOF) {
				*cc=(char)decoded.c2;
				++decpos; ++cc;
				if (cc==mem_buffer_end) cc=mem_buffer_start;
				if (decoded.c3!=EOF) {
					*cc=(char)decoded.c3;
					++decpos; ++cc;
					if (cc==mem_buffer_end) cc=mem_buffer_start;
				}
			}
		}
		++pos; ++c;
		if (c==mem_buffer_end) c=mem_buffer_start;
	}
	buffer_filled+=decpos-pos;
	mem_buffer_filled+=decpos-pos;
	mem_buffer_next_fill+=decpos-pos;
	if (mem_buffer_next_fill<mem_buffer_start)
			mem_buffer_next_fill+=mem_buffer_size;
}
static inline void finish_decoding() {
	// As it will have just experienced a CRLF or an EOF, the only thing
	// this can do is reset the state if base64 was truncated.
	// We won't gain any more characters or need to remove anything.
	// It is important that this is always the case as other routines
	// rely on it.
	(void)decodechar(EOF);
}

// These are slow but easy to write! Lookup tables would be quicker.
// Still, I think it'll probably be fast enough.
static inline int decode_hex(int c) {
	if (c>=(int)'0'&&c<=(int)'9') return c-(int)'0';
	if (c>=(int)'A'&&c<=(int)'F') return c-(int)'A'+10;
	return EOF;
}
static inline int decode_64(int c) {
	if (c>=(int)'A'&&c<=(int)'Z') return c-(int)'A';
	if (c>=(int)'a'&&c<=(int)'z') return c-(int)'a'+26;
	if (c>=(int)'0'&&c<=(int)'9') return c-(int)'0'+52;
	if (c==(int)'+') return 62;
	if (c==(int)'/') return 63;
	// if (c==(int)'=') return EOF;
	return EOF;
}
static inline void encode_hex_byte(unsigned int h) {
	int h1=(int)((h>>4)&0x0f);
	int h2=(int)(h&0x0f);
	if (h1<10) resort_to_errno(put((int)'0'+h1)==EOF,"error encoding",EX_IOERR);
	else if (h1<16)
			resort_to_errno(put((int)'A'+h1-10)==EOF,"error encoding",EX_IOERR);
	else resort_to_exit(true,"internal error: byte too large",EX_SOFTWARE);
	if (h2<10) resort_to_errno(put((int)'0'+h2)==EOF,"error encoding",EX_IOERR);
	else if (h2<16)
			resort_to_errno(put((int)'A'+h2-10)==EOF,"error encoding",EX_IOERR);
	else resort_to_exit(true,"internal error: byte too large",EX_SOFTWARE);
}
static inline void encode_64(unsigned int b) {
	if (b<26)
			resort_to_errno(put((int)'A'+b)==EOF,"error encoding",EX_IOERR);
	else if (b<52)
			resort_to_errno(put((int)'a'+b-26)==EOF,"error encoding",EX_IOERR);
	else if (b<62)
			resort_to_errno(put((int)'0'+b-52)==EOF,"error encoding",EX_IOERR);
	else if (b==62)
			resort_to_errno(put((int)'+')==EOF,"error encoding",EX_IOERR);
	else if (b==63)
			resort_to_errno(put((int)'/')==EOF,"error encoding",EX_IOERR);
	else resort_to_exit(true,
			"internal error: base64 value too large",EX_SOFTWARE);
}

/* tag: error_functions */

// Syslog constants:
// level: LOG_ERR, LOG_WARNING, LOG_NOTICE, LOG_INFO, LOG_DEBUG
// facility: LOG_MAIL, LOG_DAEMON, LOG_USER, LOG_LOCALn(0-7)

static inline void * alloc_or_exit(size_t s) /*@allocates result@*/ {
	void * m;
	m=malloc(s);
	if (m==NULL) {
#ifdef USE_STDERR
		fprintf(stderr,"foot_filter: %s\n","out of memory");
#endif
#ifdef USE_SYSLOG
		syslog(LOG_ERR|LOG_MAIL,"%s\n","out of memory");
#endif
		exit(EX_OSERR);
	}
	return m;
}
static inline void /*noreturnwhentrue*/
		resort_to_exit(bool when,const char * message,int status) {
	if (when) {
#ifdef USE_STDERR
		fprintf(stderr,"foot_filter: %s\n",message);
#endif
#ifdef USE_SYSLOG
		syslog(LOG_ERR|LOG_MAIL,"%s\n",message);
#endif
		exit(status);
	}
}
static inline void /*noreturnwhentrue*/
		resort_to_errno(bool when,const char * message,int status) {
	if (when) {
#ifdef USE_STDERR
		fprintf(stderr,"foot_filter: %s (%s)\n",message,strerror(errno));
#endif
#ifdef USE_SYSLOG
		syslog(LOG_ERR|LOG_MAIL,"%s (%m)\n",message);
#endif
		exit(status);
	}
}
static inline void resort_to_warning(bool when,const char * message) {
	if (when) warning(message);
}
static inline void warning(const char * message) {
#ifdef USE_STDERR
	fprintf(stderr,"foot_filter: %s\n",message);
#endif
#ifdef USE_SYSLOG
	syslog(LOG_WARNING|LOG_MAIL,"%s\n",message);
#endif
}

/* tag: helper_functions */

// The program was written following all the specs using CRLF for newlines,
// but we get them from Postfix with LF only, so these wrapper functions
// do the translation in such a way that it can easily be disabled if desired.
static inline int get() {
	int c;
#ifdef UNIX_EOL
	static bool got_nl=false;
	if (got_nl) {
		got_nl=false;
		return 10;
	}
#endif
	c=getchar();
#ifdef UNIX_EOL
	if (c==10) {
		got_nl=true;
		return 13;
	}
#endif
	return c;
}
static inline int put(int c) {
#ifdef UNIX_EOL
	if (c==13) return c;
#endif
	return putchar(c);
}
static inline int putstr(const char * s) {
	while (*s!='\0') if (put((int)(unsigned int)*s++)==EOF) return EOF;
	return 0;
}

static inline bool case_insensitively_heads(const char * head,const char * buffer) {
	const char * s1=head;
	const char * s2=buffer;
	for (;;) {
		if (*s1=='\0') return true; /* for equality return *s2=='\0'; */
		else if (*s2=='\0') return false;
		if (*s1!=*s2&&
				(*s1<'A'||*s1>'Z'||*s2!=*s1-'A'+'a')&&
				(*s2<'A'||*s2>'Z'||*s1!=*s2-'A'+'a')) return false;
		++s1; ++s2;
	}
}

