type size	= int64		/* spans entire address space */
type usize	= uint64	/* unsigned size */
type off	= int64         /* file offsets */
type intptr	= uint64	/* can hold any pointer losslessly */
type time	= int64		/* milliseconds since epoch */
type scno	= int64		/* syscall */

/* processes/threads */
type pid	= int	/* process id */
type tid	= int	/* thread id */
type cloneopt	= int64	/* options for clone(2) */

/* file descriptor manipulation */
type fdopt	= int64	/* fd options */
type fd		= int32	/* fd */
type whence	= uint64	/* seek from whence */
type filemode	= uint32	/* file open mode */

type mprot	= int64	/* memory protection */
type mopt	= int64	/* memory mapping options */
type socktype	= int64	/* socket type */
type sockproto	= int32	/* socket protocol */
type sockfam	= uint16	/* socket family */
type sockopt	= int64
type msgflags	= uint32
type cmsgtype	= uint32

type epollflags	= uint32
type epollop	= uint32
type epollevttype	= uint32

type pollevt	= uint16

type futexop	= uint32
type signo	= int32
type sigflags	= int64
type fallocmode	= uint32
type mfdflags	= uint32
type aiocontext	= uint64
type msg	= void#
type arch_prctlop	= uint64


type clock = union
	`Clockrealtime
	`Clockmonotonic
	`Clockproccpu
	`Clockthreadcpu
	`Clockmonotonicraw
	`Clockrealtimecoarse
	`Clockmonotoniccoarse
	`Clockboottime
	`Clockrealtimealarm
	`Clockboottimealarm
;;

type waitstatus = union
	`Waitexit int32
	`Waitsig  int32
	`Waitstop int32
	`Waitfail int32
;;

type sigset = struct
	bits	: uint32[2]
;;

type sigaction = struct
	handler	: byte#	/* code pointer */
	flags	: sigflags
	restore	: byte#	/* code pointer */
	mask	: sigset
;;

const Sipadsz = 128
type siginfo = struct
	signo	: int
	errno	: int
	code	: int

	_pad	: int[Sipadsz]
;;

/* union { int, void* } */
type sigval = struct
	_pad	: void#
;;

const Sigevmaxsz = 64
const Sigevpadsz = Sigevmaxsz / sizeof(int) - 4
type sigevent = struct
	value	: sigval
	signo	: int
	notify	: int
	_pad	: int[Sigevpadsz]
;;

type timespec = struct
	sec	: int64
	nsec	: int64
;;

type timeval = struct
	sec	: int64
	usec	: int64
;;

type timex = struct
        modes		: uint     	/* mode selector */
        offset		: int64 	/* time offset (usec) */
        freq		: int64		/* frequency offset (scaled ppm) */
        maxerror	: int64		/* maximum error (usec) */
        esterror	: int64		/* estimated error (usec) */
        status		: int             /* clock command/status */
        constant	: int64		/* pll time constant */
        precision	: int64		/* clock precision (usec) (read only) */
        tolerance	: int64		/* clock frequency tolerance (ppm) */
        time		: timeval	/* (read only, except for ADJ_SETOFFSET) */
        tick		: int64		/* (modified) usecs between clock ticks */

        ppsfreq		: int64		/* pps frequency (scaled ppm) (ro) */
        jitter		: int64		/* pps jitter (us) (ro) */
        shift		: int		/* interval duration (s) (shift) (ro) */
        stabil		: int64		/* pps stability (scaled ppm) (ro) */
        jitcnt		: int64		/* jitter limit exceeded (ro) */
        calcnt		: int64		/* calibration intervals (ro) */
        errcnt		: int64		/* calibration errors (ro) */
        stbcnt		: int64		/* stability limit exceeded (ro) */

        tai		: int		/* TAI offset (ro) */

	__pad		: int[11]
;;


type rusage = struct
	utime	: timeval	/* user time */
	stime	: timeval	/* system time */
	_opaque	: uint64[14]	/* padding (darwin-specific data) */
