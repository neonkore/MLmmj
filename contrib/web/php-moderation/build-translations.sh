#!/bin/bash
#
# Build script for generating the MLMMJ moderation web interface locales
#

set -e

LOCALE_TRANS=fr_FR
WEB_SCRIPT_FILES=mlmmj-moderation.php

if [ ! -d translations ]; then
	echo "Wrong working directory." >&2
	exit 1
fi

echo "===> Managing internationalizations and localizations"

echo "=> Extracting strings from sources"
xgettext $WEB_SCRIPT_FILES -o translations/templates.pot

echo "=> Merging in every language .po file:"
for i in $LOCALE_TRANS; do
	echo -n "$i "
	msgmerge -s -U "translations/$i.po" translations/templates.pot
done

echo "=> Creating l10n folders"
for i in $LOCALE_TRANS; do
	mkdir -p "translations/locale/$i/LC_MESSAGES"
done

echo "=> Creating binary formats of language files:"
for i in $LOCALE_TRANS; do
	echo -n $i" "
	msgfmt -c -v -o "translations/locale/$i/LC_MESSAGES/messages.mo" \
		"translations/$i.po"
done
