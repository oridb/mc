type size	= int64		/* spans entire address space */
type usize	= uint64	/* unsigned size */
type off	= int64		/* file offsets */
type intptr	= uint64	/* can hold any pointer losslessly */
type time	= int64		/* milliseconds since epoch */
type pid	= int		/* process id */
type scno	= int64		/*syscall*/
type fdopt	= int64		/* fd options */
type fd		= int32		/* fd */
type whence	= uint64	/* seek from whence */
type mprot	= int64		/* memory protection */
type mopt	= int64		/* memory mapping options */
type socktype	= int64		/* socket type */
type sockopt	= int64		/* socket option */
type sockproto	= int64		/* socket protocol */
type sockfam	= uint8		/* socket family */
type filemode	= uint16
type filetype	= uint8
type fcntlcmd	= int64
type umtxop	= int32
type signo	= int32
type sigflags	= int32
type gid	= int32
type uid	= int32
type ffcounter	= int64
type register	= int64
type lwpid	= int32
type rlim	= int64
type socklen	= int32
type cpuwhich	= int32
type id		= int64
type cpulevel	= int
type cpusetid	= int
type idtype	= int
type sysarchop	= int

type acltype	= int
type acltag	= uint32
type aclperm	= uint32
type aclenttype	= uint16
type aclflag	= uint16
type aclpermset = int#
type aclflagset	= uint16#
type clockid	= int32
type dev	= int32

type caddr	= byte#

type clock = union
	`Clockrealtime
	`Clockrealtime_precise
	`Clockrealtime_fast
	`Clockmonotonic
	`Clockmonotonic_precise     
	`Clockmonotonic_fast
	`Clockuptime
	`Clockuptime_precise
	`Clockuptime_fast
	`Clockvirtual
	`Clockprof
	`Clocksecond
;;

type kevent = struct
	ident	: intptr	/* identifier for this event */
	filter	: int16		/* filter for event */
	flags	: uint16
	fflags	: int32
	data	: intptr
	udata	: void#		/* user : opaque data identifier */
;;

type pollfd = struct
	fd	: fd
	events	: uint16
	revents	: uint16
;;

const Fdsetsize = 1024
type fdset = struct
	_mask	: uint64[Fdsetsize/64]
;;

type bintime = struct
	sec	: time
	frac	: uint64
;;

type mac = struct
	buflen	: size
	string	: byte#
;;

const Aclmaxent = 254
type acl = struct
	maxcnt	: uint
	cnt	: uint
	/* Will be required e.g. to implement NFSv4.1 ACL inheritance. */
	spare	: int[4];
	entry	: aclentry[Aclmaxent];
;;

type aclentry = struct
	tag	: acltag
	id	: uid
	perm	: aclperm
	/* NFSv4 entry type, "allow" or "deny".  Unused in POSIX.1e ACLs. */
	etype	: aclenttype
	/* NFSv4 ACL inheritance.  Unused in POSIX.1e ACLs. */
	flags	: aclflag
;;

const _MC_FPFMT_NODEV	: int64 = 0x10000	/* device not present or configured */
const _MC_FPFMT_XMM	: int64 = 0x10002
const _MC_FPOWNED_NONE	: int64 = 0x20000	/* FP state not used */
const _MC_FPOWNED_FPU	: int64 = 0x20001	/* FP state came from FPU */
const _MC_FPOWNED_PCB	: int64 = 0x20002	/* FP state came from PCB */

type mcontext = struct
	/*
	 * The definition of mcontext must match the layout of
	 * struct sigcontext after the sc_mask member.  This is so
	 * that we can support sigcontext and ucontext at the same
	 * time.
	 */
	onstack	: register	/* XXX - sigcontext compat. */
	rdi	: register		/* machine state (struct trapframe) */
	rsi	: register
	rdx	: register
	rcx	: register
	r8	: register
	r9	: register
	rax	: register
	rbx	: register
	rbp	: register
	r10	: register
	r11	: register
	r12	: register
	r13	: register
	r14	: register
	r15	: register
	trapno	: uint32
	fs	: uint16
	gs	: uint16
	addr	: register
	flags	: uint32
	es	: uint16
	ds	: uint16
	err	: register
	rip	: register
	cs	: register
	rflags	: register
	rsp	: register
	ss	: register

	len	: int64			/* sizeof(mcontext) */

	fpfmt	: int64
	ownedfp	: int64
	/*
	 * See <machine/fpu.h> for the internals of mc_fpstate[].
	 */
	/* FIXME: needs padding? */
	fpstate	: uint64[64]// __aligned(16);

	fsbase	: register
	gsbase	: register

	xfpustate	: register
	xfpustate_len	: register

	mc_spare : int64[4];