;;

type sched_param = struct
	priority	: int
;;

type sched_attr = struct
        size	: uint32
        sched_policy	: uint32
        sched_flags	: uint64

        /* SCHED_NORMAL, SCHED_BATCH */
        sched_nice	: int32

        /* SCHED_FIFO, SCHED_RR */
        sched_priority	: uint32

        /* SCHED_DEADLINE */
        sched_runtime	: uint64
        sched_deadline	: uint64
        sched_period	: uint64

;;

type statbuf = struct
	dev	: uint64
	ino	: uint64
	nlink	: uint64
	mode	: filemode
	uid	: uint32
	gid	: uint32
	__pad0	: uint32
	rdev	: uint64
	size	: uint64
	blksz	: uint32
	blocks	: uint64
	atime	: timespec
	mtime	: timespec
	ctime	: timespec
	__pad1	: uint64[3]
;;

type statfs = struct
	kind	: uint64
	bsize	: uint64
	blocks	: uint64
	bfree	: uint64
	bavail	: uint64
	files	: uint64
	ffree	: uint64
	fsid	: int[2]
	namelen	: uint64
	frsize	: uint64
	flags	: uint64
	spare	: uint64[4]
;;

type ustat = struct
 	tfree	: uint32;		/* Number of free blocks.  */
    	tinode	: uint64;		/* Number of free inodes.  */
	fname	: byte[6]
	fpack	: byte[6]
;;

type dirent64 = struct
	ino	: uint64
	off	: uint64
	reclen	: uint16
	etype	: byte
	name	: byte[...]	/* special case; zero length => unchecked indexing */
;;

type utsname = struct
	system	: byte[65]
	node	: byte[65]
	release	: byte[65]
	version	: byte[65]
	machine	: byte[65]
	domain	: byte[65]
;;

type sockaddr = struct
	fam	: sockfam
	data	: byte[14]
;;

type sockaddr_in = struct
	fam	: sockfam
	port	: uint16
	addr	: byte[4]
	zero	: byte[8]
;;

type sockaddr_in6 = struct
	fam	: sockfam
	port	: uint16
	addr	: byte[16]
	scope	: uint32
;;

type sockaddr_un = struct
	fam	: sockfam
	path	: byte[108]
;;

type sockaddr_storage = struct
	fam	: sockfam
	__align	: uint32
	__pad	: byte[112]
;;

type bpfgattr = void#
type bpfmapattr = struct
	maptype	: uint32
	keysz	: uint32
	valsz	: uint32
	mapents	: uint32
	mapflg	: uint32
;;

type bpfeltattr = struct
	fd	: uint32
	key	: uint64
	val	: uint64
	flg	: uint64
;;

type bpfprogattr = struct
	kind	: uint32
	insncnt	: uint32
	insns	: uint64
	license	: uint64
	loglev	: uint32
	logsz	: uint32
	logbuf	: uint64
	kvers	: uint32
;;

type bpfobjattr = struct
	path	: uint64
	fd	: uint32
;;

type bfpattachattr = struct
	targfd	: uint32
	fd	: uint32
	kind	: uint32
	flags	: uint32
;;

type epollevt = struct
	events	: epollevttype
	data	: byte[8]
;;

type pollfd = struct
	fd	: fd
	events	: pollevt
	revents	: pollevt
;;

type file_handle = struct
	bytes	: uint
	kind	: int
	handle	: byte[...]
;;

type iovec = struct
	base	: byte#
	len	: uint64
;;

type semun = struct
	__pad	: void#
;;

type msgbuf = struct
	mtype	: int64
	buf	: byte[...]
;;

type msghdr = struct
	name		: sockaddr#
	namelen		: int32
	iov		: iovec#
	iovlen		: uint64
	control		: byte#
	controllen	: uint64
	flags		: msgflags
;;

type getcpu_cache = struct
	__opaque	: byte[128]
;;

