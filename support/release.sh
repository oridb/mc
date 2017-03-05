#!/bin/sh
set -e
set -x

tmp=/tmp/myr-release
rm -rf $tmp
mkdir -p $tmp
cp -r ../mc $tmp/
(
	cd $tmp/mc
	git clean -xfd
	rm -rf .git
)

(
	cd $tmp
	tar cvf myrddin-$1.tar mc
	bzip2 myrddin-$1.tar
	tar cvf myrddin-$1.tar mc
	gzip myrddin-$1.tar
	tar cvf myrddin-$1.tar mc
	xz myrddin-$1.tar
)

cp $tmp/myrddin-$1.tar.* .