;;

type ucontext = struct
	/*
	 * Keep the order of the first two fields. Also,
	 * keep them the first two fields in the structure.
	 * This way we can have a union with struct
	 * sigcontext and ucontext. This allows us to
	 * support them both at the same time.
	 * note: the union is not defined, though.
	 */
	sigmask		: sigset
	mcontext	: mcontext
	link		: ucontext#
	sigstack	: sigstack

	flags		: int
	spare		: int[4]
;;

type ffclock_estimate = struct
	update_time	: bintime	/* Time of last estimates update. */
	update_ffcount	: ffcounter	/* Counter value at last update. */
	leapsec_next	: ffcounter	/* Counter value of next leap second. */
	period		: uint64	/* Estimate of counter period. */
	errb_abs	: uint32	/* Bound on absolute clock error [ns]. */
	errb_rate	: uint32	/* Bound on counter rate error [ps/s]. */
	status		: uint32	/* Clock status. */
	leapsec_total	: int16		/* All leap seconds seen so far. */
	leapsec		: int8		/* Next leap second (in {-1,0,1}). */
;;

type sigset = struct
	bits	: uint32[4]
;;

type sigaction = struct
	handler	: byte#	/* code pointer */
	flags	: sigflags
	mask	: sigset
;;

type sigstack = struct
	sp	: void#	/* signal stack base */
	size	: size 	/* signal stack length */
	flags	: int  	/* SS_DISABLE and/or SS_ONSTACK */
;;

type sigevent = struct
	notify	: int;		/* Notification type */
	signo	: int;		/* Signal number */
	value	: uint64
	_union	: uint64[8]	/* FIXME: replace with actual type */
;;

type siginfo = struct
	signo	: int		/* signal number */
	errno	: int		/* errno association */
	/*
	 * Cause of signal, one of the SI_ macros or signal-specific
	 * values, i.e. one of the FPE_... values for SIGFPE.  This
	 * value is equivalent to the second argument to an old-style
	 * FreeBSD signal handler.
	 */
	code	: int		/* signal code */
	pid	: pid			/* sending process */
	uid	: uid			/* sender's ruid */
	status	: int		/* exit value */
	addr	: void#		/* faulting instruction */
	val	: uint64
	_spare1	: int64	/* FIXME: abi */
	_spare2	: int[7]
;;

type waitstatus = union
	`Waitfail int32
	`Waitexit int32
	`Waitsig  int32
	`Waitstop int32
;;

type timezone = struct
	minswest	: int	/* minutes west of Greenwich */
	dsttime		: int	/* type of dst correction */
;;

type timespec = struct
	sec	: int64
	nsec	: int64
;;

type timeval = struct
	sec	: int64
	usec	: int64
;;

type itimerval = struct
	interval	: timeval
	value		: timeval
;;

type itimerspec = struct
	interval	: timespec
	value		: timespec
;;

/*
 * NTP daemon interface -- ntp_adjtime(2) -- used to discipline CPU clock
 * oscillator and control/determine status.
 *
 * Note: The offset, precision and jitter members are in microseconds if
 * STA_NANO is zero and nanoseconds if not.
 */
