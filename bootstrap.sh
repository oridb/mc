#!/bin/sh
pwd=`pwd`
echo 	cd $pwd/libstd
	cd $pwd/libstd
echo 	../6/6m	`$pwd/sysselect.sh syserrno`
	../6/6m	`$pwd/sysselect.sh syserrno`
echo 	../6/6m	systypes.myr 
	../6/6m	systypes.myr 
echo 	../6/6m	`$pwd/sysselect.sh sys`
	../6/6m	`$pwd/sysselect.sh sys`
echo 	../6/6m	`$pwd/sysselect.sh ifreq`
	../6/6m	`$pwd/sysselect.sh ifreq`
echo 	as	-g -o util.o `$pwd/sysselect.sh util`
	as	-g -o util.o `$pwd/sysselect.sh util`
echo 	as	-g -o syscall.o `$pwd/sysselect.sh syscall`
	as	-g -o syscall.o `$pwd/sysselect.sh syscall`
echo 	../muse/muse	-o sys ifreq.use syserrno.use systypes.use sys.use 
	../muse/muse	-o sys ifreq.use syserrno.use systypes.use sys.use 
echo 	ar	-rcs libsys.a ifreq.o util.o syserrno.o syscall.o systypes.o sys.o 
	ar	-rcs libsys.a ifreq.o util.o syserrno.o syscall.o systypes.o sys.o 
echo 	../6/6m	-I . types.myr 
	../6/6m	-I . types.myr 
echo 	../6/6m	-I . cstrconv.myr 
	../6/6m	-I . cstrconv.myr 
echo 	../6/6m	-I . option.myr 
	../6/6m	-I . option.myr 
echo 	../6/6m	-I . errno.myr 
	../6/6m	-I . errno.myr 
echo 	../6/6m	-I . `$pwd/sysselect.sh syswrap`
	../6/6m	-I . `$pwd/sysselect.sh syswrap`
echo 	../6/6m	-I . die.myr 
	../6/6m	-I . die.myr 
echo 	../6/6m	-I . sleq.myr 
	../6/6m	-I . sleq.myr 
echo 	../6/6m	-I . hassuffix.myr 
	../6/6m	-I . hassuffix.myr 
echo 	../6/6m	-I . hashfuncs.myr 
	../6/6m	-I . hashfuncs.myr 
echo 	../6/6m	-I . slfill.myr 
	../6/6m	-I . slfill.myr 
echo 	../6/6m	-I . clear.myr 
	../6/6m	-I . clear.myr 
echo 	../6/6m	-I . extremum.myr 
	../6/6m	-I . extremum.myr 
echo 	../6/6m	-I . units.myr 
	../6/6m	-I . units.myr 
echo 	../6/6m	-I . alloc.myr 
	../6/6m	-I . alloc.myr 
echo 	../6/6m	-I . utf.myr 
	../6/6m	-I . utf.myr 
echo 	../6/6m	-I . `$pwd/sysselect.sh syswrap-ss`
	../6/6m	-I . `$pwd/sysselect.sh syswrap-ss`
echo 	../6/6m	-I . varargs.myr 
	../6/6m	-I . varargs.myr 
echo 	../6/6m	-I . chartype.myr 
	../6/6m	-I . chartype.myr 
echo 	../6/6m	-I . cmp.myr 
	../6/6m	-I . cmp.myr 
echo 	../6/6m	-I . hasprefix.myr 
	../6/6m	-I . hasprefix.myr 
echo 	../6/6m	-I . slcp.myr 
	../6/6m	-I . slcp.myr 
echo 	../6/6m	-I . sldup.myr 
	../6/6m	-I . sldup.myr 
echo 	../6/6m	-I . slpush.myr 
	../6/6m	-I . slpush.myr 
echo 	../6/6m	-I . bigint.myr 
	../6/6m	-I . bigint.myr 
echo 	../6/6m	-I . fltbits.myr 
	../6/6m	-I . fltbits.myr 
echo 	../6/6m	-I . fltfmt.myr 
	../6/6m	-I . fltfmt.myr 
echo 	../6/6m	-I . fmt.myr 
	../6/6m	-I . fmt.myr 
echo 	../6/6m	-I . rand.myr 
	../6/6m	-I . rand.myr 
echo 	../6/6m	-I . `$pwd/sysselect.sh wait`
	../6/6m	-I . `$pwd/sysselect.sh wait`
echo 	../6/6m	-I . now.myr 
	../6/6m	-I . now.myr 
echo 	../6/6m	-I . strjoin.myr 
	../6/6m	-I . strjoin.myr 
echo 	../6/6m	-I . mk.myr 
	../6/6m	-I . mk.myr 