type perf_event_attr = struct
	kind		: uint32
	size		: uint32
	config		: uint64
	timing		: uint64	/* frequency or period */
	samplekind	: uint64
	readformat	: uint64
	flags		: uint64
	wakeups		: uint32	/* events or watermarks */
	addr		: uint32	/* can also be extension of config */
	len		: uint32	/* can also be extension of config1 */
	brsamplekind	: uint64
	uregs		: uint64
	ustack		: uint32
	clockid		: int32
	intrregs	: uint64
	auxwatermark	: uint32
	samplestack	: uint16
	reserved	: uint16
;;

type mmsghdr = struct
	hdr	: msghdr
	len	: uint32
;;

type cmsghdr = struct
	len	: uint64
	level	: sockproto
	cmtype	: cmsgtype
	data	: byte[...]
;;

type capuserheader = struct
	version	: uint32
	pid	: int
;;

type capuserdata = struct
	effective	: uint32
	permitted	: uint32
	inheritable	: uint32
;;

type kexec_segment = struct
	buf	: void#
	bufsz	: size
	mem	: void#
	memsz	: size
;;

/* clone options */
const Clonesignal	: cloneopt = 0xff
const Clonevm		: cloneopt = 0x100
const Clonefs		: cloneopt = 0x200
const Clonefiles	: cloneopt = 0x400
const Clonesighand	: cloneopt = 0x800
const Cloneptrace	: cloneopt = 0x2000
const Clonevfork	: cloneopt = 0x4000
const Cloneparent	: cloneopt = 0x8000
const Clonethread	: cloneopt = 0x10000
const Clonenewns	: cloneopt = 0x20000
const Clonesysvsem	: cloneopt = 0x40000
const Clonesettls	: cloneopt = 0x80000
const Cloneparentsettid	: cloneopt = 0x100000
const Clonechildcleartid: cloneopt = 0x200000
const Clonedetached	: cloneopt = 0x400000
const Cloneuntraced	: cloneopt = 0x800000
const Clonechildsettid	: cloneopt = 0x1000000
const Clonenewuts	: cloneopt = 0x4000000
const Clonenewipc	: cloneopt = 0x8000000
const Clonenewuser	: cloneopt = 0x10000000
const Clonenewpid	: cloneopt = 0x20000000
const Clonenewnet	: cloneopt = 0x40000000
const Cloneio		: cloneopt = 0x80000000

type ptregs = struct
;;

/* open options */
const Ordonly  	: fdopt = 0x0
const Owronly  	: fdopt = 0x1
const Ordwr    	: fdopt = 0x2
const Ocreat   	: fdopt = 0x40
const Oexcl  	: fdopt = 0x80
const Otrunc   	: fdopt = 0x200
const Oappend  	: fdopt = 0x400
const Ondelay  	: fdopt = 0x800
const Odirect	: fdopt = 0x4000
const Olarge	: fdopt = 0x8000
const Odir	: fdopt = 0x10000
const Onofollow	: fdopt = 0x20000
const Onoatime	: fdopt = 0x40000
const Ocloexec	: fdopt = 0x80000

/* stat modes */
const Sifmt	: filemode = 0xf000
const Sififo	: filemode = 0x1000
const Sifchr	: filemode = 0x2000
const Sifdir	: filemode = 0x4000
const Sifblk	: filemode = 0x6000
const Sifreg	: filemode = 0x8000
const Siflnk	: filemode = 0xa000
const Sifsock	: filemode = 0xc000

/* mmap protection */
const Mprotnone	: mprot = 0x0
const Mprotrd	: mprot = 0x1
const Mprotwr	: mprot = 0x2
const Mprotexec	: mprot = 0x4
const Mprotrw	: mprot = 0x3 /* convenience */

/* mmap options */
const Mshared	: mopt = 0x1
const Mpriv	: mopt = 0x2
const Mfixed	: mopt = 0x10
const Mfile	: mopt = 0x0
const Manon	: mopt = 0x20
const M32bit	: mopt = 0x40

