MYRBIN=mbld

MYRSRC= \
	build.myr \
	clean.myr \
	config.myr \
	deps.myr \
	fsel.myr \
	install.myr \
	main.myr \
	opts.myr \
	parse.myr \
	subdir.myr \
	util.myr \
	types.myr

include config.mk
include mk/myr.mk

