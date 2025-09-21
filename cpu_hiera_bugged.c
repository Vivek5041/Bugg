#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

/*
 Synthetic pthread mutex benchmark with an injected bug:
 - Each thread does WARMUP_ITERATIONS acquires+releases (nested `nesting_depth` locks) as warmup.
 - Then it does NUM_ITERATIONS real iterations of acquire+release.
 - With a small probability (bug_prob) the code intentionally *skips* the corresponding
   pthread_mutex_unlock() for one of the acquired locks in an iteration, simulating a missed unlock.

 Usage:
   ./cpu_hiera_bugged <num_threads> <nesting_depth> <warmup_iterations> <num_iterations> [bug_prob]
   - bug_prob is optional and defaults to 1e-6 (very small). It is a floating point probability in [0,1].

 Note: skipping an unlock will very likely cause deadlock under contention; use small bug_prob.
*/

#define MAX_NESTING 1024

static pthread_mutex_t *mutexes = NULL;
static int nesting_depth = 1;
static uint64_t warmup_iterations = 1000;
static uint64_t num_iterations = 10000;
static uint64_t counter = 0;
struct thread_args {
    int tid;
    uint64_t iters;
    double bug_prob;
};

static inline void die(const char *s) { perror(s); exit(1); }

void *worker(void *arg) {
    struct thread_args *a = (struct thread_args *)arg;
    int tid = a->tid;
    uint64_t i, level;
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self() ^ (unsigned int)tid;

    /* Warmup */
    for (i = 0; i < warmup_iterations; ++i) {
        for (level = 0; level < (unsigned)nesting_depth; ++level) {
            if (pthread_mutex_lock(&mutexes[level]) != 0) die("pthread_mutex_lock");
        }
        for (level = 0; level < (unsigned)nesting_depth; ++level) {
            if (pthread_mutex_unlock(&mutexes[level]) != 0) die("pthread_mutex_unlock");
        }
    }

    /* Measured iterations. We inject a bug: with probability bug_prob we skip an unlock for one of the locks. */
    for (i = 0; i < a->iters; ++i) {
    	int skip_lock_level = -1;
    	int skip_unlock_level = -1;

#ifdef INJECT_UU_BUG
    /* Decide whether to skip a lock this iteration (unbalanced lock). */
    double r_lock = (double)rand_r(&seed) / (double)RAND_MAX;
    if (r_lock < a->bug_prob) {
        skip_lock_level = rand_r(&seed) % nesting_depth;
        fprintf(stderr, "[BUG] thread %d iteration %" PRIu64 " skipping lock of level %d\n",
                tid, i, skip_lock_level);
    }
#endif

    /* Acquire nesting_depth locks, except maybe one skipped */
    for (level = 0; level < (unsigned)nesting_depth; ++level) {
        if ((int)level == skip_lock_level) {
#ifdef INJECT_UU_BUG
            /* Intentionally skip locking this mutex */
            continue;
#endif
        }
        if (pthread_mutex_lock(&mutexes[level]) != 0) die("pthread_mutex_lock");
    }

    /* Simulate some work */
    counter++;
    if ((i & 255) == 0) sched_yield();

#ifdef INJECT_UL_BUG
    /* Decide whether to skip an unlock this iteration (unbalanced unlock). */
    double r_unlock = (double)rand_r(&seed) / (double)RAND_MAX;
    if (r_unlock < a->bug_prob) {
        skip_unlock_level = rand_r(&seed) % nesting_depth;
        fprintf(stderr, "[BUG] thread %d iteration %" PRIu64 " skipping unlock of level %d\n",
                tid, i, skip_unlock_level);
    }
#endif

    /* Release locks; if skip_unlock_level == x, do not unlock mutexes[x]. */
    for (level = 0; level < (unsigned)nesting_depth; ++level) {
        if ((int)level == skip_unlock_level) {
#ifdef INJECT_UL_BUG
            /* Intentionally skip unlocking this mutex */
            continue;
#endif
        }
        if (pthread_mutex_unlock(&mutexes[level]) != 0) die("pthread_mutex_unlock");
    }
        /* Note: if a skip happened, that mutex remains locked and may cause deadlock for other threads. */
    }return NULL;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <num_threads> <nesting_depth> <warmup_iterations> <num_iterations> [bug_prob]\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    nesting_depth = atoi(argv[2]);
    warmup_iterations = (uint64_t)atoll(argv[3]);
    num_iterations = (uint64_t)atoll(argv[4]);
    double bug_prob = 1e-6; /* default very small probability */
    if (argc >= 6) bug_prob = atof(argv[5]);
    if (nesting_depth < 1 || nesting_depth > MAX_NESTING) {
        fprintf(stderr, "nesting_depth must be 1..%d\n", MAX_NESTING);
        return 1;
    }
    if (num_threads < 1) {
        fprintf(stderr, "num_threads must be >= 1\n");
        return 1;
    }

    mutexes = calloc(nesting_depth, sizeof(pthread_mutex_t));
    if (!mutexes) die("calloc");

    for (int i = 0; i < nesting_depth; ++i) {
        if (pthread_mutex_init(&mutexes[i], NULL) != 0) die("pthread_mutex_init");
    }

    pthread_t *t = calloc(num_threads, sizeof(pthread_t));
    struct thread_args *args = calloc(num_threads, sizeof(struct thread_args));
    if (!t || !args) die("calloc2");

    for (int i = 0; i < num_threads; ++i) {
        args[i].tid = i;
        args[i].iters = num_iterations;
        args[i].bug_prob = bug_prob;
        if (pthread_create(&t[i], NULL, worker, &args[i]) != 0) die("pthread_create");
    }

    for (int i = 0; i < num_threads; ++i) pthread_join(t[i], NULL);

    for (int i = 0; i < nesting_depth; ++i) pthread_mutex_destroy(&mutexes[i]);

    free(mutexes);
    free(t);
    free(args);

    printf("Done: Counter %lu\n", counter);
    return 0;
}