/* socket families. INCOMPLETE. */
const Afunspec	: sockfam = 0
const Afunix	: sockfam = 1
const Afinet	: sockfam = 2
const Afinet6	: sockfam = 10

/* socket types. */
const Sockstream	: socktype = 1	/* sequenced, reliable byte stream */
const Sockdgram		: socktype = 2	/* datagrams */
const Sockraw		: socktype = 3	/* raw proto */
const Sockrdm		: socktype = 4	/* reliably delivered messages */
const Sockseqpacket	: socktype = 5	/* sequenced, reliable packets */
const Sockdccp		: socktype = 6	/* data congestion control protocol */
const Sockpack		: socktype = 10	/* linux specific packet */

/* socket options */
const Sodebug		: sockopt = 1
const Soreuseaddr	: sockopt = 2
const Sotype		: sockopt = 3
const Soerror		: sockopt = 4
const Sodontroute	: sockopt = 5
const Sobroadcast	: sockopt = 6
const Sosndbuf		: sockopt = 7
const Sorcvbuf		: sockopt = 8
const Sosndbufforce	: sockopt = 32
const Sorcvbufforce	: sockopt = 33
const Sokeepalive	: sockopt = 9
const Sooobinline	: sockopt = 10
const Sono_check	: sockopt = 11
const Sopriority	: sockopt = 12
const Solinger		: sockopt = 13
const Sobsdcompat	: sockopt = 14
const Soreuseport	: sockopt = 15
const Sopasscred	: sockopt = 16
const Sopeercred	: sockopt = 17
const Sorcvlowat	: sockopt = 18
const Sosndlowat	: sockopt = 19
const Sorcvtimeo	: sockopt = 20
const Sosndtimeo	: sockopt = 21

/* socket option levels */
const Solsocket		: sockproto = 1

/* network protocols */
const Ipproto_ip	: sockproto = 0
const Ipproto_icmp	: sockproto = 1
const Ipproto_tcp	: sockproto = 6
const Ipproto_udp	: sockproto = 17
const Ipproto_raw	: sockproto = 255

/* message flags */
const Msgoob		: msgflags = 0x0001
const Msgpeek		: msgflags = 0x0002
const Msgdontroute	: msgflags = 0x0004
const Msgctrunc		: msgflags = 0x0008
const Msgtrunc		: msgflags = 0x0020
const Msgeor		: msgflags = 0x0080
const Msgwaitall	: msgflags = 0x0100
const Msgnosignal	: msgflags = 0x4000

/* ancillary data */
const Scmrights		: cmsgtype = 1

/* epoll flags */
const Epollcloexec	: epollflags	= 0o2000000

/* epoll ops */
const Epollctladd	: epollop	= 1
const Epollctlmod	: epollop	= 2
const Epollctldel	: epollop	= 3

/* epoll events */
const Epollin	: epollevttype = 0x001
const Epollpri	: epollevttype = 0x002
const Epollout	: epollevttype = 0x004
const Epollerr	: epollevttype = 0x008
const Epollhup	: epollevttype = 0x010
const Epollrdnorm	: epollevttype = 0x040
const Epollrdband	: epollevttype = 0x080
const Epollwrnorm	: epollevttype = 0x100
const Epollwrband	: epollevttype = 0x200
const Epollmsg		: epollevttype = 0x400
const Epollrdhup	: epollevttype = 0x2000
const Epollwakeup	: epollevttype = 1 << 29
const Epolloneshot	: epollevttype = 1 << 30
const Epolledge	: epollevttype = 1 << 31

/* futex ops */
const Futexwait	: futexop = 0
const Futexwake	: futexop = 1
/* Futexfd: removed */
const Futexrequeue	: futexop = 3
const Futexcmprequeue	: futexop = 4
const Futexwakeop	: futexop = 5
const Futexlockpi	: futexop = 6
const Futexunlockpi	: futexop = 7
const Futextrylockpi	: futexop = 8
const Futexwaitbitset	: futexop = 9
const Futexwakebitset	: futexop = 10
const Futexwaitrequeuepi	: futexop = 11
const Futexcmprequeuepi	: futexop = 12