type timex = struct
	modes 		: uint	/* clock mode bits (wo) */
	offset		: int64			/* time offset (ns/us) (rw) */
	freq		: int64			/* frequency offset (scaled PPM) (rw) */
	maxerror	: int64		/* maximum error (us) (rw) */
	esterror	: int64		/* estimated error (us) (rw) */
	status		: int	   	/* clock status bits (rw) */
	constant	: int64		/* poll interval (log2 s) (rw) */
	precision	: int64		/* clock precision (ns/us) (ro) */
	tolerance	: int64		/* clock frequency tolerance (scaled
				 	 * PPM) (ro) */
	/*
	 * following : The read-only structure members are implemented
	 * if : only the PPS signal discipline is configured in the
	 * kernel. are : They included in all configurations to insure
	 * portability.
	 */
	ppsfreq	: int64		/* PPS frequency (scaled PPM) (ro) */
	jitter	: int64			/* PPS jitter (ns/us) (ro) */
	shift	: int			/* interval duration (s) (shift) (ro) */
	stabil	: int64			/* PPS stability (scaled PPM) (ro) */
	jitcnt	: int64			/* jitter limit exceeded (ro) */
	calcnt	: int64			/* calibration intervals (ro) */
	errcnt	: int64			/* calibration errors (ro) */
	stbcnt	: int64			/* stability limit exceeded (ro) */
;;

type rusage = struct
	utime	: timeval /* user time */
	stime	: timeval /* system time */
	maxrss	: uint64 /* max resident set size*/
	ixrss	: uint64 /* shared text size */
	idrss	: uint64 /* unshared data size */
	isrss	: uint64 /* unshared stack size */
	minflt	: uint64 /* page reclaims */
	majflt	: uint64 /* page faults */
	nswap	: uint64 /* swaps */
	inblock	: uint64 /* block input ops */
	oublock	: uint64 /* block output ops */
	msgsnd	: uint64 /* messages sent */	
	msgrcv	: uint64 /* messages received */
	nsignals : uint64 /* signals received */
	nvcsw	: uint64 /* voluntary context switches */
	nivcsw	: uint64 /* involuntary context switches */
;;

type wrusage = struct
	self	: rusage
	child	: rusage
;;

type rlimit = struct
	cur	: rlim
	max	: rlim
;;

const Caprightsversion	= 0
type caprights = struct
	rights	: uint64[Caprightsversion + 2]
;;

type statbuf = struct
	dev	: uint32 
	ino	: uint32 
	mode	: filemode
	nlink	: uint16
	uid	: uint32
	gid	: uint32
	rdev	: uint32
	atime	: timespec
	mtime	: timespec
	ctime	: timespec
	size	: int64
	blocks	: int64
	blksize	: uint32
	flags	: uint32
	gen	: uint32
	lspare	: int32
	birthtim	: timespec 
;;

const Mfsnamelen	= 16		/* length of type name including null */
const Mnamelen		= 88		/* size of on/from name bufs */
const Statfs_version	= 0x20030518	/* current version number */
type statfs = struct
	version		: uint32	/* structure version number */
	kind		: uint32	/* type of filesystem */
	flags		: uint64	/* copy of mount exported flags */
	bsize		: uint64	/* filesystem fragment size */
	iosize		: uint64	/* optimal transfer block size */
	blocks		: uint64	/* total data blocks in filesystem */
	bfree		: uint64	/* free blocks in filesystem */
	bavail		: int64		/* free blocks avail to non-superuser */
	files		: uint64	/* total file nodes in filesystem */
	ffree		: int64		/* free nodes avail to non-superuser */
	syncwrites	: uint64	/* count of sync writes since mount */
	asyncwrites	: uint64	/* count of async writes since mount */
	syncreads	: uint64	/* count of sync reads since mount */
	asyncreads	: uint64	/* count of async reads since mount */
	spare		: uint64[10]	/* unused spare */
	namemax		: uint32	/* maximum filename length */
	owner	  	: uid		/* user that mounted the filesystem */
	fsid	  	: fsid		/* filesystem id */
	charspare	: byte[80]	/* spare string space */
	fstypename	: byte[Mfsnamelen]	/* filesystem type name */
	mntfromname	: byte[Mnamelen]	/* mounted filesystem */
	mntonname	: byte[Mnamelen]	/* directory on which mounted */
;;

type fhandle = struct
	fsid	: fsid
	fid	: fid
;;

type fsid = struct
	val	: int32[2]
;;

const Maxfidsz = 16
type fid = struct
	len	: int16		/* length of data in bytes */
	data0	: int16	/* force longword alignment */
	data	: byte[Maxfidsz];	/* data (variable length) */
