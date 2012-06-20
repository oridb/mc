#if defined(__APPLE__) && defined(__MACH__)
/* for OSX */
#   define Assembler "as -arch i386 -g -o %s -"
#   define Fprefix "_"
#else
/* Default to linux */
#   define Assembler "as --32 -g -o %s -"
#   define Fprefix ""
#endif
