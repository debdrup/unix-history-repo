: run this script through /bin/sh
: '@(#)$Id: viamail.sh,v 1.2 1992/10/16 17:15:00 jromine Exp $'

DELAY=0 FROM=
case "$1" in
    -*)	DELAY="`echo $1 | sed -e 's%-%%'`"
	shift
	;;
esac
if [ ! -z "$PERSON" ]; then
    FROM="-viafrom $PERSON"
fi

if [ $# -lt 3 ]; then
    echo 'usage: viamail: "mailpath" "subject-string" directory-or-file ...' 1>&2
    exit 1;
fi

mailpath="$1"
echo "mailpath = $mailpath" 1>&2
shift

subject="$1"
echo "subject-string = $subject" 1>&2
shift

echo "files = $*" 1>&2

tar cvf - $* | compress | \
    mhn -viamail "$mailpath" -viasubj "$subject" \
	-viaparm "type=tar; conversions=x-compress" \
	-viacmnt "extract with uncompress | tar xvpf -" \
	-viadelay "$DELAY" \
	-verbose $FROM
