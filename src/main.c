#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <curl/curl.h>
#include <json-c/json.h>

#define USERAGENT "libcurl-agent/1.0"

/* placeholder url */
#define POST_URL "https://bsky.app/profile/raysan5.bsky.social/post/3le4og7pvh22w"

#define MAX_ACTOR_LENGTH 128
#define DID_LEN 32

#define ATPROTO "at://"

CURL *curl;

char* extract_post_id(const char* url) {
    const char* last_slash = strrchr(url, '/');
    if (last_slash != NULL) {
        return strdup(last_slash + 1);
    }
    return NULL;
}

const char* get_actor(const char* post) {
    char* actor = malloc(MAX_ACTOR_LENGTH * sizeof(char));
    const char* profile_start = strstr(post, "profile/");

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

struct MemoryStruct init_MemoryStruct() {
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    return chunk;
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, struct MemoryStruct *userp) {
    size_t realsize = size * nmemb;
    userp->memory = realloc(userp->memory, userp->size + realsize + 1);
    if (userp->memory == NULL) {
        fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(userp->memory[userp->size]), contents, realsize);

    userp->size += realsize;
    userp->memory[userp->size] = 0;
    return realsize;
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

json_object* get_quotes(const char* actor_did, const char* post_id) {
    CURLcode res;

    char url[256];
    snprintf(url, sizeof(url), "https://public.api.bsky.app/xrpc/app.bsky.feed.getQuotes?uri=%s%s/app.bsky.feed.post/%s", ATPROTO, actor_did, post_id);

    struct MemoryStruct chunk = init_MemoryStruct();

    json_object *json_response = NULL;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        chunk.memory = NULL;
    } else {
        json_response = json_tokener_parse(chunk.memory);
        if (json_response == NULL) {
            fprintf(stderr, "failed to parse JSON response from get_quotes()\n");
        }
    }

    return json_response;
}


int is_visited(const char* post_identifier, char** visited, int visited_count) {
    for (int i = 0; i < visited_count; i++) {
        if (strcmp(visited[i], post_identifier) == 0) return 1;
    }
    return 0;
}

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


void shared_curl_init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize cURL\n");
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USERAGENT);
}


void shared_curl_destroy() {
    curl_easy_cleanup(curl);
}


int main(void) {
    shared_curl_init();

    const char* actor = get_actor(POST_URL);
    char *did = get_did(actor);
    char *post_id = extract_post_id(POST_URL);

    json_object *quotes_json = get_quotes(did, post_id);

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

    json_object_put(quotes_json);

    free(did);
    free(post_id);
    free((char*)actor);

    shared_curl_destroy();
    curl_global_cleanup();

    return 0;
}