echo 	../6/6m	-I . sljoin.myr 
	../6/6m	-I . sljoin.myr 
echo 	../6/6m	-I . result.myr 
	../6/6m	-I . result.myr 
echo 	../6/6m	-I . slurp.myr 
	../6/6m	-I . slurp.myr 
echo 	../6/6m	-I . strfind.myr 
	../6/6m	-I . strfind.myr 
echo 	../6/6m	-I . dirname.myr 
	../6/6m	-I . dirname.myr 
echo 	../6/6m	-I . putint.myr 
	../6/6m	-I . putint.myr 
echo 	../6/6m	-I . mkpath.myr 
	../6/6m	-I . mkpath.myr 
echo 	../6/6m	-I . strsplit.myr 
	../6/6m	-I . strsplit.myr 
echo 	../6/6m	-I . `$pwd/sysselect.sh env`
	../6/6m	-I . `$pwd/sysselect.sh env`
echo 	../6/6m	-I . endian.myr 
	../6/6m	-I . endian.myr 
echo 	../6/6m	-I . htab.myr 
	../6/6m	-I . htab.myr 
echo 	../6/6m	-I . intparse.myr 
	../6/6m	-I . intparse.myr 
echo 	../6/6m	-I . ipparse.myr 
	../6/6m	-I . ipparse.myr 
echo 	../6/6m	-I . strstrip.myr 
	../6/6m	-I . strstrip.myr 
echo 	../6/6m	-I . `$pwd/sysselect.sh resolve`
	../6/6m	-I . `$pwd/sysselect.sh resolve`
echo 	../6/6m	-I . pathjoin.myr 
	../6/6m	-I . pathjoin.myr 
echo 	../6/6m	-I . optparse.myr 
	../6/6m	-I . optparse.myr 
echo 	../6/6m	-I . `$pwd/sysselect.sh dir`
	../6/6m	-I . `$pwd/sysselect.sh dir`
echo 	../6/6m	-I . execvp.myr 
	../6/6m	-I . execvp.myr 
echo 	../6/6m	-I . slput.myr 
	../6/6m	-I . slput.myr 
echo 	../6/6m	-I . spork.myr 
	../6/6m	-I . spork.myr 
echo 	../6/6m	-I . getint.myr 
	../6/6m	-I . getint.myr 
echo 	../6/6m	-I . bytebuf.myr 
	../6/6m	-I . bytebuf.myr 
echo 	../6/6m	-I . blat.myr 
	../6/6m	-I . blat.myr 
echo 	../6/6m	-I . try.myr 
	../6/6m	-I . try.myr 
echo 	../6/6m	-I . sort.myr 
	../6/6m	-I . sort.myr 
echo 	../6/6m	-I . search.myr 
	../6/6m	-I . search.myr 
echo 	../6/6m	-I . getcwd.myr 
	../6/6m	-I . getcwd.myr 
echo 	../6/6m	-I . `$pwd/sysselect.sh dial`
	../6/6m	-I . `$pwd/sysselect.sh dial`
echo 	../6/6m	-I . bitset.myr 
	../6/6m	-I . bitset.myr 
echo 	../6/6m	-I . swap.myr 
	../6/6m	-I . swap.myr 
echo 	../muse/muse	-o std syswrap-ss.use slfill.use dial.use putint.use fmt.use syswrap.use try.use sort.use blat.use pathjoin.use strjoin.use dir.use mk.use swap.use hassuffix.use execvp.use types.use ipparse.use strfind.use utf.use cstrconv.use search.use die.use units.use sljoin.use slpush.use result.use bytebuf.use now.use htab.use env.use strstrip.use getcwd.use bitset.use rand.use resolve.use slurp.use intparse.use varargs.use clear.use hasprefix.use slput.use mkpath.use getint.use strsplit.use dirname.use sleq.use endian.use alloc.use optparse.use spork.use fltbits.use cmp.use sldup.use chartype.use fltfmt.use bigint.use option.use extremum.use hashfuncs.use wait.use errno.use slcp.use 
	../muse/muse	-o std syswrap-ss.use slfill.use dial.use putint.use fmt.use syswrap.use try.use sort.use blat.use pathjoin.use strjoin.use dir.use mk.use swap.use hassuffix.use execvp.use types.use ipparse.use strfind.use utf.use cstrconv.use search.use die.use units.use sljoin.use slpush.use result.use bytebuf.use now.use htab.use env.use strstrip.use getcwd.use bitset.use rand.use resolve.use slurp.use intparse.use varargs.use clear.use hasprefix.use slput.use mkpath.use getint.use strsplit.use dirname.use sleq.use endian.use alloc.use optparse.use spork.use fltbits.use cmp.use sldup.use chartype.use fltfmt.use bigint.use option.use extremum.use hashfuncs.use wait.use errno.use slcp.use 
