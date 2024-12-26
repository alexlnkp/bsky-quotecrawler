#ifndef   __CRAWLER_H__
#define   __CRAWLER_H__

void shared_curl_init(void);
void shared_curl_destroy(void);

const char* get_actor(const char* post);
char* get_did(const char *actor);
char* extract_post_id(const char* url);
char* post_uri_to_https(const char *uri);

void recursive_quote_search(const char* actor_did, const char* post_id,
                            char*** visited, int* visited_count, char*** all_quotes, int* all_quotes_count);

#endif /* __CRAWLER_H__ */
