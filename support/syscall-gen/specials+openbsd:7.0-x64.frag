/* process control */
const exit	: (status:int -> void)
const getpid	: ( -> pid)
const kill	: (pid:pid, sig:int64 -> int64)
const fork	: (-> pid)
const wait4	: (pid:pid, loc:int32#, opt : int64, usage:rusage#	-> int64)
const waitpid	: (pid:pid, loc:int32#, opt : int64	-> int64)
const execv	: (cmd : byte[:], args : byte[:][:] -> int64)
const execve	: (cmd : byte[:], args : byte[:][:], env : byte[:][:] -> int64)
/* wrappers to extract wait status */
const waitstatus	: (st : int32 -> waitstatus)
extern const __tfork_thread	: (tfp : tforkparams#, sz : size, fn : void#, arg : void# -> pid)

/* fd manipulation */
const open	: (path:byte[:], opts:fdopt -> fd)
const openmode	: (path:byte[:], opts:fdopt, mode:int64 -> fd)
const close	: (fd:fd -> int64)
const creat	: (path:byte[:], mode:int64 -> fd)
const unlink	: (path:byte[:] -> int)
const read	: (fd:fd, buf:byte[:] -> size)
const pread	: (fd:fd, buf:byte[:], off : off -> size)
const readv	: (fd:fd, iov:iovec[:] -> size)
const write	: (fd:fd, buf:byte[:] -> size)
const pwrite	: (fd:fd, buf:byte[:], off : off -> size)
const writev	: (fd:fd, iov:iovec[:] -> size)
const lseek	: (fd:fd, off : off, whence : whence -> int64)
const stat	: (path:byte[:], sb:statbuf# -> int64)
const lstat	: (path:byte[:], sb:statbuf# -> int64)
const fstat	: (fd:fd, sb:statbuf# -> int64)
const mkdir	: (path : byte[:], mode : int64	-> int64)
generic ioctl	: (fd:fd, req : int64, arg:@a# -> int64)
const getdents	: (fd : fd, buf : byte[:] -> int64)
const chdir	: (p : byte[:] -> int64)
const __getcwd	: (buf : byte[:] -> int64)
const poll	: (pfd : pollfd[:], tm : int -> int)

/* signals */
const sigaction	: (sig : signo, act : sigaction#, oact : sigaction# -> int)
const sigprocmask	: (how : int32, set : sigset#, oset : sigset# -> int)

/* fd stuff */
const pipe	: (fds : fd[2]# -> int64)
const dup	: (fd : fd -> fd)
const dup2	: (src : fd, dst : fd -> fd)
/* NB: the C ABI uses '...' for the args. */
const fcntl	: (fd : fd, cmd : fcntlcmd, args : byte# -> int64)

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

/* time - doublecheck if this is right */
const clock_getres	: (clk : clock, ts : timespec# -> int32)
const clock_gettime	: (clk : clock, ts : timespec# -> int32)
const clock_settime	: (clk : clock, ts : timespec# -> int32)
const sleep	: (time : uint64 -> int32)
const nanosleep	: (req : timespec#, rem : timespec# -> int32)

/* system information */
const uname 	: (buf : utsname# -> int)
const sysctl	: (mib : int[:], \
		old : void#, oldsz : size#, \
		new : void#, newsz : size# \
		-> int)

/* 
wraps a syscall argument, converting it to 64 bits for the syscall function. This is
the same as casting, but more concise than writing a cast to uint64
*/
generic a = {x : @t; -> (x : uint64)}

extern const cstring	: (str : byte[:] -> byte#)
extern const alloca	: (sz : size	-> byte#)

extern const __freebsd_pipe	: (fds : fd[2]# -> int64)

/* process management */
const exit	= {status;		syscall(Sysexit, a(status))}
const getpid	= {;			-> (syscall(Sysgetpid, 1) : pid)}
const kill	= {pid, sig;		-> syscall(Syskill, pid, sig)}
const fork	= {;			-> (syscall(Sysfork) : pid)}
const wait4	= {pid, loc, opt, usage;	-> syscall(Syswait4, pid, loc, opt, usage)}
const waitpid	= {pid, loc, opt;
	-> wait4(pid, loc, opt, (0 : rusage#)) 
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
	var p

	/* copy the args */
	p = alloca((args.len + 1)*sizeof(byte#))
	cargs = (p : byte##)[:args.len]
	for i = 0; i < args.len; i++
		cargs[i] = cstring(args[i])
	;;
	cargs[args.len] = (0 : byte#)

	/*
	 copy the env.
	 of course we fucking have to duplicate this code everywhere,
	 since we want to stack allocate... 
	*/
	p = alloca((env.len + 1)*sizeof(byte#))
	cenv = (p : byte##)[:env.len]
	for i = 0; i < env.len; i++
		cenv[i] = cstring(env[i])
	;;
	cenv[env.len] = (0 : byte#)

	-> syscall(Sysexecve, cstring(cmd), a(p), a(cenv))
}

/* fd manipulation */
const open	= {path, opts;		-> (syscall(Sysopen, cstring(path), a(opts), a(0o777)) : fd)}
const openmode	= {path, opts, mode;	-> (syscall(Sysopen, cstring(path), a(opts), a(mode)) : fd)}
const close	= {fd;			-> syscall(Sysclose, a(fd))}
const creat	= {path, mode;		-> (openmode(path, Ocreat | Otrunc | Owronly, mode) : fd)}
const unlink	= {path;		-> (syscall(Sysunlink, cstring(path)) : int)}
const read	= {fd, buf;		-> (syscall(Sysread, a(fd), (buf : byte#), a(buf.len)) : size)}
const pread	= {fd, buf, off;	-> (syscall(Syspread, a(fd), (buf : byte#), a(buf.len), a(off)) : size)}
const readv	= {fd, vec;		-> (syscall(Sysreadv, a(fd), (vec : iovec#), a(vec.len)) : size)}
const write	= {fd, buf;		-> (syscall(Syswrite, a(fd), (buf : byte#), a(buf.len)) : size)}
const pwrite	= {fd, buf, off;	-> (syscall(Syspwrite, a(fd), (buf : byte#), a(buf.len), a(off)) : size)}
const writev	= {fd, vec;		-> (syscall(Syswritev, a(fd), (vec : iovec#), a(vec.len)) : size)}
const lseek	= {fd, off, whence;	-> syscall(Syslseek, a(fd), a(off), a(whence))}
const stat	= {path, sb;		-> syscall(Sysstat, cstring(path), a(sb))}
const lstat	= {path, sb;		-> syscall(Syslstat, cstring(path), a(sb))}
const fstat	= {fd, sb;		-> syscall(Sysfstat, a(fd), a(sb))}
const mkdir	= {path, mode;		-> (syscall(Sysmkdir, cstring(path), a(mode)) : int64)}
generic ioctl	= {fd, req, arg;	-> (syscall(Sysioctl, a(fd), a(req), a(arg)) : int64)}
const chdir	= {dir;	-> syscall(Syschdir, cstring(dir))}
const __getcwd	= {buf;	-> syscall(Sys__getcwd, a(buf), a(buf.len))}
const getdents	= {fd, buf;		-> (syscall(Sysgetdents, a(fd), a(buf), a(buf.len)) : int64)}

/* signals */
const sigaction	= {sig, act, oact;	-> (syscall(Syssigaction, a(sig), a(act), a(oact)) : int)}
const sigprocmask	= {sig, act, oact;	-> (syscall(Syssigprocmask, a(sig), a(act), a(oact)) : int)}

/* file stuff */
const pipe	= {fds;	-> syscall(Syspipe, fds)}
const dup 	= {fd;	-> (syscall(Sysdup, a(fd)) : fd)}
const dup2 	= {src, dst;	-> (syscall(Sysdup2, a(src), a(dst)) : fd)}
const fcntl	= {fd, cmd, args; 	-> syscall(Sysfcntl, a(fd), a(cmd), a(args))}
const poll      = {pfd, tm;     -> (syscall(Syspoll, (pfd : byte#), a(pfd.len), a(tm)) : int)}

/* networking */
const socket	= {dom, stype, proto;	-> (syscall(Syssocket, a(dom), a(stype), a(proto)) : fd)}
const connect	= {sock, addr, len;	-> (syscall(Sysconnect, a(sock), a(addr), a(len)) : int)}
const accept	= {sock, addr, len;	-> (syscall(Sysaccept, a(sock), a(addr), a(len)) : fd)}
const listen	= {sock, backlog;	-> (syscall(Syslisten, a(sock), a(backlog)) : int)}
const bind	= {sock, addr, len;	-> (syscall(Sysbind, a(sock), a(addr), a(len)) : int)}
const setsockopt	= {sock, lev, opt, val, len;	-> (syscall(Syssetsockopt, a(sock), a(lev), a(opt), a(val), a(len)) : int)}
const getsockopt	= {sock, lev, opt, val, len;	-> (syscall(Syssetsockopt, a(sock), a(lev), a(opt), a(val), a(len)) : int)}

/* memory management */
const munmap	= {addr, len;		-> syscall(Sysmunmap, a(addr), a(len))}
const mmap	= {addr, len, prot, flags, fd, off;
	/* the actual syscall has padding on the offset arg */
	-> (syscall(Sysmmap, a(addr), a(len), a(prot), a(flags), a(fd), a(0), a(off)) : byte#)
}

/* time */
const clock_getres = {clk, ts;	-> (syscall(Sysclock_getres, clockid(clk), a(ts)) : int32)}
const clock_gettime = {clk, ts;	-> (syscall(Sysclock_gettime, clockid(clk), a(ts)) : int32)}
const clock_settime = {clk, ts;	-> (syscall(Sysclock_settime, clockid(clk), a(ts)) : int32)}

const sleep = {time
	var req, rem
	req = [.sec = (time : int64), .nsec = 0]
	-> nanosleep(&req, &rem)
}

const nanosleep	= {req, rem;	-> (syscall(Sysnanosleep, a(req), a(rem)) : int32)}


/* system information */
const uname	= {buf
	var mib : int[2]
	var ret
	var sys, syssz
	var nod, nodsz
	var rel, relsz
	var ver, versz
	var mach, machsz

	ret = 0
	mib[0] = 1 /* CTL_KERN */
	mib[1] = 1 /* KERN_OSTYPE */
	sys = (buf.system[:] : void#)
	syssz = buf.system.len
	ret = sysctl(mib[:], sys, &syssz, (0 : void#), (0 : size#))
	if ret < 0
		-> ret
	;;

	mib[0] = 1 /* CTL_KERN */
	mib[1] = 10 /* KERN_HOSTNAME */
	nod = (buf.node[:] : void#)
	nodsz = buf.node.len
	ret = sysctl(mib[:], nod, &nodsz, (0 : void#), (0 : size#))
	if ret < 0
		-> ret
	;;

	mib[0] = 1 /* CTL_KERN */
	mib[1] = 2 /* KERN_OSRELEASE */
	rel = (buf.release[:] : void#)
	relsz = buf.release.len
	ret = sysctl(mib[:], rel, &relsz, (0 : void#), (0 : size#))
	if ret < 0
		-> ret
	;;

	mib[0] = 1 /* CTL_KERN */
	mib[1] = 27 /* KERN_OSVERSION */
	ver = (buf.version[:] : void#)
	versz = buf.version.len
	ret = sysctl(mib[:], ver, &versz, (0 : void#), (0 : size#))
	if ret < 0
		-> ret
	;;

	mib[0] = 6 /* CTL_HW */
	mib[1] = 1 /* HW_MACHINE */
	mach = (buf.machine[:] : void#)
	machsz = buf.machine.len
	ret = sysctl(mib[:], mach, &machsz, (0 : void#), (0 : size#))
	if ret < 0
		-> ret
	;;

	-> 0
}

const sysctl = {mib, old, oldsz, new, newsz
	/* all args already passed through a() or ar  ptrs */
	-> (syscall(Syssysctl, \
		(mib : int#), a(mib.len), old, oldsz, new, newsz) : int)
}

const clockid = {clk
	match clk
	| `Clockrealtime:	-> 0
	| `Clockproccputime:	-> 2
	| `Clockmonotonic:	-> 3
	| `Clockthreadcputime:	-> 4
	| `Clockuptime:	-> 5
	;;
	-> -1
}

const waitstatus = {st
	if st < 0
		-> `Waitfail st
	;;
	match st & 0o177
	| 0:    -> `Waitexit (st >> 8)
	| 0x7f:-> `Waitstop (st >> 8)
	| sig:  -> `Waitsig sig
	;;
}

