#if defined(__APPLE__) && defined(__MACH__)
/* for OSX */
#   define Fprefix "_"
#else
/* Default to linux */
#   define Fprefix ""
#endif
#define Assembler "as --32 -g -o %s -"
