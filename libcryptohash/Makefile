MYRLIB=cryptohash
MYRSRC= \
	md5.myr \
	sha1.myr \
	sha256.myr \
	sha512.myr \
	# sha3.myr \

include config.mk
include mk/myr.mk

check: all
	make -C test check
