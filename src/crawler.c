#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include "crawler.h"

/* useragent to use for requests */
#define REQ_USERAGENT "libcurl-agent/1.0"

/* max length for the actor string. example actor string: `413x1nkp.bsky.social` */
#define MAX_ACTOR_LENGTH 128

/* DID length. example: `did:plc:ybflevxvh5zylcoxbohxu224` */
#define DID_LEN 32

/* AT protocol string. used inplace of http/https */
#define ATPROTO "at://"

CURL *curl; /* internal curl instance. don't touch */
CURLM *multi_handle; /* multi handle for asynchronous requests. don't touch */



char* extract_post_id(const char* post_url) {
    const char* last_slash = strrchr(post_url, '/');
    if (last_slash != NULL) {
        return strdup(last_slash + 1);
    }
    return NULL;
}


const char* get_actor(const char* post_url) {
    char* actor = malloc(MAX_ACTOR_LENGTH * sizeof(char));
    const char* profile_start = strstr(post_url, "profile/");

    if (!profile_start) goto failure;
    profile_start += strlen("profile/");

    const char* post_start = strstr(profile_start, "/post");
    if (!post_start) goto failure;

    size_t length = post_start - profile_start;
    if (length >= MAX_ACTOR_LENGTH) goto failure;

    strncpy(actor, profile_start, length);
    actor[length] = '\0';
    return actor;

failure:
    strncpy(actor, "unk", MAX_ACTOR_LENGTH);
    actor[MAX_ACTOR_LENGTH - 1] = '\0';
    return actor;
}


struct MemoryStruct {
    char *memory;
    size_t size;
};


/* basic MemoryStruct initializer */
struct MemoryStruct init_MemoryStruct() {
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    return chunk;
}


/* write callback function used for curl requests */
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    /* had to add this because of this odd warning:
     * call to ‘_curl_easy_setopt_err_write_callback’
     * declared with attribute warning: curl_easy_setopt
     * expects a curl_write_callback argument for this option */
    struct MemoryStruct* ud = userp;
    size_t realsize = size * nmemb;
    ud->memory = realloc(ud->memory, ud->size + realsize + 1);
    if (ud->memory == NULL) {
        fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(ud->memory[ud->size]), contents, realsize);

    ud->size += realsize;
    ud->memory[ud->size] = 0;
    return realsize;
}


void shared_curl_init(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    multi_handle = curl_multi_init();
    if (!curl || !multi_handle) {
        fprintf(stderr, "Failed to initialize cURL\n");
        exit(1);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, REQ_USERAGENT);
}


void shared_curl_destroy(void) {
    curl_easy_cleanup(curl);
    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();
}


char* get_did(const char *actor) {
    CURLcode res;
    struct MemoryStruct chunk = init_MemoryStruct();

    char* result = "unk";

    char url[128 + MAX_ACTOR_LENGTH];
    snprintf(url, sizeof(url), "https://public.api.bsky.app/xrpc/app.bsky.actor.getProfile?actor=%s", actor);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        return result;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        fprintf(stderr, "cURL request failed! response - %ld\nRAW: %s\n", response_code, chunk.memory);
        free(chunk.memory);
        return result;
    }

    struct json_object *parsed_json;
    struct json_object *did_obj;

    parsed_json = json_tokener_parse(chunk.memory);
    if (json_object_object_get_ex(parsed_json, "did", &did_obj)) {
        const char *did = json_object_get_string(did_obj);
        result = strdup(did);
    }

    json_object_put(parsed_json);
    free(chunk.memory);

    return result;
}


/* add a quote request to our curl-multi */
void add_quote_request(const char* actor_did, const char* post_id, struct MemoryStruct *chunk) {
    char url[256];
    snprintf(url, sizeof(url), "https://public.api.bsky.app/xrpc/app.bsky.feed.getQuotes?uri=%s%s/app.bsky.feed.post/%s", ATPROTO, actor_did, post_id);

    CURL *easy_handle = curl_easy_init();
    curl_easy_setopt(easy_handle, CURLOPT_URL, url);
    curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, (void*)chunk);
    curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_multi_add_handle(multi_handle, easy_handle);
}


