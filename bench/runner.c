/* written in C instead of Myrddin because I don't have FP in Myrddin yet... */
#include <stdlib.h>
#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define Nsamp 10

double run(char *prog)
{
    struct rusage ru;
    double sec, usec;
    int in, out;
    char *cmd[2];
    int pid;
    int status;

    sec = 0;
    usec = 0;
    pid = fork();
    if (pid < 0) {
        err(1, "Could not fork\n");
    } else if (pid == 0) {
        if ((in = open("/dev/zero", O_RDONLY)) < 0)
            err(1, "could not open /dev/zero");
        if ((out = open("/dev/null", O_WRONLY)) < 0)
            err(1, "could not open /dev/null");
        if (dup2(in, 0) < 0)
            err(1, "could not reopen stdin");
        if (dup2(out, 1) < 0)
            err(1, "could not reopen stdout");

        cmd[0] = prog;
        cmd[1] = NULL;
        execv(prog, cmd);
        err(1, "Failed to exec\n");
    } else {
        wait4(pid, &status, 0, &ru);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            err(1, "Subprogram failed to execute\n");
        sec = ru.ru_utime.tv_sec;
        usec = ru.ru_utime.tv_usec / (1000.0 * 1000.0);
    }
    return sec + usec;
}

double timed_run(char *prog)
{
    double avg, m, d, x;
    int i, n;

    avg = 0;
    m = 0;
    n = 0;
    for (i = 0; i < Nsamp; i++) {
        n++;
        x = run(prog);
        d = (x - avg);
        avg += d/n;
        m = m + d*(x - avg);
    }
    printf("%s:\t%fs (σ^2: %f)\n", prog, avg, m/(n-1));
    return avg;
}

int main(int argc, char **argv)
{
    double tot;
    int i;

    printf("Running benchmarks: %d samples per binary\n", Nsamp);
    tot = 0;
    for (i = 1; i < argc; i++)
        tot += timed_run(argv[i]);
    printf("total:\t%fs\n", tot);
    return 0;
}

