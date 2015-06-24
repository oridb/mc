#!/bin/sh
pwd=`pwd`
export PATH=`pwd`:`pwd`/6:`pwd`/muse:$PATH
echo 	cd $pwd/libstd;	cd $pwd/libstd
echo 	../6/6.out	systypes.myr ;	../6/6.out	systypes.myr 
echo 	../6/6.out	`$pwd/sysselect.sh sys`;	../6/6.out	`$pwd/sysselect.sh sys`
echo 	../6/6.out	`$pwd/sysselect.sh ifreq`;	../6/6.out	`$pwd/sysselect.sh ifreq`
echo 	6a	-o util.6 `$pwd/sysselect.sh util`;	6a	-o util.6 `$pwd/sysselect.sh util`
echo 	6a	-o syscall.6 `$pwd/sysselect.sh syscall`;	6a	-o syscall.6 `$pwd/sysselect.sh syscall`
echo 	../muse/6.out	-o sys sys.use ifreq.use systypes.use ;	../muse/6.out	-o sys sys.use ifreq.use systypes.use 
echo 	ar	vu libsys.a sys.6 ifreq.6 syscall.6 systypes.6 util.6 ;	ar	vu libsys.a sys.6 ifreq.6 syscall.6 systypes.6 util.6 
echo 	../6/6.out	-I . option.myr ;	../6/6.out	-I . option.myr 
echo 	../6/6.out	-I . types.myr ;	../6/6.out	-I . types.myr 
echo 	../6/6.out	-I . `$pwd/sysselect.sh syswrap`;	../6/6.out	-I . `$pwd/sysselect.sh syswrap`
echo 	../6/6.out	-I . die.myr ;	../6/6.out	-I . die.myr 
echo 	../6/6.out	-I . sleq.myr ;	../6/6.out	-I . sleq.myr 
echo 	../6/6.out	-I . hassuffix.myr ;	../6/6.out	-I . hassuffix.myr 
echo 	../6/6.out	-I . hashfuncs.myr ;	../6/6.out	-I . hashfuncs.myr 
echo 	../6/6.out	-I . slfill.myr ;	../6/6.out	-I . slfill.myr 
echo 	../6/6.out	-I . clear.myr ;	../6/6.out	-I . clear.myr 
echo 	../6/6.out	-I . extremum.myr ;	../6/6.out	-I . extremum.myr 
echo 	../6/6.out	-I . units.myr ;	../6/6.out	-I . units.myr 
echo 	../6/6.out	-I . alloc.myr ;	../6/6.out	-I . alloc.myr 
echo 	../6/6.out	-I . rand.myr ;	../6/6.out	-I . rand.myr 
echo 	../6/6.out	-I . utf.myr ;	../6/6.out	-I . utf.myr 
echo 	../6/6.out	-I . slcp.myr ;	../6/6.out	-I . slcp.myr 
echo 	../6/6.out	-I . sldup.myr ;	../6/6.out	-I . sldup.myr 
echo 	../6/6.out	-I . chartype.myr ;	../6/6.out	-I . chartype.myr 
echo 	../6/6.out	-I . cmp.myr ;	../6/6.out	-I . cmp.myr 
echo 	../6/6.out	-I . hasprefix.myr ;	../6/6.out	-I . hasprefix.myr 
echo 	../6/6.out	-I . htab.myr ;	../6/6.out	-I . htab.myr 
echo 	../6/6.out	-I . intparse.myr ;	../6/6.out	-I . intparse.myr 
echo 	../6/6.out	-I . slpush.myr ;	../6/6.out	-I . slpush.myr 
echo 	../6/6.out	-I . strfind.myr ;	../6/6.out	-I . strfind.myr 
echo 	../6/6.out	-I . strsplit.myr ;	../6/6.out	-I . strsplit.myr 
echo 	../6/6.out	-I . `$pwd/sysselect.sh wait`;	../6/6.out	-I . `$pwd/sysselect.sh wait`
echo 	../6/6.out	-I . now.myr ;	../6/6.out	-I . now.myr 
echo 	../6/6.out	-I . strjoin.myr ;	../6/6.out	-I . strjoin.myr 
echo 	../6/6.out	-I . mk.myr ;	../6/6.out	-I . mk.myr 
echo 	../6/6.out	-I . sljoin.myr ;	../6/6.out	-I . sljoin.myr 
echo 	../6/6.out	-I . result.myr ;	../6/6.out	-I . result.myr 
echo 	../6/6.out	-I . slurp.myr ;	../6/6.out	-I . slurp.myr 
echo 	../6/6.out	-I . dirname.myr ;	../6/6.out	-I . dirname.myr 
echo 	../6/6.out	-I . `$pwd/sysselect.sh errno`;	../6/6.out	-I . `$pwd/sysselect.sh errno`
echo 	../6/6.out	-I . putint.myr ;	../6/6.out	-I . putint.myr 
echo 	../6/6.out	-I . mkpath.myr ;	../6/6.out	-I . mkpath.myr 
echo 	../6/6.out	-I . introspect.myr ;	../6/6.out	-I . introspect.myr 
echo 	../6/6.out	-I . bigint.myr ;	../6/6.out	-I . bigint.myr 
echo 	../6/6.out	-I . cstrconv.myr ;	../6/6.out	-I . cstrconv.myr 
echo 	../6/6.out	-I . fltbits.myr ;	../6/6.out	-I . fltbits.myr 
echo 	../6/6.out	-I . strbuf.myr ;	../6/6.out	-I . strbuf.myr 
echo 	../6/6.out	-I . fltfmt.myr ;	../6/6.out	-I . fltfmt.myr 
echo 	../6/6.out	-I . `$pwd/sysselect.sh syswrap-ss`;	../6/6.out	-I . `$pwd/sysselect.sh syswrap-ss`
echo 	../6/6.out	-I . varargs.myr ;	../6/6.out	-I . varargs.myr 
echo 	../6/6.out	-I . fmt.myr ;	../6/6.out	-I . fmt.myr 
echo 	../6/6.out	-I . `$pwd/sysselect.sh env`;	../6/6.out	-I . `$pwd/sysselect.sh env`
echo 	../6/6.out	-I . `$pwd/sysselect.sh resolve`;	../6/6.out	-I . `$pwd/sysselect.sh resolve`
echo 	../6/6.out	-I . pathjoin.myr ;	../6/6.out	-I . pathjoin.myr 
echo 	../6/6.out	-I . optparse.myr ;	../6/6.out	-I . optparse.myr 
echo 	../6/6.out	-I . ipparse.myr ;	../6/6.out	-I . ipparse.myr 
echo 	../6/6.out	-I . execvp.myr ;	../6/6.out	-I . execvp.myr 
echo 	../6/6.out	-I . slput.myr ;	../6/6.out	-I . slput.myr 
echo 	../6/6.out	-I . spork.myr ;	../6/6.out	-I . spork.myr 
echo 	../6/6.out	-I . getint.myr ;	../6/6.out	-I . getint.myr 
echo 	../6/6.out	-I . strstrip.myr ;	../6/6.out	-I . strstrip.myr 
echo 	../6/6.out	-I . blat.myr ;	../6/6.out	-I . blat.myr 
echo 	../6/6.out	-I . try.myr ;	../6/6.out	-I . try.myr 
echo 	../6/6.out	-I . sort.myr ;	../6/6.out	-I . sort.myr 
echo 	../6/6.out	-I . search.myr ;	../6/6.out	-I . search.myr 
echo 	../6/6.out	-I . endian.myr ;	../6/6.out	-I . endian.myr 
echo 	../6/6.out	-I . getcwd.myr ;	../6/6.out	-I . getcwd.myr 
echo 	../6/6.out	-I . swap.myr ;	../6/6.out	-I . swap.myr 
echo 	../6/6.out	-I . bitset.myr ;	../6/6.out	-I . bitset.myr 
echo 	../6/6.out	-I . `$pwd/sysselect.sh dial`;	../6/6.out	-I . `$pwd/sysselect.sh dial`
echo 	../muse/6.out	-o std putint.use fmt.use try.use sort.use blat.use pathjoin.use strjoin.use mk.use errno.use hassuffix.use execvp.use swap.use types.use ipparse.use syswrap-ss.use strfind.use utf.use cstrconv.use search.use die.use units.use wait.use sljoin.use slpush.use result.use htab.use now.use strstrip.use bitset.use getcwd.use rand.use env.use slurp.use intparse.use varargs.use strbuf.use clear.use hasprefix.use slput.use mkpath.use getint.use strsplit.use introspect.use syswrap.use resolve.use dirname.use sleq.use endian.use alloc.use optparse.use spork.use dial.use fltbits.use cmp.use sldup.use chartype.use fltfmt.use bigint.use extremum.use hashfuncs.use option.use slcp.use slfill.use ;	../muse/6.out	-o std putint.use fmt.use try.use sort.use blat.use pathjoin.use strjoin.use mk.use errno.use hassuffix.use execvp.use swap.use types.use ipparse.use syswrap-ss.use strfind.use utf.use cstrconv.use search.use die.use units.use wait.use sljoin.use slpush.use result.use htab.use now.use strstrip.use bitset.use getcwd.use rand.use env.use slurp.use intparse.use varargs.use strbuf.use clear.use hasprefix.use slput.use mkpath.use getint.use strsplit.use introspect.use syswrap.use resolve.use dirname.use sleq.use endian.use alloc.use optparse.use spork.use dial.use fltbits.use cmp.use sldup.use chartype.use fltfmt.use bigint.use extremum.use hashfuncs.use option.use slcp.use slfill.use 
echo 	ar	vu libstd.a putint.6 fmt.6 try.6 sort.6 blat.6 pathjoin.6 strjoin.6 mk.6 errno.6 hassuffix.6 execvp.6 swap.6 types.6 ipparse.6 syswrap-ss.6 strfind.6 utf.6 cstrconv.6 search.6 die.6 units.6 wait.6 sljoin.6 slpush.6 result.6 htab.6 now.6 strstrip.6 bitset.6 getcwd.6 rand.6 env.6 slurp.6 intparse.6 varargs.6 strbuf.6 clear.6 hasprefix.6 slput.6 mkpath.6 getint.6 strsplit.6 introspect.6 syswrap.6 resolve.6 dirname.6 sleq.6 endian.6 alloc.6 optparse.6 spork.6 dial.6 fltbits.6 cmp.6 sldup.6 chartype.6 fltfmt.6 bigint.6 extremum.6 hashfuncs.6 option.6 slcp.6 slfill.6 ;	ar	vu libstd.a putint.6 fmt.6 try.6 sort.6 blat.6 pathjoin.6 strjoin.6 mk.6 errno.6 hassuffix.6 execvp.6 swap.6 types.6 ipparse.6 syswrap-ss.6 strfind.6 utf.6 cstrconv.6 search.6 die.6 units.6 wait.6 sljoin.6 slpush.6 result.6 htab.6 now.6 strstrip.6 bitset.6 getcwd.6 rand.6 env.6 slurp.6 intparse.6 varargs.6 strbuf.6 clear.6 hasprefix.6 slput.6 mkpath.6 getint.6 strsplit.6 introspect.6 syswrap.6 resolve.6 dirname.6 sleq.6 endian.6 alloc.6 optparse.6 spork.6 dial.6 fltbits.6 cmp.6 sldup.6 chartype.6 fltfmt.6 bigint.6 extremum.6 hashfuncs.6 option.6 slcp.6 slfill.6 
echo 	cd $pwd;	cd $pwd
echo 	cd $pwd/libbio;	cd $pwd/libbio
echo 	../6/6.out	-I ../libstd bio.myr ;	../6/6.out	-I ../libstd bio.myr 
echo 	../6/6.out	-I ../libstd puti.myr ;	../6/6.out	-I ../libstd puti.myr 
echo 	../6/6.out	-I ../libstd geti.myr ;	../6/6.out	-I ../libstd geti.myr 
echo 	../muse/6.out	-o bio puti.use bio.use geti.use ;	../muse/6.out	-o bio puti.use bio.use geti.use 
echo 	ar	vu libbio.a puti.6 bio.6 geti.6 ;	ar	vu libbio.a puti.6 bio.6 geti.6 
echo 	cd $pwd;	cd $pwd
echo 	cd $pwd/libregex;	cd $pwd/libregex
echo 	../6/6.out	-I ../libstd types.myr ;	../6/6.out	-I ../libstd types.myr 
echo 	../6/6.out	-I ../libstd interp.myr ;	../6/6.out	-I ../libstd interp.myr 
echo 	../6/6.out	-I ../libstd ranges.myr ;	../6/6.out	-I ../libstd ranges.myr 
echo 	../6/6.out	-I ../libstd compile.myr ;	../6/6.out	-I ../libstd compile.myr 
echo 	../muse/6.out	-o regex interp.use types.use compile.use ranges.use ;	../muse/6.out	-o regex interp.use types.use compile.use ranges.use 
echo 	ar	vu libregex.a interp.6 types.6 compile.6 ranges.6 ;	ar	vu libregex.a interp.6 types.6 compile.6 ranges.6 
echo 	cd $pwd;	cd $pwd
echo 	cd $pwd/mbld;	cd $pwd/mbld
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd `$pwd/sysselect.sh config`;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd `$pwd/sysselect.sh config`
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd opts.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd opts.myr 
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd types.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd types.myr 
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd util.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd util.myr 
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd deps.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd deps.myr 
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd fsel.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd fsel.myr 
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd parse.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd parse.myr 
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd build.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd build.myr 
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd install.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd install.myr 
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd clean.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd clean.myr 
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd test.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd test.myr 
echo 	../6/6.out	-I ../libregex -I ../libbio -I ../libstd main.myr ;	../6/6.out	-I ../libregex -I ../libbio -I ../libstd main.myr 
echo 	6l	-lo mbld ../rt/_myrrt.6 clean.6 types.6 deps.6 fsel.6 util.6 parse.6 main.6 build.6 opts.6 config.6 install.6 test.6 ../libregex/libregex.a ../libbio/libbio.a ../libstd/libstd.a ../libstd/libsys.a ;	6l	-lo mbld ../rt/_myrrt.6 clean.6 types.6 deps.6 fsel.6 util.6 parse.6 main.6 build.6 opts.6 config.6 install.6 test.6 ../libregex/libregex.a ../libbio/libbio.a ../libstd/libstd.a ../libstd/libsys.a 
echo 	cd $pwd;	cd $pwd
echo 	cd $pwd/libregex;	cd $pwd/libregex
echo 	../6/6.out	-I . -I ../libbio -I ../libstd redump.myr ;	../6/6.out	-I . -I ../libbio -I ../libstd redump.myr 
echo 	6l	-lo redump ../rt/_myrrt.6 redump.6 libregex.a ../libbio/libbio.a ../libstd/libstd.a ../libstd/libsys.a ;	6l	-lo redump ../rt/_myrrt.6 redump.6 libregex.a ../libbio/libbio.a ../libstd/libstd.a ../libstd/libsys.a 
