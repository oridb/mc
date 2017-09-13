BEGIN {
	sys = ARGV[1]
	split(sys, sp, ":")
	systype = sp[1]
	sysvers = sp[2]
	arch = ARGV[2]

	print "/*";
	print "  generated-ish source";
	print "  stitched for "sys" arch:"arch;
	print "  edit with caution.";
	print " */";
	ARGC=1
	masterfmt = 1
	if (systype =="freebsd") {
		Typefield = 3;
		Protofield = 4;
		renamings["sys_exit"] = "exit"
	} else if (systype == "openbsd") {
		Typefield = 2;
		Protofield = 3;
		strippfx="^sys_"
	} else if (systype == "netbsd") {
		Typefield = 2;
		Protofield = 4;
	} else if (systype == "linux") {
		Typefield = 0;
		Protofield = 4;
		strippfx="^(linux_|linux_new)"
		loadtbl()
		Protofield=2
		masterfmt = 0
	} else {
		print "unknown system " ARGV[1];
		exit 1;
	}
	# stop processing args as files.

	loadtypes()
	loadspecials()
}

NF == 0 || $1 ~ /^;/ || $1 ~ "^#" {
	next
}

END {
	printf("pkg sys =\n")
	for(i = 0; i < ntypes; i++)
		printf("\t%s\n", types[i])
	printf("\n")
	for(i = 0; i < nnums; i++)
		printf("\t%s\n", scnums[i])
	printf("\n")
	printf("\t/* start manual overrides { */\n");
	for(i = 0; i < nspecialdefs; i++)
		printf("\t%s\n", specialdefs[i])
	printf("\t/* } end manual overrides */\n");
	printf("\n")
	for(i = 0; i < nproto; i++)
		printf("\t%s\n", scproto[i])
	printf(";;\n\n")

	printf("\t/* start manual overrides { */\n");
	for(i = 0; i < nspecialbody; i++)
		printf("\t%s\n", specialbody[i])
	printf("\t/* } end manual overrides */\n");
	for(i = 0; i < ncall; i++)
		printf("%s\n", sccall[i])
}

$Typefield == "STD" || (!masterfmt && $1 == "asmlinkage") {
	if (!parse())
		next;
	numcst = "const Sys"name
	if (length(numcst) < 16)
		tabs="\t\t\t"
	else if (length(numcst) < 24)
		tabs="\t\t"
	else
		tabs = "\t"
	num = sprintf("%s%s: scno = %d", numcst, tabs, $1);
	scnums[nnums++] = num

	if (nocode)
		next;

	if (length("const ") + length(name) < 16)
		tabs="\t\t\t"
	else if (length("const ") + length(name) < 24)
		tabs="\t\t"
	else
		tabs = "\t"
	proto = sprintf("const %s%s:  (", name, tabs);
	sep="";
	for (i = 0; i < argc; i++) {
		proto = proto sprintf("%s%s : %s", sep, argname[i], argtype[i])
		sep = ", ";
	}
	proto = proto sprintf(" -> %s)", ret)
	scproto[nproto++] = proto

	call = sprintf("const %s\t= {", name);
	sep="";
	for (i = 0; i < argc; i++) {
		call = call sprintf("%s%s", sep, argname[i])
		sep = ", ";
	}
	call = call sprintf("\n\t -> (syscall(Sys%s", name)
	for (i = 0; i < argc; i++) {
		call = call sprintf(", a(%s)", argname[i])
	}
	call = call sprintf(") : %s)\n}", ret)
	sccall[ncall++] = call
}


function err(was, wanted) {
	printf("%s: line %d: unexpected %s (expected %s)\n",
		infile, NR, was, wanted);
	printf("\tnear %s\n", $0);
	exit 1;
}