;;

type utsname = struct
	system	: byte[256]
	node	: byte[256] 
	release	: byte[256]
	version	: byte[256]
	machine	: byte[256]
;;


type inaddr = struct
	addr	: byte[4]
;;

type in6addr = struct
	addr	: byte[16]
;;

type sockaddr = struct
	len	: byte
	fam	: sockfam
	data	: byte[14] /* what is the *actual* length? */
;;

type sockaddr_in = struct
	len	: byte
	fam	: sockfam
	port	: uint16
	addr	: byte[4]
	zero	: byte[8]
;;

type sockaddr_in6 = struct
	len	: byte
	fam	: sockfam
	port	: uint16
	flow	: uint32
	addr	: byte[16]
	scope	: uint32
;;

type sockaddr_un = struct
	len	: uint8
	fam	: sockfam
	path	: byte[104]
;;

type sockaddr_storage = struct
	len	: byte
	fam	: sockfam
	__pad1	: byte[6]
	__align	: int64
	__pad2 	: byte[112]
;;	

type dirent = struct
	fileno	: uint32
	reclen	: uint16
	ftype	: filetype
	namelen	: uint8
	name	: byte[256]	
;;	

type rtprio = struct
	rttype	: uint16
	rtprio	: uint16
;;

type thrparam = struct
	startfn	: void#	/* pointer to code for thread entry */
	arg	: void#	/* pointer argument for thread entry */
	stkbase	: byte#	/* stack base address */
	stksz	: size	/* size of stack */
	tlsbase	: byte#	/* base of thread local storage */
	tlssz	: size	/* size of tls */
	tid	: uint64#	/* place to store new tid */
	ptid	: uint64#	/* place to store parent tid */
	flags	: int32	/* flags for the thread */
	rtp	: rtprio#	/* realtime priority */
	spare	: void#[3]	/* padding */
;;

type iovec = struct
	base	: byte#
	len	: uint64
;;

const Maxcpu = 256
type cpuset = struct
	bits	: uint64[Maxcpu/64]
;;

type sched_param = struct
	priority	: int
;;

const Jailapi =	2
type jail =  struct
	version	: uint32;
	path	: byte#
	hostname		: byte#
	jailname		: byte#
	ip4s	: uint32
	ip6s	: uint32
	ip4	: inaddr#
	ip6	: in6addr#
;;

const Msgoob		= 0x1		/* process out-of-band data */
const Msgpeek		= 0x2		/* peek at incoming message */
const Msgdontroute	= 0x4		/* send without using routing tables */
const Msgeor		= 0x8		/* data completes record */
const Msgtrunc		= 0x10		/* data discarded before delivery */
const Msgctrunc		= 0x20		/* control data lost before delivery */
const Msgwaitall	= 0x40		/* wait for full request or error */
const Msgnosignal	= 0x20000	/* do not generate SIGPIPE on EOF */
const Msgdontwait	= 0x80		/* this message should be nonblocking */
const Msgeof		= 0x100		/* data completes connection */
const Msgnotification	= 0x2000        /* SCTP notification */
const Msgnbio		= 0x4000	/* FIONBIO mode, used by fifofs */
const Msgcompat      	= 0x8000	/* used in sendit() */
const Msgcmsg_cloexec 	= 0x40000	/* make received fds close-on-exec */
const Msgwaitforone	= 0x80000	/* for recvmmsg() */
type msghdr = struct
	name	: void#	/* optional address */
	namelen	: int32	/* size of address */
	iov	: iovec	/* scatter/gather array */
	iovlen	: int	/* # elements in msg_iov */
	ctl	: void#	/* ancillary data, see below */
	ctllen	: int32	/* ancillary data buffer len */
	flags	: int	/* flags on received message */
;;

type sf_hdtr = struct
	hdr	: iovec#
	hdrcnt	: int
	trl	: iovec#
	trlcnt	: int
;;

