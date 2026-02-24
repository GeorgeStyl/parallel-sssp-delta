#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

#define INITIAL_ADJ_CAPACITY 4
#define INITIAL_BUCKET_CAPACITY 4
#define NUM_THREADS 4
#define MAX_BUCKETS 200000 

/* Struct Definitions */
typedef struct { int to; double weight; } Edge;
typedef struct { int id; } Vertex;
typedef struct { Vertex* v; int size; int capacity; pthread_mutex_t lock; } Bucket;
typedef struct { int n, m; Edge** adj; int* adj_size; int* adj_cap; } Graph;

/* Global State */
Graph* global_g;
double* global_dist;
Bucket** global_B;
int global_Bsize;
double global_DELTA;
int current_bucket_idx = 0;

pthread_barrier_t barrier;
pthread_mutex_t* vertex_locks;

/* Enhanced Logging Function */
void log_event(int tid, const char *format, ...) {
    char filename[32];
    sprintf(filename, "thread_%d.log", tid);
    FILE *f = fopen(filename, "a");
    if (!f) return;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* tm_info = localtime(&tv.tv_sec);
    char time_buffer[20];
    strftime(time_buffer, 20, "%H:%M:%S", tm_info);

    fprintf(f, "[%s.%03ld] [TID %d]: ", time_buffer, tv.tv_usec / 1000, tid);
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fprintf(f, "\n");
    fclose(f);
}

/* Thread-Safe Edge Relaxation */
void relax_edge(int u, int v, double weight, int tid) {
    double new_dist = global_dist[u] + weight;

    if (new_dist < global_dist[v]) {
        pthread_mutex_lock(&vertex_locks[v]);
        if (new_dist < global_dist[v]) {
            global_dist[v] = new_dist;
            pthread_mutex_unlock(&vertex_locks[v]);

            int bidx = (int)floor(new_dist / global_DELTA);
            if (bidx < global_Bsize) {
                Bucket* b = global_B[bidx];
                pthread_mutex_lock(&b->lock);
                if (b->size == b->capacity) {
                    b->capacity *= 2;
                    b->v = realloc(b->v, b->capacity * sizeof(Vertex));
                }
                b->v[b->size++].id = v;
                pthread_mutex_unlock(&b->lock);
            }
        } else {
            pthread_mutex_unlock(&vertex_locks[v]);
        }
    }
}

/* Revised Thread Worker with Strict Synchronization */
void* thread_worker(void* arg) {
    int my_tid = *(int*)arg;
    log_event(my_tid, "THREAD_INIT");

    /* Phase 1: Light Edges Fixpoint */
    // We need a shared variable for the snapshot (can be a global or a pointer)
    static int shared_size;

    while (1) {
        /* BARRIER 1: Synchronized Exit Check */
        pthread_barrier_wait(&barrier);
        if (current_bucket_idx >= global_Bsize) {
            log_event(my_tid, "EXIT_SIGNAL_RECEIVED");
            break;
        }

        int i = current_bucket_idx;
        int processed_in_bucket = 0;
        int iteration = 0;

        /* Phase 1: Light Edges Fixpoint */
        while (1) {

            /* BARRIER 2: Sync BEFORE capturing size to prevent TID 2 lag */
            pthread_barrier_wait(&barrier);
            

            /* Only TID 0 takes the snapshot of the current global size */
            if (my_tid == 0) {
                shared_size = global_B[i]->size;
            }

            /* BARRIER: Ensure TID 0 has finished writing to shared_size */
            pthread_barrier_wait(&barrier);

            // Now everyone uses the EXACT same variable 'shared_size'
            int size_at_start = shared_size;
        
            log_event(my_tid, "B%d_L_ITER_%d: Processing %d to %d",  i, iteration, processed_in_bucket, size_at_start);

            // !!!!! int size_at_start = global_B[i]->size;
            // !!!!! log_event(my_tid, "B%d_L_ITER_%d: Processing %d to %d", i, iteration, processed_in_bucket, size_at_start);

            // If no new nodes were added in the previous iteration, the bucket is settled
            if (size_at_start == processed_in_bucket) {
                log_event(my_tid, "B%d_L_STABLE", i);
                break; 
            }

            for (int k = processed_in_bucket + my_tid; k < size_at_start; k += NUM_THREADS) {
                int u = global_B[i]->v[k].id;
                for (int e = 0; e < global_g->adj_size[u]; e++) {
                    Edge edge = global_g->adj[u][e];
                    if (edge.weight <= global_DELTA) {
                        relax_edge(u, edge.to, edge.weight, my_tid);
                    }
                }
            }

            processed_in_bucket = size_at_start;
            iteration++;
        }

        /* Phase 2: Heavy Edges */
        log_event(my_tid, "B%d_H_START", i);
        int final_size = global_B[i]->size;
        for (int k = my_tid; k < final_size; k += NUM_THREADS) {
            int u = global_B[i]->v[k].id;
            for (int e = 0; e < global_g->adj_size[u]; e++) {
                Edge edge = global_g->adj[u][e];
                if (edge.weight > global_DELTA) {
                    relax_edge(u, edge.to, edge.weight, my_tid);
                }
            }
        }

        /* BARRIER 3: Sync after Heavy Edges */
        pthread_barrier_wait(&barrier);

        if (my_tid == 0) {
            int old_idx = current_bucket_idx;
            current_bucket_idx++;
            while (current_bucket_idx < global_Bsize && global_B[current_bucket_idx]->size == 0) {
                current_bucket_idx++;
            }
            log_event(my_tid, "INDEX_MOVE: %d -> %d", old_idx, current_bucket_idx);
        }
        
        /* BARRIER 4: Sync Index Update */
        pthread_barrier_wait(&barrier);
    }
    return NULL;
}