const Futexpriv	: futexop = 128
const Futexclockrt	: futexop = 256

const Futexbitsetmatchany : int32 = -1

/* poll events : posix */
const Pollin	: pollevt = 0x001	/* There is data to read.  */
const Pollpri	: pollevt = 0x002	/* There is urgent data to read.  */
const Pollout	: pollevt = 0x004	/* Writing now will not block.  */
const Pollerr	: pollevt = 0x008           /* Error condition.  */
const Pollhup	: pollevt = 0x010           /* Hung up.  */
const Pollnval	: pollevt = 0x020           /* Invalid polling request.  */

/* poll events: xopen */
const Pollrdnorm	: pollevt = 0x040	/* Normal data may be read.  */
const Pollrdband	: pollevt = 0x080	/* Priority data may be read.  */
const Pollwrnorm	: pollevt = 0x100	/* Writing now will not block.  */
const Pollwrband	: pollevt = 0x200	/* Priority data may be written.  */

/* poll events: linux */
const Pollmsg		: pollevt = 0x400
const Pollremove	: pollevt = 0x1000
const Pollrdhup		: pollevt = 0x2000

const Seekset	: whence = 0
const Seekcur	: whence = 1
const Seekend	: whence = 2

/* return value for a failed mapping */
const Mapbad	: byte# = (-1 : byte#)

/* arch_prctl ops */
const Archsetgs : arch_prctlop = 0x1001
const Archsetfs : arch_prctlop = 0x1002
const Archgetfs : arch_prctlop = 0x1003
const Archgetgs : arch_prctlop = 0x1004

/* signal flags */
const Sanocldstop	: sigflags = 0x00000001
const Sanocldwait	: sigflags = 0x00000002
const Sasiginfo		: sigflags = 0x00000004
const Sarestorer	: sigflags = 0x04000000
const Saonstack		: sigflags = 0x08000000
const Sarestart		: sigflags = 0x10000000
const Sanodefer		: sigflags = 0x40000000
const Saresethand	: sigflags = 0x80000000
const Sanomask		: sigflags = Sanodefer
const Saoneshot		: sigflags = Saresethand

/* signal numbers */
const Sighup	: signo = 1
const Sigint	: signo = 2
const Sigquit	: signo = 3
const Sigill	: signo = 4
const Sigtrap	: signo = 5
const Sigabrt	: signo = 6
const Sigiot	: signo = 6
const Sigbus	: signo = 7
const Sigfpe	: signo = 8
const Sigkill	: signo = 9
const Sigusr1	: signo = 10
const Sigsegv	: signo = 11
const Sigusr2	: signo = 12
const Sigpipe	: signo = 13
const Sigalrm	: signo = 14
const Sigterm	: signo = 15
const Sigstkflt	: signo = 16
const Sigchld	: signo = 17
const Sigcont	: signo = 18
const Sigstop	: signo = 19
const Sigtstp	: signo = 20
const Sigttin	: signo = 21
const Sigttou	: signo = 22
const Sigurg	: signo = 23
const Sigxcpu	: signo = 24
const Sigxfsz	: signo = 25
const Sigvtalrm	: signo = 26
const Sigprof	: signo = 27
const Sigwinch	: signo = 28
const Sigio	: signo = 29
const Sigpoll	: signo = Sigio

/* fallocate mode */
const Fallockeepsize		: fallocmode = 0x01
const Fallocpunchhole		: fallocmode = 0x02
const Fallocnohidestale		: fallocmode = 0x04
const Falloccollapserange	: fallocmode = 0x08
const Falloczerorange		: fallocmode = 0x10
const Fallocinsertrange		: fallocmode = 0x20

/* memfd flags */
const Mfdcloexec	: mfdflags = 0x01
const Mfdallowsealing	: mfdflags = 0x02

/* exported values: initialized by start code */
extern var __cenvp : byte##