type aiocb = struct
	fildes	: int		/* File descriptor */
	offset	: off		/* File offset for I/O */
	buf	: void# 	/* I/O buffer in process space */
	nbytes	: size		/* Number of bytes for I/O */
	__spare__ 	: int[2]
	__spare2__	: void#
	lio_opcode	: int		/* LIO opcode */
	reqprio		: int		/* Request priority -- ignored */
	_aiocb_private	: __aiocb_private
	sigevent	: sigevent	/* Signal to deliver */
;;

type __aiocb_private = struct
	status	: int64
	error	: int64
	kinfo	: void#
;;

const _Uuidnodesz = 6
type uuid = struct
	time_low			: uint32
	time_mid			: uint16
	time_hi_and_version		: uint16
	clock_seq_hi_and_reserved	: uint8
	clock_seq_low			: uint8
	node				: uint8[_Uuidnodesz];
;;

const Umtxabstime = 1
type umtx_time = struct
	timeout	: timespec
	flags	: uint32
	clockid	: uint32
;;

/* open options */
const Ordonly  	: fdopt = 0x0
const Owronly  	: fdopt = 0x1
const Ordwr    	: fdopt = 0x2
const Oappend  	: fdopt = 0x8
const Ocreat   	: fdopt = 0x200
const Onofollow	: fdopt = 0x100
const Ondelay  	: fdopt = 0x4
const Otrunc   	: fdopt = 0x400
const Odir	: fdopt = 0x20000

const Oshlock	: fdopt = 0x0010	/* open with shared file lock */
const Oexlock	: fdopt = 0x0020	/* open with exclusive file lock */
const Oasync	: fdopt = 0x0040	/* signal pgrp when data ready */
const Ofsync	: fdopt = 0x0080	/* synchronous writes */
const Oexcl	: fdopt = 0x0800	/* error if already exists */
const Ocloexec	: fdopt = 0x00100000

/* stat modes */	
const Sifmt	: filemode = 0xf000
const Sififo	: filemode = 0x1000
const Sifchr	: filemode = 0x2000
const Sifdir	: filemode = 0x4000
const Sifblk	: filemode = 0x6000
const Sifreg	: filemode = 0x8000
const Siflnk	: filemode = 0xa000
const Sifsock 	: filemode = 0xc000

/* mmap protection */
const Mprotnone	: mprot = 0x0
const Mprotrd	: mprot = 0x1
const Mprotwr	: mprot = 0x2
const Mprotexec	: mprot = 0x4
const Mprotrw	: mprot = 0x3

/* mmap options */
const Mshared	: mopt = 0x1
const Mpriv	: mopt = 0x2
const Mfixed	: mopt = 0x10
const Mfile	: mopt = 0x0
const Manon	: mopt = 0x1000
const M32bit	: mopt = 0x80000

/* file types */
const Dtunknown	: filetype = 0
const Dtfifo	: filetype = 1
const Dtchr	: filetype = 2
const Dtdir	: filetype = 4
const Dtblk	: filetype = 6
const Dtreg	: filetype = 8
const Dtlnk	: filetype = 10
const Dtsock	: filetype = 12
const Dtwht	: filetype = 14

/* socket families. INCOMPLETE. */
const Afunspec	: sockfam = 0
const Afunix	: sockfam = 1
const Afinet	: sockfam = 2
const Afinet6	: sockfam = 28

/* socket types. */
const Sockstream	: socktype = 1
const Sockdgram		: socktype = 2
const Sockraw		: socktype = 3
const Sockrdm		: socktype = 4
const Sockseqpacket	: socktype = 5

/* socket options */
const Sodebug	: sockopt = 0x0001		/* turn on debugging info recording */
const Soacceptconn	: sockopt = 0x0002	/* socket has had listen() */
const Soreuseaddr	: sockopt = 0x0004	/* allow local address reuse */
const Sokeepalive	: sockopt = 0x0008	/* keep connections alive */
const Sodontroute	: sockopt = 0x0010	/* just use interface addresses */
const Sobroadcast	: sockopt = 0x0020	/* permit sending of broadcast msgs */
const Souseloopback	: sockopt = 0x0040	/* bypass hardware when possible */
const Solinger		: sockopt = 0x0080	/* linger on close if data present */
const Sooobinline	: sockopt = 0x0100	/* leave received OOB data in line */
const Soreuseport	: sockopt = 0x0200	/* allow local address & port reuse */
const Sotimestamp	: sockopt = 0x0400	/* timestamp received dgram traffic */
const Sonosigpipe	: sockopt = 0x0800	/* no SIGPIPE from EPIPE */
const Soacceptfilter	: sockopt = 0x1000	/* there is an accept filter */
const Sobintime		: sockopt = 0x2000	/* timestamp received dgram traffic */
const Sonooffload	: sockopt = 0x4000	/* socket cannot be offloaded */
const Sonoddp		: sockopt = 0x8000	/* disable direct data placement */

