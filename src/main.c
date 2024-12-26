#include <stdio.h>
#include <stdlib.h>

#include "crawler.h"

/* placeholder url */
#define POST_URL "https://bsky.app/profile/raysan5.bsky.social/post/3le4og7pvh22w"

int main(void) {
    shared_curl_init();

    const char* actor = get_actor(POST_URL);
    char *did = get_did(actor);
    char *post_id = extract_post_id(POST_URL);

    char** visited = NULL;
    int visited_count = 0;

    char** all_quote_uris = NULL;
    int all_quote_uris_count = 0;
    recursive_quote_search(did, post_id, &visited, &visited_count, &all_quote_uris, &all_quote_uris_count);

    for (int i = 0; i < all_quote_uris_count; i++) {
        char* https = post_uri_to_https(all_quote_uris[i]);
        printf("%s\n", https);
        free(https);
        free(all_quote_uris[i]);
    }

    printf("%d\n", all_quote_uris_count);

    free(all_quote_uris);
    free(visited);

    free(did);
    free(post_id);
    free((char*)actor);

    shared_curl_destroy();

    return 0;
}
