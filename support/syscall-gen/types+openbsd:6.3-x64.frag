type size	= int64	/* spans entire address space */
type usize	= uint64	/* unsigned size */
type off	= int64	/* file offsets */
type intptr	= uint64/* can hold any pointer losslessly */
type time	= int64	/* milliseconds since epoch */
type pid	= int32	/* process id */
type scno	= int64	/*syscall*/
type fdopt	= int64	/* fd options */
type fd		= int32	/* fd */
type whence	= uint64	/* seek from whence */
type mprot	= int64	/* memory protection */
type mopt	= int64	/* memory mapping options */
type socktype	= int64	/* socket type */
type sockproto	= int64	/* socket protocol */
type sockopt	= int32	/* socket option */
type sockfam	= uint8	/* socket family */
type filemode	= uint32
type filetype	= uint8
type fcntlcmd	= int64
type signo	= int32
type sigflags	= int32
type sigset	= uint32
type msg	= void
type gid	= uint32

const Futexwait		: int = 1
const Futexwake		: int = 2
const Futexrequeue	: int = 3


type clock = union
	`Clockrealtime
	`Clockmonotonic
	`Clockproccputime
	`Clockthreadcputime
	`Clockuptime
;;

type waitstatus = union
	`Waitfail int32
	`Waitexit int32
	`Waitsig  int32
	`Waitstop int32
;;

type rlimit = struct
	cur	: uint64	/* current (soft) limit */
	max	: uint64	/* maximum value for rlim_cur */
;;

type timespec = struct
	sec	: int64
	nsec	: int64
;;

type timeval = struct
	sec	: int64
	usec	: int64
;;

type timezone = struct
	minwest	: int32	/* minutes west of Greenwich */
	dsttime	: int32	/* type of dst correction */
;;

type pollfd = struct
	fd      : fd
	events  : uint16
	revents : uint16
;;

type itimerval = struct
	interval	: timeval	/* timer interval */
	value		: timeval	/* current value */
;;

type sigaction = struct
	handler	: byte#	/* code pointer */
	mask	: sigset
	flags	: sigflags
;;
/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 */
type sigcontext = struct
	/* plain match trapframe */
	rdi	: int64
	rsi	: int64
	rdx	: int64
	rcx	: int64
	r8	: int64
	r9	: int64
	r10	: int64
	r11	: int64
	r12	: int64
	r13	: int64
	r14	: int64
	r15	: int64
	rbp	: int64
	rbx	: int64
	rax	: int64
	gs	: int64
	fs	: int64
	es	: int64
	ds	: int64
	trapno	: int64
	err	: int64
	rip	: int64
	cs	: int64
	rflags	: int64
	rsp	: int64
	ss	: int64

	fpstate	: fxsave64#
	__pad	: int32
	mask	: int32
	cookie	: int64
;;

type sigaltstack = struct
	sp	: void#
	size	: size
	flags	: int32
;;

type fxsave64 = struct
	fcw	: int16
	fsw	: int16
	ftw	: int8
	unused1	: int8
	fop	: int16
	rip	: int64
	rdp	: int64
	mxcsr	: int32
	mxcsrmask	: int32
	st	: int64[8][2]   /* 8 normal FP regs */
	xmm	: int64[16][2] /* 16 SSE2 registers */
	unused3	: int8[96]
;;

const Simaxsz	= 128
const Sipad	= (Simaxsz / 4) - 3
type siginfo = struct
	signo	: int
	code	: int
	errno	: int
	pad	: int[Sipad]
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

type tforkparams = struct
	tcb	: void#
	tid	: pid#
	stk	: byte#
;;

type statbuf = struct
	mode	: filemode
	dev	: uint32 
	ino	: uint64
	nlink	: uint32
	uid	: uint32
	gid	: uint32
	rdev	: uint32
	atime	: timespec
	mtime	: timespec
	ctime	: timespec
	size	: off
	blocks	: int64
	blksize	: uint32
	flags	: uint32
	gen	: uint32
	birthtim	: timespec 
;;

type semun = struct
	semarr	: void#
;;

const Mfsnamelen 	= 16	/* length of fs type name, including nul */
const Mnamelen		= 90	/* length of buffer for returned name */

type statfs = struct
	flags	: uint32	/* copy of mount flags */
	bsize	: uint32	/* file system block size */
	iosize	: uint32	/* optimal transfer block size */

		        	/* unit is f_bsize */
	blocks	: uint64	/* total data blocks in file system */
	bfree	: uint64	/* free blocks in fs */
	bavail	: int64		/* free blocks avail to non-superuser */

	files	: int64		/* total file nodes in file system */
	ffree	: int64		/* free file nodes in fs */
	favail	: int64		/* free file nodes avail to non-root */

	syncwr	: int64		/* count of sync writes since mount */
	syncrd	: int64		/* count of sync reads since mount */
	asyncwr	: int64		/* count of async writes since mount */
	asyncrd	: int64		/* count of async reads since mount */

	fsid	: fsid		/* file system id */
	namemax	: uint32	/* maximum filename length */
	owner	: uid		/* user that mounted the file system */
	ctime	: uint64	/* last mount [-u] time */

	fstypename	: byte[Mfsnamelen];	/* fs type name */
	mntonname	: byte[Mnamelen];	/* directory on which mounted */
	mntfromname	: byte[Mnamelen];	/* mounted file system */
	mntfromspec	: byte[Mnamelen];	/* special for mount request */
	///union mount_info mount_info;	/* per-filesystem mount options */
	__mountinfo	: byte[160];	/* storage for 'union mount_info' */
;;

type utsname = struct
	system	: byte[32]
	node : byte[32] 
	release : byte[32]
	version : byte[32]
	machine : byte[32]
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
	__pad1  : byte[6]
	__align : int64
	__pad2  : byte[240]
;;	

type dirent = struct
	fileno	: uint64
	off	: uint64
	reclen	: uint16
	ftype	: uint8
	namlen	: uint8
	__pad	: byte[4]
	name	: byte[256]	
;;

type iovec = struct
	base	: byte#
	len	: uint64
;;

/* open options */
const Ordonly  	: fdopt = 0x0
const Owronly  	: fdopt = 0x1
const Ordwr    	: fdopt = 0x2
const Oappend  	: fdopt = 0x8
const Ondelay  	: fdopt = 0x4
const Oshlock	: fdopt = 0x10		/* open with shared file lock */
const Oexlock	: fdopt = 0x20		/* open with exclusive file lock */
const Oasync	: fdopt = 0x40		/* signal pgrp when data ready */
const Osync	: fdopt = 0x80		/* backwards compatibility */
const Onofollow	: fdopt = 0x100
const Ocreat   	: fdopt = 0x200
const Otrunc   	: fdopt = 0x400
const Oexcl   	: fdopt = 0x800
const Ocloexec	: fdopt = 0x10000
const Odsync	: fdopt = Osync		/* synchronous data writes */
const Orsync	: fdopt = Osync		/* synchronous reads */
const Odir	: fdopt = 0x20000

/* poll options */
const Pollin	: uint16 = 0x0001
const Pollpri	: uint16 = 0x0002
const Pollout	: uint16 = 0x0004
const Pollerr	: uint16 = 0x0008
const Pollhup	: uint16 = 0x0010
const Pollnval	: uint16 = 0x0020
const Pollnorm	: uint16 = 0x0040
const Pollrdband: uint16 = 0x0080
const Pollwrband: uint16 = 0x0100

/* stat modes */	
const Sifmt	: filemode = 0xf000
const Sififo	: filemode = 0x1000
const Sifchr	: filemode = 0x2000
const Sifdir	: filemode = 0x4000
const Sifblk	: filemode = 0x6000
const Sifreg	: filemode = 0x8000
const Siflnk	: filemode = 0xa000
const Sifsock 	: filemode = 0xc000
const Sisvtx 	: filemode = 0x0200

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
const Mstack    : mopt = 0x4000
const Mnoreplace	: mopt = 0x0800

/* file types */
const Dtunknown	: filetype = 0
const Dtfifo	: filetype = 1
const Dtchr	: filetype = 2
const Dtdir	: filetype = 4
const Dtblk	: filetype = 6
const Dtreg	: filetype = 8
const Dtlnk	: filetype = 10
const Dtsock	: filetype = 12

/* socket families. INCOMPLETE. */
const Afunspec	: sockfam = 0
const Afunix	: sockfam = 1
const Afinet	: sockfam = 2
const Afinet6	: sockfam = 24

/* socket types. */
const Sockstream	: socktype = 1
const Sockdgram		: socktype = 2
const Sockraw		: socktype = 3
const Sockrdm		: socktype = 4
const Sockseqpacket	: socktype = 5

/* socket options */
const Sodebug		: sockopt = 0x0001	/* turn on debugging info recording */
const Soacceptconn	: sockopt = 0x0002	/* socket has had listen() */
const Soreuseaddr	: sockopt = 0x0004	/* allow local address reuse */
const Sokeepalive	: sockopt = 0x0008	/* keep connections alive */
const Sodontroute	: sockopt = 0x0010	/* just use interface addresses */
const Sobroadcast	: sockopt = 0x0020	/* permit sending of broadcast msgs */
const Souseloopback	: sockopt = 0x0040	/* bypass hardware when possible */
const Solinger		: sockopt = 0x0080	/* linger on close if data present */
const Sooobinline	: sockopt = 0x0100	/* leave received OOB data in line */
const Soreuseport	: sockopt = 0x0200	/* allow local address & port reuse */
const Sotimestamp	: sockopt = 0x0800	/* timestamp received dgram traffic */
const Sobindany		: sockopt = 0x1000	/* allow bind to any address */
const Sosndbuf		: sockopt = 0x1001	/* send buffer size */
const Sorcvbuf		: sockopt = 0x1002	/* receive buffer size */
const Sosndlowat	: sockopt = 0x1003	/* send low-water mark */
const Sorcvlowat	: sockopt = 0x1004	/* receive low-water mark */
const Sosndtimeo	: sockopt = 0x1005	/* send timeout */
const Sorcvtimeo	: sockopt = 0x1006	/* receive timeout */
const Soerror		: sockopt = 0x1007	/* get error status and clear */
const Sotype		: sockopt = 0x1008	/* get socket type */
const Sonetproc		: sockopt = 0x1020	/* multiplex; network processing */
const Sortable		: sockopt = 0x1021	/* routing table to be used */
const Sopeercred	: sockopt = 0x1022	/* get connect-time credentials */
const Sosplice		: sockopt = 0x1023	/* splice data to other socket */

/* socket option levels */
const Solsocket		: sockproto = 0xffff

/* network protocols */
const Ipproto_ip	: sockproto = 0
const Ipproto_icmp	: sockproto = 1
const Ipproto_tcp	: sockproto = 6
const Ipproto_udp	: sockproto = 17
const Ipproto_raw	: sockproto = 255

const Seekset	: whence = 0
const Seekcur	: whence = 1
const Seekend	: whence = 2

/* system specific constants */
const Maxpathlen	: size = 1024

/* fcntl constants */
const Fdupfd	 : fcntlcmd = 0		/* duplicate file descriptor */
const Fgetfd	 : fcntlcmd = 1		/* get file descriptor flags */
const Fsetfd	 : fcntlcmd = 2		/* set file descriptor flags */
const Fgetfl	 : fcntlcmd = 3		/* get file status flags */
const Fsetfl	 : fcntlcmd = 4		/* set file status flags */
const Fgetown	 : fcntlcmd = 5		/* get SIGIO/SIGURG proc/pgrp */
const Fsetown	 : fcntlcmd = 6		/* set SIGIO/SIGURG proc/pgrp */
const Fogetlk	 : fcntlcmd = 7		/* get record locking information */
const Fosetlk	 : fcntlcmd = 8		/* set record locking information */

/* return value for a failed mapping */
const Mapbad	: byte# = (-1 : byte#)

/* signal flags */
const Saonstack		: sigflags = 0x0001	/* take signal on signal stack */
const Sarestart		: sigflags = 0x0002	/* restart system on signal return */
const Saresethand	: sigflags = 0x0004	/* reset to SIG_DFL when taking signal */
const Sanodefer		: sigflags = 0x0010	/* don't mask the signal we're delivering */
const Sanocldwait	: sigflags = 0x0020	/* don't create zombies (assign to pid 1) */
const Sanocldstop	: sigflags = 0x0008	/* do not generate SIGCHLD on child stop */
const Sasiginfo		: sigflags = 0x0040	/* generate siginfo_t */

/* signals */
const Sighup	: signo = 1	/* hangup */
const Sigint	: signo = 2	/* interrupt */
const Sigquit	: signo = 3	/* quit */
const Sigill	: signo = 4	/* illegal instruction (not reset when caught) */
const Sigtrap	: signo = 5	/* trace trap (not reset when caught) */
const Sigabrt	: signo = 6	/* abort() */
const Sigiot	: signo = Sigabrt	/* compatibility */
const Sigemt	: signo = 7	/* EMT instruction */
const Sigfpe	: signo = 8	/* floating point exception */
const Sigkill	: signo = 9	/* kill (cannot be caught or ignored) */
const Sigbus	: signo = 10	/* bus error */
const Sigsegv	: signo = 11	/* segmentation violation */
const Sigsys	: signo = 12	/* bad argument to system call */
const Sigpipe	: signo = 13	/* write on a pipe with no one to read it */
const Sigalrm	: signo = 14	/* alarm clock */
const Sigterm	: signo = 15	/* software termination signal from kill */
const Sigurg	: signo = 16	/* urgent condition on IO channel */
const Sigstop	: signo = 17	/* sendable stop signal not from tty */
const Sigtstp	: signo = 18	/* stop signal from tty */
const Sigcont	: signo = 19	/* continue a stopped process */
const Sigchld	: signo = 20	/* to parent on child stop or exit */
const Sigttin	: signo = 21	/* to readers pgrp upon background tty read */
const Sigttou	: signo = 22	/* like TTIN for output if (tp->t_local&LTOSTOP) */
const Sigio	: signo = 23	/* input/output possible signal */
const Sigxcpu	: signo = 24	/* exceeded CPU time limit */
const Sigxfsz	: signo = 25	/* exceeded file size limit */
const Sigvtalrm : signo = 26	/* virtual time alarm */
const Sigprof	: signo = 27	/* profiling time alarm */
const Sigwinch	: signo = 28	/* window size changes */
const Siginfo	: signo = 29	/* information request */
const Sigusr1	: signo = 30	/* user defined signal 1 */
const Sigusr2	: signo = 31	/* user defined signal 2 */
const Sigthr	: signo = 32	/* thread library AST */

extern const syscall : (sc:scno, args:... -> int64)
extern var __cenvp : byte##
