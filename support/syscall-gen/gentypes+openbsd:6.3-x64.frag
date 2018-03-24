type dev = int32
type uid = uint32
type fd_mask = uint32
type uintptr = uint64
type clockid = int32
type id = uint32
type key = int64
type shmatt = int16

type tfork = struct
	tcb	: void#
	tid	: pid#
	stack	: void#

;;

type msghdr = struct
	name	: void#
	namelen	: int32
	iov	: iovec#
	iovlen	: uint
	control	: void#
	controllen	: int32
	flags	: int

;;

type fsid = struct
	val	: int32[2]

;;

type fid = struct
	len	: uint16
	reserved	: uint16
	data	: byte[16]

;;

type fhandle = struct
	fsid	: fsid
	fid	: fid

;;

type fdset = struct
	bits	: fd_mask[32]

;;

type kevent = struct
	ident	: uintptr
	filter	: int16
	flags	: uint16
	fflags	: uint
	data	: int64
	udata	: void#

;;

type kbind = struct
	addr	: void#
	size	: size

;;

type sembuf = struct
	num	: uint16
	op	: int16
	flg	: int16

;;

type ipc_perm = struct
	cuid	: uid
	cgid	: gid
	uid	: uid
	gid	: gid
	mode	: filemode
	seq	: uint16
	key	: key

;;

type shmid_ds = struct
	perm	: ipc_perm
	segsz	: int
	lpid	: pid
	cpid	: pid
	nattch	: shmatt
	atime	: time
	atimensec	: int64
	dtime	: time
	dtimensec	: int64
	ctime	: time
	ctimensec	: int64
	internal	: void#

;;

type msqid_ds = struct
	perm	: ipc_perm
	first	: msg#
	last	: msg#
	cbytes	: uint64
	qnum	: uint64
	qbytes	: uint64
	lspid	: pid
	lrpid	: pid
	stime	: time
	pad1	: int64
	rtime	: time
	pad2	: int64
	ctime	: time
	pad3	: int64
	pad4	: int64[4]

;;