/* socket option levels */
const Solsocket		: sockproto = 0xffff

/* network protocols */
const Ipproto_ip	: sockproto = 0
const Ipproto_icmp	: sockproto = 1
const Ipproto_tcp	: sockproto = 6
const Ipproto_udp	: sockproto = 17
const Ipproto_raw	: sockproto = 255

/* poll options */
const Pollin		: uint16 = 0x0001	/* any readable data available */
const Pollpri		: uint16 = 0x0002	/* OOB/Urgent readable data */
const Pollout		: uint16 = 0x0004	/* file descriptor is writeable */
const Pollrdnorm	: uint16 = 0x0040	/* non-OOB/URG data available */
const Pollwrnorm	: uint16 = Pollout	/* no write type differentiation */
const Pollrdband	: uint16 = 0x0080	/* OOB/Urgent readable data */
const Pollwrband	: uint16 = 0x0100	/* OOB/Urgent data can be written */
/* General FreeBSD extension (currently only supported for sockets): */
const Pollinigneof	: uint16 = 0x2000	/* like POLLIN, except ignore EOF */
/*
* These events are set if they occur regardless of whether they were
* requested.
*/
const Pollerr		: uint16 = 0x0008		/* some poll error occurred */
const Pollhup		: uint16 = 0x0010		/* file descriptor was "hung up" */
const Pollnval		: uint16 = 0x0020		/* requested events "invalid" */

const Seekset	: whence = 0
const Seekcur	: whence = 1
const Seekend	: whence = 2

/* system specific constants */
const Maxpathlen	: size = 1024

/* fcntl constants */
const Fdupfd	: fcntlcmd = 0		/* duplicate file descriptor */
const Fgetfd	: fcntlcmd = 1		/* get file descriptor flags */
const Fsetfd	: fcntlcmd = 2		/* set file descriptor flags */
const Fgetfl	: fcntlcmd = 3		/* get file status flags */
const Fsetfl	: fcntlcmd = 4		/* set file status flags */
const Fgetown	: fcntlcmd = 5		/* get SIGIO/SIGURG proc/pgrp */
const Fsetown	: fcntlcmd = 6		/* set SIGIO/SIGURG proc/pgrp */
const Fogetlk	: fcntlcmd = 7		/* get record locking information */
const Fosetlk	: fcntlcmd = 8		/* set record locking information */
const Fosetlkw	: fcntlcmd = 9		/* F_SETLK; wait if blocked */
const Fdup2fd	: fcntlcmd = 10		/* duplicate file descriptor to arg */
const Fgetlk	: fcntlcmd = 11		/* get record locking information */
const Fsetlk	: fcntlcmd = 12		/* set record locking information */
const Fsetlkw	: fcntlcmd = 13		/* F_SETLK; wait if blocked */
const Fsetlk_remote	: fcntlcmd = 14		/* debugging support for remote locks */
const Freadahead	: fcntlcmd = 15		/* read ahead */
const Frdahead	: fcntlcmd = 16		/* Darwin compatible read ahead */
const Fdupfd_cloexec	: fcntlcmd = 17		/* Like F_DUPFD, but FD_CLOEXEC is set */
const Fdup2fd_cloexec	: fcntlcmd = 18		/* Like F_DUP2FD, but FD_CLOEXEC is set */

/* return value for a failed mapping */
const Mapbad	: byte# = (-1 : byte#)

