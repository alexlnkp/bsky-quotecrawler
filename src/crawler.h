#ifndef   __CRAWLER_H__
#define   __CRAWLER_H__

/* initializes internal curl instances within crawler */
void shared_curl_init(void);

/* destroys internal curl instances within crawler */
void shared_curl_destroy(void);

/* get actor/handle of a user from a post url. example:
 * input: "https://bsky.app/profile/413x1nkp.bsky.social/post/3ldzgecezms2d";
 * output: "413x1nkp.bsky.social" */
const char* get_actor(const char* post);

/* request DID of an actor. example:
 * input: "413x1nkp.bsky.social";
 * output: "did:plc:ybflevxvh5zylcoxbohxu224" */
char* get_did(const char *actor);

/* extract post id from the post url. example:
 * input: "https://bsky.app/profile/413x1nkp.bsky.social/post/3ldzgecezms2d";
 * output: "3ldzgecezms2d" */
char* extract_post_id(const char* url);

/* convert post's AT-URI to https link. example:
 * input: "at://did:plc:ybflevxvh5zylcoxbohxu224/app.bsky.feed.post/3l7det4aqy52h";
 * output: "https://bsky.app/profile/did:plc:ybflevxvh5zylcoxbohxu224/post/3l7det4aqy52h" */
char* post_uri_to_https(const char *uri);

/* recursively find all quotes and store the ATPROTO links to each one in `char*** all_quotes` */
void recursive_quote_search(const char* actor_did, const char* post_id,
                            char*** visited, int* visited_count, char*** all_quotes, int* all_quotes_count);

#endif /* __CRAWLER_H__ */