/* process completed curl-multi requests */
void process_completed_requests(void) {
    int still_running = 0;
    do {
        curl_multi_perform(multi_handle, &still_running);
    } while (still_running);

    CURLMsg *msg;
    int msgs_left;
    while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            struct MemoryStruct *chunk;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &chunk);
            curl_multi_remove_handle(multi_handle, msg->easy_handle);
            curl_easy_cleanup(msg->easy_handle);
            free(chunk);
        }
    }
}


/* get quotes from a post */
json_object* get_quotes(const char* actor_did, const char* post_id) {
    struct MemoryStruct chunk = init_MemoryStruct();
    add_quote_request(actor_did, post_id, &chunk);
    process_completed_requests();

    json_object *json_response = NULL;
    if (chunk.size > 0) {
        json_response = json_tokener_parse(chunk.memory);
        if (json_response == NULL) {
            fprintf(stderr, "failed to parse JSON response from get_quotes()\n");
        }
    }
    free(chunk.memory);
    return json_response;
}


/* check if quote was already visited */
int is_visited(const char* post_identifier, char** visited, int visited_count) {
    for (int i = 0; i < visited_count; i++) {
        if (strcmp(visited[i], post_identifier) == 0) return 1;
    }
    return 0;
}


/* mark the quote as visited */
void add_visited(const char* post_identifier, char*** visited, int* visited_count) {
    *visited = realloc(*visited, (*visited_count + 1) * sizeof(char*));
    (*visited)[*visited_count] = strdup(post_identifier);
    (*visited_count)++;
}


void recursive_quote_search(const char* actor_did, const char* post_id,
                            char*** visited, int* visited_count, char*** all_quotes, int* all_quotes_count) {
    char post_identifier[256];
    snprintf(post_identifier, sizeof(post_identifier), "%s/%s", actor_did, post_id);

    if (is_visited(post_identifier, *visited, *visited_count)) return;
    add_visited(post_identifier, visited, visited_count);

    json_object* quotes = get_quotes(actor_did, post_id);
    if (quotes == NULL) return;

    json_object* posts;
    if (json_object_object_get_ex(quotes, "posts", &posts)) {
        int array_len = json_object_array_length(posts);
        for (int i = 0; i < array_len; i++) {
            json_object* post = json_object_array_get_idx(posts, i);
            const char* post_uri = json_object_get_string(json_object_object_get(post, "uri"));
            (*all_quotes) = realloc(*all_quotes, (*all_quotes_count + 1) * sizeof(char*));
            (*all_quotes)[*all_quotes_count] = strdup(post_uri);
            (*all_quotes_count)++;

            const char* quoted_actor_did = json_object_get_string(json_object_object_get(json_object_object_get(post, "author"), "did"));
            char* quoted_post_id = extract_post_id(post_uri);
            recursive_quote_search(quoted_actor_did, quoted_post_id, visited, visited_count, all_quotes, all_quotes_count);
            free(quoted_post_id);
        }
    }

    json_object_put(quotes);
}


/* get DID from given AT-URI. example:
 * input: "at://did:plc:ybflevxvh5zylcoxbohxu224/app.bsky.feed.post/3l7det4aqy52h";
 * output: "did:plc:ybflevxvh5zylcoxbohxu224" */
char* get_did_from_uri(const char* uri) {
    char* did = calloc(DID_LEN, sizeof(char));
    strncpy(did, uri + strlen(ATPROTO), DID_LEN);
    return did;
}


char* post_uri_to_https(const char *uri) {
    if (strncmp(uri, ATPROTO, 5) != 0) return (char*)uri;

    size_t result_size = 128;
    char* result = calloc(result_size, sizeof(char));

    const char *base = "https://bsky.app/profile/";
    const char *post_path = "/post/";
    const char *app_path = "/app.bsky.feed.post/";

    char* did = get_did_from_uri(uri);
    char* post_id = extract_post_id(uri);
    strcat(result, base);                                                 /* append base     */
    strncat(result + strlen(base), did, DID_LEN);                         /* append did      */
    strcat(result + strlen(base) + DID_LEN, post_path);                   /* append `/post/` */
    strcat(result + strlen(base) + DID_LEN + strlen(post_path), post_id); /* append post id  */
    /* result should be already null-terminated since we used calloc() */

    free(did);
    free(post_id);

    return result;
}