/* umtx ops */
const Umtxlock	: umtxop = 0
const Umtxunlock	: umtxop = 1
const Umtxwait	: umtxop = 2
const Umtxwake	: umtxop = 3
const UmtxmtxTrylock	: umtxop = 4
const Umtxmtxlock	: umtxop = 5
const Umtxmtxunlock	: umtxop = 6
const Umtxsetceiling	: umtxop = 7
const Umtxcvwait	: umtxop = 8
const Umtxcvsignal	: umtxop = 9
const Umtxcvbroadcast	: umtxop = 10
const Umtxwaituint	: umtxop = 11
const Umtxrwrdlock	: umtxop = 12
const Umtxrwwrlock	: umtxop = 13
const Umtxrwunlock	: umtxop = 14
const Umtxwaituintpriv	: umtxop = 15
const Umtxwakepriv	: umtxop = 16
const Umtxmutexwait	: umtxop = 17
const Umtxsemwait	: umtxop = 19
const Umtxsemwake	: umtxop = 20
const Umtxnwakepriv	: umtxop = 21
const Umtxmtxwake2	: umtxop = 22
const Umtxmax	: umtxop = 23

/* signal actions */
const Saonstack		: sigflags = 0x0001	/* take signal on signal stack */
const Sarestart		: sigflags = 0x0002	/* restart system call on signal return */
const Saresethand	: sigflags = 0x0004	/* reset to SIG_DFL when taking signal */
const Sanodefer		: sigflags = 0x0010	/* don't mask the signal we're delivering */
const Sanocldwait	: sigflags = 0x0020	/* don't keep zombies around */
const Sasiginfo		: sigflags = 0x0040	/* signal handler with SA_SIGINFO args */

/* signal numbers */
const Sighup	: signo = 1	/* hangup */
const Sigint	: signo = 2	/* interrupt */
const Sigquit	: signo = 3	/* quit */
const Sigill	: signo = 4	/* illegal instr. (not reset when caught) */
const Sigtrap	: signo = 5	/* trace trap (not reset when caught) */
const Sigabrt	: signo = 6	/* abort() */
const Sigiot	: signo = Sigabrt	/* compatibility */
const Sigemt	: signo = 7	/* EMT instruction */
const Sigfpe	: signo = 8	/* floating point exception */
const Sigkill	: signo = 9	/* kill (cannot be caught or ignored) */
const Sigbus	: signo = 10	/* bus error */
const Sigsegv	: signo = 11	/* segmentation violation */
const Sigsys	: signo = 12	/* non-existent system call invoked */
const Sigpipe	: signo = 13	/* write on a pipe with no one to read it */
const Sigalrm	: signo = 14	/* alarm clock */
const Sigterm	: signo = 15	/* software termination signal from kill */
const Sigurg	: signo = 16	/* urgent condition on IO channel */
const Sigstop	: signo = 17	/* sendable stop signal not from tty */
const Sigtstp	: signo = 18	/* stop signal from tty */
const Sigcont	: signo = 19	/* continue a stopped process */
const Sigchld	: signo = 20	/* to parent on child stop or exit */
const Sigttin	: signo = 21	/* to readers pgrp upon background tty read */
const Sigttou	: signo = 22	/* like TTIN if (tp->t_local&LTOSTOP) */
const Sigio	: signo = 23	/* input/output possible signal */
const Sigxcpu	: signo = 24	/* exceeded CPU time limit */
const Sigxfsz	: signo = 25	/* exceeded file size limit */
const Sigvtalrm	: signo = 26	/* virtual time alarm */
const Sigprof	: signo = 27	/* profiling time alarm */
const Sigwinch	: signo = 28	/* window size changes */
const Siginfo	: signo = 29	/* information request */
const Sigusr1	: signo = 30	/* user defined signal 1 */
const Sigusr2	: signo = 31	/* user defined signal 2 */
const Sigthr	: signo = 32	/* reserved by thread library. */
const Siglwp	: signo = Sigthr
const Siglibrt	: signo = 33	/* reserved by real-time library. */

/* sysarch ops */
const Archamd64getfs   : sysarchop = 128
const Archamd64setfs   : sysarchop = 129
const Archamd64getgs   : sysarchop = 130
const Archamd64setgs   : sysarchop = 131
const Archamd64getxfpu : sysarchop = 131

extern const syscall : (sc:scno, args:... -> int64)
extern var __cenvp : byte##
