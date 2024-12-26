#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "crawler.h"

/* placeholder url */
#define POST_URL "https://bsky.app/profile/raysan5.bsky.social/post/3le4og7pvh22w"

struct quote_search_params {
    const char* actor_did;
    const char* post_id;
    char** visited;
    int visited_count;
    char** all_quotes;
    int all_quotes_count;
};

struct quote_search_params qsp;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int is_thread_running = 1;
int new_quote_available = 0;

void signal_main_thread(void) {
    pthread_mutex_lock(&mutex);
    new_quote_available = 1;
    pthread_mutex_unlock(&mutex);
}

void print_last_quote(struct quote_search_params *qsp) {
    if (qsp->all_quotes_count > 0) {
        char* https = post_uri_to_https(qsp->all_quotes[qsp->all_quotes_count - 1]);
        printf("%s\n", https);
        free(https);
    }
    new_quote_available = 0;
}

void* init_recursive_quote_search(void* arg) {
    struct quote_search_params* qsp = arg;

    recursive_quote_search(qsp->actor_did, qsp->post_id, &qsp->visited,
                          &qsp->visited_count, &qsp->all_quotes, &qsp->all_quotes_count);

    pthread_mutex_lock(&mutex);
    is_thread_running = 0;
    pthread_mutex_unlock(&mutex);
    return NULL;
}


int main(void) {
    shared_curl_init();

    const char* actor = get_actor(POST_URL);
    qsp = (struct quote_search_params){
        .actor_did = get_did(actor),
        .post_id = extract_post_id(POST_URL),
        .visited = NULL,
        .visited_count = 0,
        .all_quotes = NULL,
        .all_quotes_count = 0
    };

    pthread_t quote_search_thread;
    if (pthread_create(&quote_search_thread, NULL, init_recursive_quote_search, &qsp) != 0) {
        fprintf(stderr, "Failed to create thread\n");
        return 1;
    }

    while (is_thread_running) {
        pthread_mutex_lock(&mutex);
        if (new_quote_available) print_last_quote(&qsp);
        pthread_mutex_unlock(&mutex);

        usleep(1000);
    }

    pthread_join(quote_search_thread, NULL);

    printf("%d\n", qsp.all_quotes_count);

    free(qsp.all_quotes);
    free(qsp.visited);

    free((char*)qsp.actor_did);
    free((char*)qsp.post_id);
    free((char*)actor);

    shared_curl_destroy();

    return 0;
}
