#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define SAMPLES_DIRECTORY "samples"
#define SUBTASK_PREFIX "subtask"

struct subtask_data
{
    int serial;
    int test_score;
    int score;
    int first_test;
    int last_test;
    unsigned char is_samples;
    unsigned char is_hidden;
    unsigned char is_by_test;
    unsigned char is_first_fail;
    unsigned char is_total_only;
};

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "not enough arguments\n");
        exit(1);
    }

    const char *testdir = argv[1];
    {
        struct stat stb;
        if (lstat(testdir, &stb) < 0) {
            fprintf(stderr, "test directory '%s' does not exist\n", testdir);
            exit(1);
        }
        if (!S_ISDIR(stb.st_mode)) {
            fprintf(stderr, "'%s' is not a directory\n", testdir);
            exit(1);
        }
    }

    {
        char *p = NULL;
        asprintf(&p, "%s/%s", testdir, SAMPLES_DIRECTORY);
        struct stat stb;
        if (lstat(p, &stb) < 0) {
            fprintf(stderr, "samples directory '%s' does not exist\n", p);
            exit(1);
        }
        if (!S_ISDIR(stb.st_mode)) {
            fprintf(stderr, "'%s' is not a directory\n", p);
            exit(1);
        }
        free(p);
    }

    int subtask_count = 0;
    {
        DIR *d = opendir(testdir);
        if (!d) {
            fprintf(stderr, "cannot open directory '%s': %s\n", testdir, strerror(errno));
            exit(1);
        }
        struct dirent *dd;
        while ((dd = readdir(d))) {
            if (!strcmp(dd->d_name, ".") || !strcmp(dd->d_name, "..")) continue;
            if (!strcmp(dd->d_name, SAMPLES_DIRECTORY)) continue;
            if (strncmp(dd->d_name, SUBTASK_PREFIX, sizeof(SUBTASK_PREFIX) - 1)) {
                fprintf(stderr, "directory '%s' contains invalid entry '%s'\n", testdir, dd->d_name);
                exit(1);
            }
            errno = 0;
            char *eptr = NULL;
            char *str = dd->d_name + sizeof(SUBTASK_PREFIX) - 1;
            long value = strtol(str, &eptr, 10);
            if (errno || *eptr || eptr == str || (int) value != value || value <= 0 || value > 1000) {
                fprintf(stderr, "directory '%s' contains invalid entry '%s'\n", testdir, dd->d_name);
                exit(1);
            }
            char *p = NULL;
            asprintf(&p, "%s/%s%ld", testdir, SUBTASK_PREFIX, value);
            struct stat stb;
            if (lstat(p, &stb) < 0) {
                fprintf(stderr, "subtask directory '%s' does not exist\n", p);
                exit(1);
            }
            if (!S_ISDIR(stb.st_mode)) {
                fprintf(stderr, "'%s' is not a directory\n", p);
                exit(1);
            }
            free(p);
            ++subtask_count;
        }
        closedir(d);
    }
    if (subtask_count <= 0) {
        fprintf(stderr, "no subtasks\n");
        exit(1);
    }

    for (int subtask = 1; subtask <= subtask_count; ++subtask) {
        char *p = NULL;
        asprintf(&p, "%s/%s%d", testdir, SUBTASK_PREFIX, subtask);
        struct stat stb;
        if (lstat(p, &stb) < 0) {
            fprintf(stderr, "subtask directory '%s' does not exist\n", p);
            exit(1);
        }
        if (!S_ISDIR(stb.st_mode)) {
            fprintf(stderr, "'%s' is not a directory\n", p);
            exit(1);
        }
        free(p);
    }

    if (argc != subtask_count + 2) {
        fprintf(stderr, "wrong number of arguments\n");
        exit(1);
    }

    struct subtask_data *subtasks = calloc(subtask_count + 1, sizeof(*subtasks));
    subtasks[0].is_samples = 1;

    for (int st = 1; st <= subtask_count; ++st) {
        // parse command-line spec
        char *spec = strdup(argv[st + 1]);
        int slen = strlen(spec);
        // handle suffixes
        while (slen > 0) {
            if (spec[slen - 1] == 'h') {
                // hidden group
                subtasks[st].is_hidden = 1;
                spec[--slen] = 0;
            } else if (spec[slen - 1] == '+') {
                // by test scoring
                subtasks[st].is_by_test = 1;
                spec[--slen] = 0;
            } else if (spec[slen - 1] == 's') {
                // stop at first
                subtasks[st].is_first_fail = 1;
                spec[--slen] = 0;
            } else if (spec[slen - 1] == 't') {
                // show only total
                subtasks[st].is_total_only = 1;
                spec[--slen] = 0;
            } else {
                break;
            }
        }
        errno = 0;
        char *eptr = NULL;
        long val = strtol(spec, &eptr, 10);
        if (errno || *eptr || eptr == spec || (int) val != val || val < 0) {
            fprintf(stderr, "invalid score %s\n", spec);
            exit(1);
        }
        if (subtasks[st].is_by_test) {
            subtasks[st].test_score = val;
        } else {
            subtasks[st].score = val;
        }
        free(spec);
    }

    for (int st = 0; st <= subtask_count; ++st) {
        struct subtask_data *g = &subtasks[st];
        g->serial = st;
        char *p = NULL;
        if (g->is_samples) {
            asprintf(&p, "%s/%s", testdir, SAMPLES_DIRECTORY);
        } else {
            asprintf(&p, "%s/%s%d", testdir, SUBTASK_PREFIX, st);
        }
        DIR *d = opendir(p);
        if (!d) {
            fprintf(stderr, "cannot open directory '%s'\n", p);
            exit(1);
        }
        struct dirent *dd;
        int min_test_num = INT_MAX;
        int max_test_num = INT_MIN;
        while ((dd = readdir(d))) {
            if (!strcmp(dd->d_name, ".") || !strcmp(dd->d_name, "..")) continue;
            if (isdigit(dd->d_name[0])) {
                errno = 0;
                long val = strtol(dd->d_name, NULL, 10);
                if (errno || val <= 0 || (int) val != val) {
                    fprintf(stderr, "invalid test name '%s'\n", dd->d_name);
                    exit(1);
                }
                int v = val;
                if (v < min_test_num) min_test_num = v;
                if (v > max_test_num) max_test_num = v;
            }
        }
        if (max_test_num <= 0) {
            fprintf(stderr, "no tests in subtask %d\n", st);
            exit(1);
        }
        for (int tn = min_test_num; tn <= max_test_num; ++tn) {
            char *pi = NULL, *po = NULL;
            asprintf(&pi, "%s/%02d", p, tn);
            asprintf(&po, "%s/%02d.a", p, tn);

            struct stat stb;
            if (lstat(pi, &stb) < 0) {
                fprintf(stderr, "test file '%s' does not exist\n", pi);
                exit(1);
            }
            if (!S_ISREG(stb.st_mode)) {
                fprintf(stderr, "test file '%s' is not regular\n", pi);
                exit(1);
            }
            if (lstat(po, &stb) < 0) {
                fprintf(stderr, "answer file '%s' does not exist\n", pi);
                exit(1);
            }
            if (!S_ISREG(stb.st_mode)) {
                fprintf(stderr, "answer file '%s' is not regular\n", pi);
                exit(1);
            }

            free(pi);
            free(po);
        }
        free(p);
        g->first_test = min_test_num;
        g->last_test = max_test_num;
    }

    if (subtasks[0].first_test != 1) {
        fprintf(stderr, "first test number must be 1\n");
        exit(1);
    }
    for (int st = 1; st <= subtask_count; ++st) {
        if (subtasks[st - 1].last_test + 1 != subtasks[st].first_test) {
            fprintf(stderr, "last test in group %d is %d, but the first test in group %d is %d\n",
                    st - 1, subtasks[st - 1].last_test, st, subtasks[st].first_test);
            exit(1);
        }
    }

    int total_score = 0;
    for (int st = 1; st <= subtask_count; ++st) {
        struct subtask_data *g = &subtasks[st];
        if (g->is_by_test) {
            g->score = g->test_score * (g->last_test - g->first_test + 1);
        }
        total_score += g->score;
    }

    printf("# command line:");
    for (int i = 0; i < argc; ++i) {
        printf(" %s", argv[i]);
    }
    printf("\n");
    for (int st = 0; st <= subtask_count; ++st) {
        struct subtask_data *g = &subtasks[st];
        printf("group %d {\n", g->serial);
        if (g->first_test == g->last_test) {
            printf("    tests %d;\n", g->first_test);
        } else {
            printf("    tests %d-%d;\n", g->first_test, g->last_test);
        }
        if (g->is_by_test) {
            printf("    test_score %d;\n", g->test_score);
        } else {
            printf("    score %d;\n", g->score);
        }
        if (st > 0) {
            printf("    requires 0; # FIX IT\n");
        }
        if (!g->is_first_fail) {
            printf("    test_all;\n");
        }
        if (!g->is_samples) {
            printf("    stat_to_users;\n");
        }
        printf("}\n\n");
    }

    printf("# [problem]\n");
    printf("# full_score = %d\n", total_score);
    printf("# open_tests = \"");
    for (int st = 0; st <= subtask_count; ++st) {
        struct subtask_data *g = &subtasks[st];
        if (st > 0) printf(",");
        if (g->first_test == g->last_test) {
            printf("%d", g->first_test);
        } else {
            printf("%d-%d", g->first_test, g->last_test);
        }
        printf(":");
        if (g->is_samples) {
            printf("full");
        } else if (g->is_hidden || g->is_total_only) {
            printf("hidden");
        } else {
            printf("brief");
        }
    }
    printf("\"\n");
    printf("# final_open_tests = \"");
    printf("%d-%d:full", 1, subtasks[subtask_count].last_test);
    printf("\"\n");
    printf("# test_score_list = \"");
    char *sep = "";
    for (int st = 0; st <= subtask_count; ++st) {
        struct subtask_data *g = &subtasks[st];
        for (int tn = g->first_test; tn <= g->last_test; ++tn) {
            printf("%s", sep); sep = " ";
            int s = 0;
            if (g->is_by_test) {
                s = g->test_score;
            } else if (tn == g->last_test) {
                s = g->score;
            }
            printf("%d", s);
        }
    }
    printf("\"\n");
}
