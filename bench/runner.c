/* written in C instead of Myrddin because I don't have FP in Myrddin yet... */
#include <stdlib.h>
#include <stdio.h>

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
    char *cmd[2];
    int pid;
    int status;

    sec = 0;
    usec = 0;
    pid = fork();
    if (pid < 0) {
        err(1, "Could not fork\n");
    } else if (pid == 0) {
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

void timed_run(char *prog)
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
}

int main(int argc, char **argv)
{
    int i;

    printf("Running benchmarks: %d samples per binary\n", Nsamp);
    for (i = 1; i < argc; i++)
        timed_run(argv[i]);
    return 0;
}