function myrtype(t) {
	gsub(" *$", "", t)
	if (systype == "linux")
		gsub("^l_", "", t)

	if (t == "char")		t = "byte";
	if (t == "unsigned char")       t = "byte";
	if (t == "unsigned")	        t = "uint";
	if (t == "unsigned int")	t = "uint";
	if (t == "unsigned long")	t = "uint64";
	if (t == "long")		t = "int64";
	if (t == "u_char")		t = "byte";
	if (t == "u_int")		t = "uint";
	if (t == "u_long")		t = "uint64";
	if (t == "acl_type_t")		t = "acltype";
	if (t == "acl_tag_t")		t = "acltag";
	if (t == "acl_perm_t")		t = "aclperm";
	if (t == "acl_entry_type_t")	t = "aclenttype";
	if (t == "mode_t")		t = "filemode"
	if (t == "umode_t")		t = "filemode"
	if (t == "aio_context_t")	t = "aiocontext"
	if (t == "qid_t")		t = "int32"
	# myrddin sizes are signed
	if (t == "ssize_t")		t = "size";
	if (t == "__socklen_t")		t = "int32";
	if (t == "socklen_t")		t = "int32";
	if (t == "fd_set")		t = "fdset";
	if (t == "__ucontext")		t = "ucontext";
	if (t == "cap_rights_t")	t = "caprights";
	if (t == "stack_t")		t = "sigstack";
	if (t == "thr_param")		t = "thrparam";
	if (t == "nstat")		t = "statbuf";
	if (t == "stat")		t = "statbuf";
	if (t == "suseconds_t")		t = "int64";
	if (t == "caddr_t")		t = "void#";

	# linux stuff
	if (t == "u8")			t = "uint64";
	if (t == "u16")			t = "uint64";
	if (t == "u32")			t = "uint64";
	if (t == "u64")			t = "uint64";
	if (t == "qid_t")		t = "int32";
	if (t == "times_argv")		t = "tms";
	if (t == "newstat")		t = "statbuf";
	if (t == "stat64")		t = "statbuf";
	if (t == "new_utsname")		t = "utsname"
	if (t == "user_cap_header")	t = "capuserheader";
	if (t == "user_cap_data")	t = "capuserdata";
	if (t == "epoll_event")		t = "epollevt";
	if (t == "statfs_buf")		t = "statfs";
	if (t == "user_msghdr")		t = "int32"
	if (t == "msg")			t = "void#"
	if (t == "mqd_t")		t = "int"
	if (t == "bpf_attr")		t = "bpfgattr"
	if (t == "key_serial_t")	t = "int32"
	if (t == "linux_dirent64")	t = "dirent64"
	if (t == "linux_robust_list_head")
		t = "robust_list_head"

	gsub("_t$", "", t)
	gsub("^__", "", t)
	return t;
}

function cname(t) {
	# mostly to work around freebsd's linux syscalls.master
	# being a bit off in places.
	if (systype == "linux")
		gsub("^l_", "", t)
	if (t == "times_argv")		t = "tms";
	if (t == "newstat")		t = "stat";
	if (t == "statfs_buf")		t = "statfs";
	if (t == "linux_robust_list_head")
		t = "robust_list_head"

	return t;
}

function unkeyword(kw) {
	if (kw == "type")
		return "kind";
	if (kw == "in")
		return "_in";
	return kw;
}