/* ... Graph Loading and Main remains largely the same ... */
Graph* read_graph(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) exit(EXIT_FAILURE);
    Graph* g = malloc(sizeof(Graph));
    fscanf(f, "%d %d", &g->n, &g->m);
    g->adj = malloc(g->n * sizeof(Edge*));
    g->adj_size = calloc(g->n, sizeof(int));
    g->adj_cap = malloc(g->n * sizeof(int));
    for (int i = 0; i < g->n; i++) {
        g->adj_cap[i] = INITIAL_ADJ_CAPACITY;
        g->adj[i] = malloc(INITIAL_ADJ_CAPACITY * sizeof(Edge));
    }
    for (int i = 0; i < g->m; i++) {
        int u, v; double w;
        fscanf(f, "%d %d %lf", &u, &v, &w);
        if (g->adj_size[u] == g->adj_cap[u]) {
            g->adj_cap[u] *= 2;
            g->adj[u] = realloc(g->adj[u], g->adj_cap[u] * sizeof(Edge));
        }
        g->adj[u][g->adj_size[u]++] = (Edge){ v, w };
    }
    fclose(f);
    return g;
}

int main(int argc, char** argv) {
    if (argc != 4) return EXIT_FAILURE;
    
    // Clear logs from previous run
    for (int i = 0; i < NUM_THREADS; i++) {
        char filename[32]; sprintf(filename, "thread_%d.log", i);
        FILE *f = fopen(filename, "w"); if (f) fclose(f);
    }

    global_g = read_graph(argv[1]);
    int source = atoi(argv[2]);
    global_DELTA = atof(argv[3]);

    pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    vertex_locks = malloc(global_g->n * sizeof(pthread_mutex_t));
    global_dist = malloc(global_g->n * sizeof(double));

    for (int i = 0; i < global_g->n; i++) {
        pthread_mutex_init(&vertex_locks[i], NULL);
        global_dist[i] = DBL_MAX;
    }

    global_dist[source] = 0.0;
    global_Bsize = MAX_BUCKETS;
    global_B = malloc(global_Bsize * sizeof(Bucket*));
    for (int i = 0; i < global_Bsize; i++) {
        global_B[i] = malloc(sizeof(Bucket));
        global_B[i]->size = 0; global_B[i]->capacity = INITIAL_BUCKET_CAPACITY;
        global_B[i]->v = malloc(INITIAL_BUCKET_CAPACITY * sizeof(Vertex));
        pthread_mutex_init(&global_B[i]->lock, NULL);
    }

    // Prepare Bucket 0 before starting threads
    global_B[0]->v[global_B[0]->size++].id = source;

    pthread_t threads[NUM_THREADS];
    int tids[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        tids[i] = i;
        pthread_create(&threads[i], NULL, thread_worker, &tids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);

    for (int i = 0; i < global_g->n; i++) {
        if (global_dist[i] == DBL_MAX) printf("%d: INF\n", i);
        else printf("%d: %.6f\n", i, global_dist[i]);
    }

    /* --- CLEANUP SECTION --- */

    // 1. Destroy Synchronization Objects
    pthread_barrier_destroy(&barrier);

    for (int i = 0; i < global_g->n; i++) {
        pthread_mutex_destroy(&vertex_locks[i]);
    }
    free(vertex_locks);

    for (int i = 0; i < global_Bsize; i++) {
        pthread_mutex_destroy(&global_B[i]->lock);
        free(global_B[i]->v);
        free(global_B[i]);
    }
    free(global_B);

    // 2. Free Graph and Distance Array
    for (int i = 0; i < global_g->n; i++) {
        free(global_g->adj[i]);
    }
    free(global_g->adj);
    free(global_g->adj_size);
    free(global_g->adj_cap);
    free(global_g);
    
    free(global_dist);

    return 0;
}