echo 	ar	-rcs libstd.a syswrap-ss.o slfill.o dial.o putint.o fmt.o syswrap.o try.o sort.o blat.o pathjoin.o strjoin.o dir.o mk.o swap.o hassuffix.o execvp.o types.o ipparse.o strfind.o utf.o cstrconv.o search.o die.o units.o sljoin.o slpush.o result.o bytebuf.o now.o htab.o env.o strstrip.o getcwd.o bitset.o rand.o resolve.o slurp.o intparse.o varargs.o clear.o hasprefix.o slput.o mkpath.o getint.o strsplit.o dirname.o sleq.o endian.o alloc.o optparse.o spork.o fltbits.o cmp.o sldup.o chartype.o fltfmt.o bigint.o option.o extremum.o hashfuncs.o wait.o errno.o slcp.o 
	ar	-rcs libstd.a syswrap-ss.o slfill.o dial.o putint.o fmt.o syswrap.o try.o sort.o blat.o pathjoin.o strjoin.o dir.o mk.o swap.o hassuffix.o execvp.o types.o ipparse.o strfind.o utf.o cstrconv.o search.o die.o units.o sljoin.o slpush.o result.o bytebuf.o now.o htab.o env.o strstrip.o getcwd.o bitset.o rand.o resolve.o slurp.o intparse.o varargs.o clear.o hasprefix.o slput.o mkpath.o getint.o strsplit.o dirname.o sleq.o endian.o alloc.o optparse.o spork.o fltbits.o cmp.o sldup.o chartype.o fltfmt.o bigint.o option.o extremum.o hashfuncs.o wait.o errno.o slcp.o 
echo 	cd $pwd
	cd $pwd
echo 	cd $pwd/libbio
	cd $pwd/libbio
echo 	../6/6m	-I ../libstd bio.myr 
	../6/6m	-I ../libstd bio.myr 
echo 	../6/6m	-I ../libstd puti.myr 
	../6/6m	-I ../libstd puti.myr 
echo 	../6/6m	-I ../libstd geti.myr 
	../6/6m	-I ../libstd geti.myr 
echo 	../muse/muse	-o bio puti.use bio.use geti.use 
	../muse/muse	-o bio puti.use bio.use geti.use 
echo 	ar	-rcs libbio.a puti.o bio.o geti.o 
	ar	-rcs libbio.a puti.o bio.o geti.o 
echo 	cd $pwd
	cd $pwd
echo 	cd $pwd/libregex
	cd $pwd/libregex
echo 	../6/6m	-I ../libstd types.myr 
	../6/6m	-I ../libstd types.myr 
echo 	../6/6m	-I ../libstd interp.myr 
	../6/6m	-I ../libstd interp.myr 
echo 	../6/6m	-I ../libstd ranges.myr 
	../6/6m	-I ../libstd ranges.myr 
echo 	../6/6m	-I ../libstd compile.myr 
	../6/6m	-I ../libstd compile.myr 
echo 	../muse/muse	-o regex interp.use types.use compile.use ranges.use 
	../muse/muse	-o regex interp.use types.use compile.use ranges.use 
echo 	ar	-rcs libregex.a interp.o types.o compile.o ranges.o 
	ar	-rcs libregex.a interp.o types.o compile.o ranges.o 
echo 	cd $pwd
	cd $pwd
echo 	cd $pwd/mbld
	cd $pwd/mbld
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex config.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex config.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex types.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex types.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex opts.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex opts.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex util.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex util.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex subdir.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex subdir.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex deps.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex deps.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex fsel.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex fsel.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex parse.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex parse.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex build.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex build.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex install.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex install.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex clean.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex clean.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex test.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex test.myr 
echo 	../6/6m	-I ../libstd -I ../libbio -I ../libregex main.myr 
	../6/6m	-I ../libstd -I ../libbio -I ../libregex main.myr 
echo 	ld	-o mbld ../rt/_myrrt.o clean.o config.o deps.o types.o fsel.o util.o subdir.o main.o parse.o build.o opts.o install.o test.o -L../libstd -L../libbio -L../libregex -L/home/ori/bin/lib/myr -lregex -lbio -lstd -lsys -lsys -lstd -lbio -lregex 
	ld	-o mbld ../rt/_myrrt.o clean.o config.o deps.o types.o fsel.o util.o subdir.o main.o parse.o build.o opts.o install.o test.o -L../libstd -L../libbio -L../libregex -L/home/ori/bin/lib/myr -lregex -lbio -lstd -lsys -lsys -lstd -lbio -lregex 
