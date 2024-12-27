#ifndef   __CRAWLER_H__
#define   __CRAWLER_H__

/* get actor/handle of a user from a post url. example:
 * input: "https://bsky.app/profile/413x1nkp.bsky.social/post/3ldzgecezms2d";
 * output: "413x1nkp.bsky.social" */
const char* get_actor(const char* post);

/* request DID of an actor. example:
 * input: "413x1nkp.bsky.social";
 * output: -- */
void get_did(const char *actor);

void get_everything(const char* post_url);

#endif /* __CRAWLER_H__ */
