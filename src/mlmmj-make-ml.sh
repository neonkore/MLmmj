#!/bin/sh
#
# mlmmj-make-ml.sh - henne@hennevogel.de
#

VERSION="0.1"
DEFAULTDIR="/var/spool/mlmmj"

USAGE="mlmmj-make-ml "$VERSION"
"$0" [OPTIONS]

-h	display this help text
-L	the name of the mailinglist
-s	your spool directory if not "$DEFAULTDIR"
-a	create the needed entrys in your /etc/aliases file
-z	nuffn for now"

while getopts ":hL:s:az" Option
do
case "$Option" in 
	h )
	echo -e "$USAGE"
	exit 0
	;;
	z )
	echo -n "nothing"
	exit 0
	;;
	L )
	LISTNAME="$OPTARG"
	;;
	s )
	SPOOLDIR="$OPTARG"
	;;
	a )
	A_CREATE="YES"
	;;
	* )
	echo -e "$0: invalid option\nTry $0 -h for more information."
	exit 1
esac
done
shift $(($OPTIND - 1))

if [ -z "$SPOOLDIR" ]; then
	SPOOLDIR="$DEFAULTDIR"
fi

echo "Creating Directorys below $SPOOLDIR. Use '-s spooldir' to change"

if [ -z "$LISTNAME" ]; then
	echo -n "What should the name of the Mailinglist be? [mlmmj-test] : "
	read LISTNAME
	if [ -z "$LISTNAME" ]; then
	LISTNAME="mlmmj-test"
	fi
fi

LISTDIR="$SPOOLDIR/$LISTNAME"

mkdir -p $LISTDIR

for DIR in incoming queue archive text subconf unsubconf bounce control \
	   moderation moderation/queue
do
	mkdir "$LISTDIR"/"$DIR"
done

touch "$LISTDIR"/index
touch "$LISTDIR"/subscribers

echo -n "The Domain for the List? [] : "
read FQDN
if [ -z "$FQDN" ]; then
	FQDN=`domainname`
fi

LISTADDRESS="$LISTNAME@$FQDN"
echo "$LISTADDRESS" > "$LISTDIR"/"listaddress"

MLMMJRECIEVE=`which mlmmj-recieve`
if [ -z "$MLMMJRECIEVE" ]; then
	MLMMJRECIEVE="/path/to/mlmmj-recieve"
fi

ALIAS="$LISTNAME:  \"|$MLMMJRECIEVE -L $SPOOLDIR/$LISTNAME/\""

if [ -n "$A_CREATE" ]; then
	echo "I want to add the following to your /etc/aliases file:"
	echo "$ALIAS"

	echo -n "is this ok? [y/N] : "
	read OKIDOKI
	case $OKIDOKI in
		y|Y)
		echo "$ALIAS" >> /etc/aliases
		;;
		n|N)
		exit 0
		;;
		*)
		echo "Options was: y, Y, n or N"
	esac
else
	echo "Don't forget to add this to /etc/aliases:"
	echo "$ALIAS"
fi

echo "Hi, this is the mlmmj program managing the mailinglist

*LSTADDR*


To confirm you want the address

*SUBADDR*


added to this list, please send a reply to

*CNFADDR*


Your mailer probably automatically replies to this address, when you hit
the reply button.

This confirmation serves two purposes. It tests that mail can be sent to your
address. Second, it makes sure someone else did not try and subscribe your
emailaddress." > $SPOOLDIR/$LISTNAME/text/sub-confirm

echo "WELCOME! You have been subscribed to the

*LSTADDR*


mailinglist." > $SPOOLDIR/$LISTNAME/text/sub-ok

echo "Hi, this is the mlmmj program managing the mailinglist

*LSTADDR*


To confirm you want the address

*SUBADDR*


removed from this list, please send a reply to

*CNFADDR*


Your mailer probably automatically replies to this address, when you hit
the reply button.

If you're not subscribed with this list, you will recieve no reply. You can
see in the From header of a mail to the mailinglist which mail you're sub-
scribed with." > $SPOOLDIR/$LISTNAME/text/unsub-confirm


echo "GOODBYE! You have been removed from the

*LSTADDR*


mailinglist." > $SPOOLDIR/$LISTNAME/text/unsub-ok

echo "Hello,

There exists the following options:

    To unsubscribe send a mail to

    *UNSUBADDR*


    To subscribe send a mail to

    *SUBADDR*


    For this help send a mail to

    *HLPADDR*

Mails can have any subject and any body." > $SPOOLDIR/$LISTNAME/text/listhelp

echo " ** DON'T FORGET **
1) The mailinglist directory have to be owned by the user running the 
mailserver (i.e. starting the binaries to work the list)
2) To run newaliases"
