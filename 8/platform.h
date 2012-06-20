#if defined(__APPLE__) && defined(__MACH__)
/* for OSX */
#   define Asmcmd "as -arch i386 -g -o %s %s"
#   define Fprefix "_"
#else
/* Default to linux */
#   define Asmcmd "as --32 -g -o %s %s"
#   define Fprefix ""
#endif
