/* elog_guard_test.c: proves that a second writer on one log is refused, that the refusal
 * costs the first writer nothing, and that a log left by a finished run is still
 * reopenable, which is the boundary of the guard rather than an oversight.
 *
 * Why it exists. On Sunday 18 October 2026 a launch command was pasted twice and the
 * second engine truncated the first one's log; a whole run was lost. elog_open now takes
 * an exclusive advisory lock before it empties the file. A guard nobody has watched
 * refuse anything is not known to be a guard, which is this volume's own rule, so cases 2
 * and 3 below make it refuse and then measure what it protected.
 *
 * Five cases, in order, each depending on the state the last one left:
 *
 *   1  a fresh path opens and takes a row
 *   2  a second open of that path while the first is still held is REFUSED
 *   3  the first log's bytes are unchanged by the refusal. This is the regression. The
 *      call was fopen(path, "w") until Tuesday 20 October 2026, and even the fixed version
 *      would fail here if it emptied the file before taking the lock instead of after
 *   4  the same path opens again once the first log is closed. This is deliberate and it
 *      is asserted rather than left implicit: 21 of this volume's systemd units carry
 *      Restart= with a fixed --log path, and e08's chapter demonstrates its console
 *      surviving kill -9, so a finished log has to be reopenable
 *   5  a path whose directory does not exist fails, and leaves nothing behind
 *
 * Needs no model, no grammar and no board: elog_open touches nothing but the file. Takes
 * its scratch directory from argv[1] so ctest can keep it inside the build tree.
 */
#include "elog.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int failures;

static void ok(int condition, const char *what)
{
    printf("%-4s %s\n", condition ? "pass" : "FAIL", what);
    if (!condition)
        failures++;
}

/* Bytes in the file, or -1 when it cannot be read. Bytes and not lines: the damage being
 * guarded against is a truncation, and this project has already been misled once by
 * counting lines in a file whose tail was NUL. */
static long size_of(const char *path)
{
    long n;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return -1;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    n = ftell(fp);
    fclose(fp);
    return n;
}

static void write_one_row(elog *l, double t_s)
{
    io_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.t_s = t_s;
    snprintf(ev.tag, sizeof(ev.tag), "guard");
    ev.n = 1;
    ev.v[0] = 1.25;
    /* NULL model: this test is about the file, not about a decision. */
    if (elog_row(l, &ev, "NORMAL", NULL) != 0)
        ok(0, "elog_row returned non-zero");
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char path[1024];
    char nodir[1024];
    elog *first;
    elog *second;
    long after_first_row;
    long header_only;

    snprintf(path, sizeof(path), "%s/elog_guard.csv", dir);
    snprintf(nodir, sizeof(nodir), "%s/no_such_directory/elog_guard.csv", dir);
    unlink(path);   /* the test owns this path; a leftover would make case 1 meaningless */

    /* 1 */
    first = elog_open(path, "guard-test");
    ok(first != NULL, "1  a fresh path opens");
    if (first == NULL) {
        printf("cannot continue without the first log\n");
        return 1;
    }
    header_only = size_of(path);
    write_one_row(first, 0.5);
    after_first_row = size_of(path);
    ok(after_first_row > header_only, "1  the row reached the file");

    /* 2 and 3: the refusal, and the bytes it protected. */
    printf("   (the next line on stderr is the expected refusal)\n");
    second = elog_open(path, "guard-test");
    ok(second == NULL, "2  a second writer on the same path is refused");
    if (second != NULL)
        elog_close(second);
    ok(size_of(path) == after_first_row,
       "3  the first log's bytes are unchanged by the refused second open");

    /* 4: the documented limit, asserted so that narrowing it later fails here first. */
    elog_close(first);
    second = elog_open(path, "guard-test");
    ok(second != NULL, "4  the path opens again once the first log is closed");
    if (second != NULL) {
        ok(size_of(path) == header_only, "4  and it starts from a header and no rows");
        write_one_row(second, 1.5);
        elog_close(second);
    }

    /* 5 */
    printf("   (the next line on stderr is an expected missing-directory message)\n");
    second = elog_open(nodir, "guard-test");
    ok(second == NULL, "5  a path in a missing directory is refused");
    if (second != NULL)
        elog_close(second);
    ok(size_of(nodir) == -1, "5  and no file was created there");

    unlink(path);
    printf("%s: %d failed\n", argv[0], failures);
    return failures == 0 ? 0 : 1;
}
