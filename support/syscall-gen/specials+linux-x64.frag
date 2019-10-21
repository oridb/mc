/* getting to the os */
extern const syscall	: (sc:scno, args:... -> int64)
extern const sigreturn	: (-> void)

/* process management */
const exit	: (status:int -> void)
const exit_group	: (status:int -> void)
const getpid	: ( -> pid)
const kill	: (pid:pid, sig:int64 -> int64)
const fork	: (-> pid)
/* FIXME: where the fuck is 'struct pt_reg' defined?? */
const clone	: (flags : cloneopt, stk : byte#, ptid : pid#, ctid : pid#, ptreg : byte# -> pid)
extern const fnclone	: ( flags : cloneopt, \
			stk : byte#, \
			ptid : pid#, \
			tls : byte#, \
			ctid : pid#, \
			ptreg : byte#, \
			fn : void#  /* we need a raw pointer */ \
			-> pid)
const wait4	: (pid:pid, loc:int32#, opt : int64, usage:rusage#	-> int64)
const waitpid	: (pid:pid, loc:int32#, opt : int64	-> pid)
const execv	: (cmd : byte[:], args : byte[:][:] -> int64)
const execve	: (cmd : byte[:], args : byte[:][:], env : byte[:][:] -> int64)
/* wrappers to extract wait status */
const waitstatus	: (st : int32 -> waitstatus)

/* file manipulation */
const open	: (path:byte[:], opts:fdopt -> fd)
const openmode	: (path:byte[:], opts:fdopt, mode:int64 -> fd)
const close	: (fd:fd -> int64)
const rename	: (from : byte[:], to : byte[:] -> int64)
const creat	: (path:byte[:], mode:int64 -> fd)
const unlink	: (path:byte[:] -> int)
const readlink	: (path:byte[:], buf:byte[:] -> int64)
const read	: (fd:fd, buf:byte[:] -> size)
const pread	: (fd:fd, buf:byte[:], off : off -> size)
const write	: (fd:fd, buf:byte[:] -> size)
const pwrite	: (fd:fd, buf:byte[:], off : off -> size)
const lseek	: (fd:fd, off:off, whence:whence -> int64)
const stat	: (path:byte[:], sb:statbuf# -> int64)
const lstat	: (path:byte[:], sb:statbuf# -> int64)
const fstat	: (fd:fd, sb:statbuf# -> int64)
const mkdir	: (path : byte[:], mode : int64	-> int64)
generic ioctl	: (fd:fd, req : int64, arg:@a# -> int64)
const getdents64	: (fd:fd, buf : byte[:] -> int64)
const chdir	: (p : byte[:] -> int64)
const getcwd	: (buf : byte[:] -> int64)
const sendmsg	: (fd:fd, msg:msghdr#, flags:msgflags -> int64)
const recvmsg	: (fd:fd, msg:msghdr#, flags:msgflags -> int64)
const fallocate : (fd:fd, mode:fallocmode, off:off, len:off -> int64)
const memfdcreate       : (name:byte[:], flags:mfdflags -> fd)

/* signals */
const sigaction	: (sig : signo, act : sigaction#, oact : sigaction# -> int)
const sigprocmask	: (how : int32, set : sigset#, oset : sigset# -> int)

/* fd stuff */
const pipe	: (fds : fd[2]# -> int64)
const dup	: (fd : fd -> fd)
const dup2	: (src : fd, dst : fd -> fd)

/* threading */
const futex	: (uaddr : int32#, op : futexop, val : int32, \
	timeout : timespec#, uaddr2 : int32#, val3 : int32 -> int)
const semctl	:  (semid : int, semnum : int, cmd : int, arg : void# -> int)

/* polling */
const epollcreate	: (flg : epollflags	-> fd)	/* actually epoll_create1 */
const epollctl	: (epfd : fd, op : int, fd : fd, evt : epollevt# -> int)
const epollwait	: (epfd : fd, evts : epollevt[:], timeout : int -> int)
const poll	: (pfd	: pollfd[:], timeout : int	-> int)

/* networking */
const socket	: (dom : sockfam, stype : socktype, proto : sockproto	-> fd)
const connect	: (sock	: fd, addr : sockaddr#, len : size -> int)
const accept	: (sock : fd, addr : sockaddr#, len : size# -> fd)
const listen	: (sock : fd, backlog : int	-> int)
const bind	: (sock : fd, addr : sockaddr#, len : size -> int)
const setsockopt	: (sock : fd, lev : sockproto, opt : sockopt, val : void#, len : size -> int)
const getsockopt	: (sock : fd, lev : sockproto, opt : sockopt, val : void#, len : size# -> int)

/* memory mapping */
const munmap	: (addr:byte#, len:size -> int64)
const mmap	: (addr:byte#, len:size, prot:mprot, flags:mopt, fd:fd, off:off -> byte#)

/* time */
const clock_getres	: (clk : clock, ts : timespec# -> int32)
const clock_gettime	: (clk : clock, ts : timespec# -> int32)
const clock_settime	: (clk : clock, ts : timespec# -> int32)
const nanosleep	: (req : timespec#, rem : timespec# -> int32)

/* user/group management */
const getuid : ( -> uint32)
const getgid : ( -> uint32)
const setuid : (uid : uint32 -> int32)
const setgid : (gid : uint32 -> int32)

/* system information */
const uname 	: (buf : utsname# -> int)

/*
wraps a syscall argument, converting it to 64 bits for the syscall function.
This is the same as casting, but more concise than writing a cast to int64.
*/
generic a = {x : @t; -> (x : uint64)}

/* asm stubs from util.s */
extern const cstring	: (str : byte[:] -> byte#)
extern const alloca	: (sz : size	-> byte#)

/* process management */
const exit	= {status;		syscall(Sysexit, a(status))}
const exit_group	= {status;	syscall(Sysexit_group, a(status))}
const getpid	= {;			-> (syscall(Sysgetpid) : pid)}
const kill	= {pid, sig;		-> syscall(Syskill, a(pid), a(sig))}
const fork	= {;			-> (syscall(Sysfork) : pid)}
const clone	= {flags, stk, ptid, ctid, ptreg;	-> (syscall(Sysclone, a(flags), a(stk), a(ptid), a(ctid), a(ptreg)) : pid)}
const wait4	= {pid, loc, opt, usage;	-> syscall(Syswait4, a(pid), a(loc), a(opt), a(usage))}
const waitpid	= {pid, loc, opt;
	var rusage
	-> (wait4(pid, loc, opt, &rusage) : pid)
}

const execv	= {cmd, args
	var p, cargs, i

	/* of course we fucking have to duplicate this code everywhere,
	* since we want to stack allocate... */
	p = alloca((args.len + 1)*sizeof(byte#))
	cargs = (p : byte##)[:args.len + 1]
	for i = 0; i < args.len; i++
		cargs[i] = cstring(args[i])
	;;
	cargs[args.len] = (0 : byte#)
	-> syscall(Sysexecve, cstring(cmd), a(p), a(__cenvp))
}

const execve	= {cmd, args, env
	var cargs, cenv, i
	var ap, ep

	/* copy the args */
	ap = alloca((args.len + 1)*sizeof(byte#))
	cargs = (ap : byte##)[:args.len + 1]
	for i = 0; i < args.len; i++
		cargs[i] = cstring(args[i])
	;;
	cargs[args.len] = (0 : byte#)

	/*
	 copy the env.
	 of course we fucking have to duplicate this code everywhere,
	 since we want to stack allocate...
	*/
	ep = alloca((env.len + 1)*sizeof(byte#))
	cenv = (ep : byte##)[:env.len]
	for i = 0; i < env.len; i++
		cenv[i] = cstring(env[i])
	;;
	cenv[env.len] = (0 : byte#)

	-> syscall(Sysexecve, cstring(cmd), a(ap), a(ep))
}

/* file manipulation */
const open	= {path, opts;		-> (syscall(Sysopen, cstring(path), a(opts), a(0o777)) : fd)}
const openmode	= {path, opts, mode;	-> (syscall(Sysopen, cstring(path), a(opts), a(mode)) : fd)}
const close	= {fd;			-> syscall(Sysclose, a(fd))}
const creat	= {path, mode;		-> (syscall(Syscreat, cstring(path), a(mode)) : fd)}
const rename	= {from, to;		-> syscall(Sysrename, cstring(from), cstring(to))}
const unlink	= {path;		-> (syscall(Sysunlink, cstring(path)) : int)}
const readlink	= {path, buf; -> syscall(Sysreadlink, cstring(path), (buf : byte#), a(buf.len))}
const read	= {fd, buf;		-> (syscall(Sysread, a(fd), (buf : byte#), a(buf.len)) : size)}
const pread	= {fd, buf, off;	-> (syscall(Syspread, a(fd), (buf : byte#), a(buf.len), a(off)) : size)}
const write	= {fd, buf;		-> (syscall(Syswrite, a(fd), (buf : byte#), a(buf.len)) : size)}
const pwrite	= {fd, buf, off;	-> (syscall(Syspwrite, a(fd), (buf : byte#), a(buf.len), a(off)) : size)}
const lseek	= {fd, off, whence;	-> syscall(Syslseek, a(fd), a(off), a(whence))}
const stat	= {path, sb;		-> syscall(Sysstat, cstring(path), a(sb))}
const lstat	= {path, sb;		-> syscall(Syslstat, cstring(path), a(sb))}
const fstat	= {fd, sb;		-> syscall(Sysfstat, a(fd), a(sb))}
const mkdir	= {path, mode;		-> (syscall(Sysmkdir, cstring(path), a(mode)) : int64)}
generic ioctl	= {fd, req, arg;	-> (syscall(Sysioctl, a(fd), a(req), a(arg)) : int64)}
const getdents64	= {fd, buf;	-> syscall(Sysgetdents64, a(fd), (buf : byte#), a(buf.len))}
const chdir	= {dir;	-> syscall(Syschdir, cstring(dir))}
const getcwd	= {buf;	-> syscall(Sysgetcwd, a(buf), a(buf.len))}
const sendmsg	= {fd, msg, flags;	-> syscall(Syssendmsg, a(fd), msg, a(flags))}
const recvmsg	= {fd, msg, flags;	-> syscall(Sysrecvmsg, a(fd), msg, a(flags))}
const fallocate	= {fd, mode, off, len;	-> syscall(Sysfallocate, a(fd), a(mode),  a(off), a(len))}
const memfdcreate      = {name, flags; -> (syscall(Sysmemfd_create, cstring(name), a(flags)) : fd)}

/* file stuff */
const pipe	= {fds;	-> syscall(Syspipe, a(fds))}
const dup 	= {fd;	-> (syscall(Sysdup, a(fd)) : fd)}
const dup2 	= {src, dst;	-> (syscall(Sysdup2, a(src), a(dst)) : fd)}

const sigaction	= {sig, act, oact;
	if act.restore == (0 : byte#)
		act.flags |= Sarestorer
		act.restore = (sys.sigreturn : byte#)
	;;
	-> (syscall(Sysrt_sigaction, a(sig), a(act), a(oact), a(sizeof(sigflags))) : int)
}
const sigprocmask	= {sig, act, oact;	-> (syscall(Sysrt_sigprocmask, a(sig), a(act), a(oact), a(sizeof(sigflags))) : int)}

/* threading */
const futex	= {uaddr, op, val, timeout, uaddr2, val3
	-> (syscall(Sysfutex, a(uaddr), a(op), a(val), a(timeout), a(uaddr2), a(val3)) : int)
}
const semctl	= {semid, semnum, cmd, arg
	-> (syscall(Syssemctl, a(semnum), a(cmd), a(arg)) : int)
}

/* poll */
const poll	= {pfd, timeout;	-> (syscall(Syspoll, (pfd : pollfd#), a(pfd.len), a(timeout)) : int)}
const epollctl	= {epfd, op, fd, evt;
	-> (syscall(Sysepoll_ctl, a(epfd), a(op), a(fd), a(evt)) : int)}
const epollwait	= {epfd, evts, timeout;
	-> (syscall(Sysepoll_wait, a(epfd), (evts : epollevt#), a(evts.len), a(timeout)) : int)}
const epollcreate	= {flg;	-> (syscall(Sysepoll_create1, a(flg)) : fd)}

/* networking */
const socket	= {dom, stype, proto;	-> (syscall(Syssocket, a(dom), a(stype), a(proto)) : fd)}
const connect	= {sock, addr, len;	-> (syscall(Sysconnect, a(sock), a(addr), a(len)) : int)}
const bind	= {sock, addr, len;	-> (syscall(Sysbind, a(sock), a(addr), a(len)) : int)}
const listen	= {sock, backlog;	-> (syscall(Syslisten, a(sock), a(backlog)) : int)}
const accept	= {sock, addr, lenp;	-> (syscall(Sysaccept, a(sock), a(addr), a(lenp)) : fd)}
const setsockopt	= {sock, lev, opt, val, len;	-> (syscall(Syssetsockopt, a(sock), a(lev), a(opt), a(val), a(len)) : int)}
const getsockopt	= {sock, lev, opt, val, len;	-> (syscall(Syssetsockopt, a(sock), a(lev), a(opt), a(val), a(len)) : int)}

/* memory mapping */
const munmap	= {addr, len;		-> syscall(Sysmunmap, a(addr), a(len))}
const mmap	= {addr, len, prot, flags, fd, off;
	-> (syscall(Sysmmap, a(addr), a(len), a(prot), a(flags), a(fd), a(off)) : byte#)
}

/* time */
const clock_getres = {clk, ts;	-> (syscall(Sysclock_getres, clockid(clk), a(ts)) : int32)}
const clock_gettime = {clk, ts;	-> (syscall(Sysclock_gettime, clockid(clk), a(ts)) : int32)}
const clock_settime = {clk, ts;	-> (syscall(Sysclock_settime, clockid(clk), a(ts)) : int32)}
const nanosleep	= {req, rem;	-> (syscall(Sysnanosleep, a(req), a(rem)) : int32)}

/* user/group management */
const getuid = {; -> (syscall(Sysgetuid) : uint32)}
const getgid = {; -> (syscall(Sysgetgid) : uint32)}
const setuid = {uid; -> (syscall(Syssetuid, a(uid)) : int32)}
const setgid = {gid; -> (syscall(Syssetgid, a(gid)) : int32)}

/* system information */
const uname	= {buf;	-> (syscall(Sysuname, buf) : int)}

const clockid = {clk
	match clk
	| `Clockrealtime:	-> 0
	| `Clockmonotonic:	-> 1
	| `Clockproccpu:	-> 2
	| `Clockthreadcpu:	-> 3
	| `Clockmonotonicraw:	-> 4
	| `Clockrealtimecoarse:	-> 5
	| `Clockmonotoniccoarse:-> 6
	| `Clockboottime:	-> 7
	| `Clockrealtimealarm:	-> 8
	| `Clockboottimealarm:	-> 9
	;;
	-> -1
}


const waitstatus = {st
	if st & 0x7f == 0 /* if exited */
		-> `Waitexit ((st & 0xff00) >> 8)
	elif ((st & 0xffff)-1) < 0xff /* if signaled */
		-> `Waitsig ((st) & 0x7f)
	elif (((st & 0xffff)*0x10001)>>8) > 0x7f00
		-> `Waitstop ((st & 0xff00) >> 8)
	;;
	-> `Waitfail st	/* wait failed to give a result */
}
