#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[512];
int nflag = 0, bflag = 0, Eflag = 0, sflag = 0;

// Process one file descriptor and output content according to flags
void cat(int fd) {
    int n;
    int line = 1;      // current line number
    int start = 1;     // ready to print line number
    int blank_run = 0; // count consecutive blank lines

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            char c = buf[i];

            if (start) {
                // Decide whether to print line number
                int print_num = 0;
                if (nflag) print_num = 1;               // -n : number all lines
                if (bflag) print_num = (c != '\n');     // -b : number nonempty lines only
                if (print_num) {
                    // manual alignment for up to 3-digit line numbers
                    if (line < 10)       printf("  %d  ", line++);
                    else if (line < 100) printf(" %d  ", line++);
                    else                 printf("%d  ", line++);
                }
                start = 0;
            }

            // -s : squeeze multiple blank lines
            if (sflag && c == '\n') {
                if (blank_run) continue; // skip extra blank lines
                blank_run = 1;
            } else {
                blank_run = 0;
            }

            // -E : show $ at end of each line
            if (Eflag && c == '\n')
                write(1, "$", 1);

            // output character
            write(1, &c, 1);

            if (c == '\n')
                start = 1; // next char starts a new line
        }
    }

    if (n < 0) {
        fprintf(2, "cat: read error\n");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    int i = 1;

    // Parse command-line options
    while (i < argc && argv[i][0] == '-') {
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'n') nflag = 1;
            else if (*p == 'b') bflag = 1;
            else if (*p == 'E') Eflag = 1;
            else if (*p == 's') sflag = 1;
            else {
                fprintf(2, "cat: unknown option -%c\n", *p);
                exit(1);
            }
        }
        i++;
    }

    // No file arguments -> read from stdin
    if (i == argc) {
        cat(0);
        exit(0);
    }

    // Process each file argument
    for (; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(2, "cat: cannot open %s\n", argv[i]);
            exit(1);
        }
        cat(fd);
        close(fd);
    }
    exit(0);
}
