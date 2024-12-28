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

char* get_did_from_uri(const char* uri);
void get_quotes(const char* actor_did, const char* post_id);
char* extract_post_id(const char* post_url);

char* post_uri_to_https(const char *uri) {
    if (strncmp(uri, ATPROTO, 5) != 0) return (char*)uri;

    size_t result_size = 128;
    char* result = calloc(result_size, sizeof(char));

    const char *base = "https://bsky.app/profile/";
    const char *post_path = "/post/";
    const char *app_path = "/app.bsky.feed.post/";

    char* did = get_did_from_uri(uri);
    char* post_id = extract_post_id(uri);
    strcat(result, base);          /* append base     */
    strncat(result, did, DID_LEN); /* append did      */
    strcat(result, post_path);     /* append `/post/` */
    strcat(result, post_id);       /* append post id  */
    /* result should be already null-terminated since we used calloc() */

    free(did);
    free(post_id);

    return result;
}


const char* get_actor(const char* post_url) {
    char* actor = calloc(MAX_ACTOR_LENGTH, sizeof(char));
    if (actor == NULL) {
        emscripten_log(EM_LOG_ERROR, "out of memory");
        return NULL;
    }

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


/* get DID from given AT-URI. example:
 * input: "at://did:plc:ybflevxvh5zylcoxbohxu224/app.bsky.feed.post/3l7det4aqy52h";
 * output: "did:plc:ybflevxvh5zylcoxbohxu224" */
char* get_did_from_uri(const char* uri) {
    char* did = calloc(DID_LEN + 1, sizeof(char));
    if (did == NULL) {
        emscripten_log(EM_LOG_ERROR, "out of memory");
        return NULL;
    }

    strncpy(did, uri + strlen(ATPROTO), DID_LEN);
    did[DID_LEN] = '\0';
    return did;
}


void quote_fetch_success(emscripten_fetch_t *fetch) {
    json_object *json_response = NULL;

    json_response = json_tokener_parse(fetch->data);
    if (json_response == NULL) {
        emscripten_log(EM_LOG_ERROR, "failed to parse JSON response from get_quotes(). data: %s\n", fetch->data);
        return;
    }

    json_object* posts;
    json_object_object_get_ex(json_response, "posts", &posts);

    size_t array_len = json_object_array_length(posts);
    for (size_t i = 0; i < array_len; i++) {
        json_object* post = json_object_array_get_idx(posts, i);

        const char* post_uri = json_object_get_string(json_object_object_get(post, "uri"));
        const char* did = get_did_from_uri(post_uri);
        const char* post_id = extract_post_id(post_uri);
        if (did == NULL || post_id == NULL || post_uri == NULL) {
            emscripten_log(EM_LOG_ERROR, "did, post_id or post_uri are NULL");
            continue;
        }

        get_quotes(did, post_id);

        char* https_url = post_uri_to_https(post_uri);

        emscripten_log(EM_LOG_INFO, "%s", https_url);
        free((char*)did); free((char*)post_id); free(https_url);
    }

    json_object_put(json_response);
}

void quote_fetch_failure(emscripten_fetch_t *fetch) {
    emscripten_log(EM_LOG_INFO, "failure! data: %s\n", fetch->data);
}


void get_quotes(const char* actor_did, const char* post_id) {
    char* url = calloc(256, sizeof(char));
    snprintf(url, 256, "https://public.api.bsky.app/xrpc/app.bsky.feed.getQuotes?uri=%s%s/app.bsky.feed.post/%s", ATPROTO, actor_did, post_id);
    // emscripten_log(EM_LOG_INFO, "getting quotes from: %s", url);

    emscripten_fetch_attr_t fetch_attr;
    emscripten_fetch_attr_init(&fetch_attr);
    memset(&fetch_attr, 0, sizeof(fetch_attr));
    fetch_attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    strcpy(fetch_attr.requestMethod, "GET");

    fetch_attr.onsuccess = (void (*)(emscripten_fetch_t *))quote_fetch_success;
    fetch_attr.onerror = (void (*)(emscripten_fetch_t *))quote_fetch_failure;

    emscripten_fetch(&fetch_attr, url);

    // free(url);
}


char* extract_post_id(const char* post_url) {
    const char* last_slash = strrchr(post_url, '/');
    if (last_slash != NULL) {
        return strdup(last_slash + 1);
    }
    return NULL;
}


void handle_result(char *result) {
    // emscripten_log(EM_LOG_CONSOLE, "Result: %s\n", result);
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
