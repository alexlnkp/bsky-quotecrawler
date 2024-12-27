#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json.h>

#include "ems_crawler.h"

/* max length for the actor string. example actor string: `413x1nkp.bsky.social` */
#define MAX_ACTOR_LENGTH 128

/* DID length. example: `did:plc:ybflevxvh5zylcoxbohxu224` */
#define DID_LEN 32

/* AT protocol string. used inplace of http/https */
#define ATPROTO "at://"

char* post_id;

const char* get_actor(const char* post_url) {
    char* actor = calloc(MAX_ACTOR_LENGTH, sizeof(char));
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


void quote_fetch_success(emscripten_fetch_t *fetch) {
    json_object *json_response = NULL;

    json_response = json_tokener_parse(fetch->data);
    if (json_response == NULL) {
        fprintf(stderr, "failed to parse JSON response from get_quotes()\n");
    }
    json_object* posts;
    json_object_object_get_ex(json_response, "posts", &posts);
    int array_len = json_object_array_length(posts);
    for (int i = 0; i < array_len; i++) {
        json_object* post = json_object_array_get_idx(posts, i);
        const char* post_uri = json_object_get_string(json_object_object_get(post, "uri"));

        emscripten_log(EM_LOG_INFO, "uri: %s", post_uri);
        json_object_put(post);
    }

    json_object_put(posts);
}

void quote_fetch_failure(emscripten_fetch_t *fetch) {
    emscripten_log(EM_LOG_INFO, "failure!\n");
}


void get_quotes(const char* actor_did, const char* post_id) {
    char url[256];
    snprintf(url, sizeof(url), "https://public.api.bsky.app/xrpc/app.bsky.feed.getQuotes?uri=%s%s/app.bsky.feed.post/%s", ATPROTO, actor_did, post_id);

    emscripten_fetch_attr_t fetch_attr;
    emscripten_fetch_attr_init(&fetch_attr);
    memset(&fetch_attr, 0, sizeof(fetch_attr));
    fetch_attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    strcpy(fetch_attr.requestMethod, "GET");

    fetch_attr.onsuccess = (void (*)(emscripten_fetch_t *))quote_fetch_success;
    fetch_attr.onerror = (void (*)(emscripten_fetch_t *))quote_fetch_failure;

    emscripten_fetch(&fetch_attr, url);
}


char* extract_post_id(const char* post_url) {
    const char* last_slash = strrchr(post_url, '/');
    if (last_slash != NULL) {
        return strdup(last_slash + 1);
    }
    return NULL;
}


void handle_result(char *result) {
    emscripten_log(EM_LOG_CONSOLE, "Result: %s\n", result);
    get_quotes(result, post_id);
    free(result);
}


void did_fetch_success(emscripten_fetch_t *fetch) {
    char *result = calloc((DID_LEN + 1), sizeof(char));

    struct json_object *parsed_json;
    struct json_object *did_obj;

    parsed_json = json_tokener_parse(fetch->data);
    if (json_object_object_get_ex(parsed_json, "did", &did_obj)) {
        const char *did = json_object_get_string(did_obj);
        strcpy(result, did);
    } else {
        emscripten_log(EM_LOG_ERROR, "response we got did not contain valid json: %s\n", fetch->data);
    }

    json_object_put(parsed_json);
    json_object_put(did_obj);

    emscripten_fetch_close(fetch);

    /* continue going on */
    handle_result(result);
}


void did_fetch_failure(emscripten_fetch_t *fetch) {
    char *result = NULL;
    emscripten_log(EM_LOG_ERROR, "Fetch failed with status: %d\n", fetch->status);
    result = strdup("unk");
    handle_result(result);
}


void get_did(const char *actor) {
    char url[128 + MAX_ACTOR_LENGTH];
    snprintf(url, sizeof(url), "https://public.api.bsky.app/xrpc/app.bsky.actor.getProfile?actor=%s", actor);

    emscripten_fetch_attr_t fetch_attr;
    emscripten_fetch_attr_init(&fetch_attr);

    memset(&fetch_attr, 0, sizeof(fetch_attr));
    fetch_attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    strcpy(fetch_attr.requestMethod, "GET");
    fetch_attr.onsuccess = (void (*)(emscripten_fetch_t *))did_fetch_success;
    fetch_attr.onerror = (void (*)(emscripten_fetch_t *))did_fetch_failure;

    emscripten_fetch(&fetch_attr, url);
}

void get_everything(const char* post_url) {
    const char* actor = get_actor(post_url);
    post_id = extract_post_id(post_url);
    get_did(actor);
}