function parse() {
	type = $Typefield;

	argc = 0
	nocode = 0
	if (!masterfmt) {
		end=NF
	} else if ($NF == "}") {
		rettype="int";
		end=NF;
	} else if ($(NF-1) == "}") {
		end=NF-1;
	} else {
		rettype=$NF;
		end=NF-3;
	}

	f = Protofield; 
	if (masterfmt) {
		numbertab[name] = $1;
		# openbsd puts flags after the kind of call
		if (systype == "openbsd" && $f != "{")
			f++;
		if ($f != "{")
			err($f, "{");
		f++
		if ($end != "}")
			err($f, "closing }");
		end--;
	}
	if ($end != ";")
		err($end, ";");
	end--;
	if ($end != ")")
		err($end, ")");
	end--;

	ret=myrtype($f);
	f++;
	if ($f == "*") {
		$f = $f"#";
		f++;
	}
	name=$f;
	if (strippfx)
		gsub(strippfx, "", name);
	if (!masterfmt && !renamings[name])
		return 0;
	if (renamings[name])
		name=renamings[name];
	if (special[name])
		nocode = 1
	f++;
	if ($f != "(")
		err($f, "(");
	f++; 
	if (f == end) {
		if ($f != "void")
			err($f, "argument definition")
		return 1;
	}

	while (f <= end) {
		argtype[argc]=""
		oldf=""
		while (f < end && $(f+1) != ",") {
			if ($f == "...")
				f++;
			if ($f == "const" || $f == "struct" ||
			    $f == "__restrict" || $f == "volatile" ||
			    $f == "union" || $f == "__user") {
				f++;
				continue;
			}
			if ($f == "*") {
				argtype[argc] = myrtype(argtype[argc]);
				$f="#"
			}
			if ($f == "#") {
				argtype[argc] = myrtype(argtype[argc])
			}
			if (argtype[argc] != "" && oldf != "*" && $f != "#" && $f != ",") {
				argtype[argc] = argtype[argc]" ";
			}
			argtype[argc] = argtype[argc]$f;
			if ($f != "#" && !knowntypes[myrtype($f)] && !wanttype[$f]) {
				t = cname($f)
				wanttype[t] = 1
				printf("%s\n", t) > "want.txt"
			}
			oldf = $f;
			f++
		}
		# unnamed protos: f(int)
		if (argtype[argc] == "" || $f == "*") {
			argname[argc] = "_a"argc
			if ($f == "*")
				argtype[argc] = myrtype(argtype[argc])"#"
			else
				argtype[argc] = myrtype($f)
		} else {
			argname[argc]=unkeyword($f);
		}
		argtype[argc] = myrtype(argtype[argc]);
		# skip name
		if ($f != ",")
			f++;
		# skip comma:
		f++;
		argc++
	}
	return 1;
}

function loadtypes() {
	path="types+"sys"-"arch".frag";
	while ((getline ln < path) > 0) {
		types[ntypes++] = ln;
		split(ln, a)
		if (a[1] == "type") {
			knowntypes[a[2]] = 1
			printf("%s\n", a[2]) > "have.txt"
		}
	}
	path="gentypes+"sys"-"arch".frag";
	while ((getline ln < path) > 0) {
		types[ntypes++] = ln;
		split(ln, a)
		if (a[1] == "type") {
			knowntypes[a[2]] = 1
			printf("%s\n", a[2]) > "have.txt"
		}
	}
}

function loadspecials() {
	path="specials+"sys"-"arch".frag";
	while ((getline ln < path) > 0) {
		while (match(ln, "\\\\[ \t]*$")) {
			getline ln2 < path
			ln = ln "\n" ln2
		}
		split(ln, sp)
		off = 0
		if (sp[off+1] == "extern")
			off++;
		if (sp[off+1] == "const" || sp[off+1] == "generic") {
			special[sp[off+2]] = 1
			if (sp[off+3] == ":")
				specialdefs[nspecialdefs++] = ln;
			else if (sp[3] == "=")
				specialbody[nspecialbody++] = ln;
			else
				err(sp[3], ": or =")
		} else {
			specialbody[nspecialbody++] = ln;
		}
	}
}

function loadtbl() {
	path = "syscalls+"sys"-"arch".tbl";
	while ((getline ln < path) > 0) {
		split(ln, sp);
		if (sp[2] != "common" && sp[2] != "64")
			continue;
		gsub("/.*$", "", sp[4])
		renamings[sp[4]] = sp[3];
		numbertab[sp[3]] = sp[1];
	}
	renamings["sys_mmap_pgoff"] = "mmap";
	renamings["sys_pread64"] = "pread";
	renamings["sys_pwrite64"] = "pwrite";
	renamings["sys_fadvise64"] = "fadvise";
	renamings["sys_prlimit64"] = "prlimit";
	# obsoletes
	renamings["sys_getdents"] = "";
}
