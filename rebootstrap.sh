#!/bin/sh

set -ex

if [ -z $BUILDHOSTS ]; then
	BUILDHOSTS=$HOME/src/myr/buildhosts
fi

build()
{
	host=$1
	os=$2
	wrkdir=$3
	auth=$4
	passwd=$5

	makeprg=gmake
	vcs=git
	remotecmd="ssh $host"
	setpath="mkdir -p $wrkdir/root/bin && export PATH=$PATH:$wrkdir/root/bin"
	showpatch="cd '$wrkdir/mc' && cat update.patch"
	case $os in
	plan9)
		if echo $host | grep '@' > /dev/null; then
			user=$(echo $host | cut -d '@' -f1)
			host=$(echo $host | cut -d '@' -f2)
		fi
		if ! -z $passwd; then
			export PASS=$passwd
		fi
		makeprg=mk
		vcs=hg
		setpath="mkdir -p $wrkdir/root/bin && bind -bc $wrkdir/root/bin /amd64/bin; 
			 mkdir -p $wrkdir/root/lib && bind -bc $wrkdir/root/lib /amd64/lib;
			 mkdir -p $wrkdir/root/sys && bind -bc $wrkdir/root/sys /sys"
		buildcmd="rc -e -c 'webfs; cd $wrkdir/mc ; $setpath; 
			$makeprg bootstrap ; $makeprg install ; $makeprg clean ; git pull ;
			$makeprg genbootstrap ; $vcs diff > update.patch'"
		showpatch="cd $wrkdir/mc ; cat update.patch ; rm -f update.patch"
		remotecmd="drawterm -G -h $host -u $user -a $auth -c "
		;;
	linux|macos)
		makeprg=make
		vcs=git
		buildcmd="cd '$wrkdir/mc' && $setpath && ./configure --prefix=$wrkdir/root
			$makeprg bootstrap && $makeprg install && $makeprg clean && git pull &&
			$makeprg genbootstrap && $vcs diff > update.patch"

		;;
	*)
		buildcmd="cd '$wrkdir/mc' && $setpath && ./configure --prefix=$wrkdir/root
			$makeprg bootstrap && $makeprg install && $makeprg clean && git pull &&
			$makeprg genbootstrap && $vcs diff > update.patch"
		;;
	esac

	$remotecmd "rm -f $wrkdir/mc/update.patch"
	$remotecmd "$buildcmd"
	$remotecmd "$showpatch" | patch -p1 || true
}

while IFS= read -r desc; do
	if ! echo $desc | grep '^#'; then
		build $desc < /dev/null
		built=1
	fi
done < $BUILDHOSTS

if [ -z "$built" ]; then
	1>&2 echo "WARNING: no hosts to build on: Please define hosts in $BUILDHOSTS, or set \$BUILDHOSTS"
fi

