#include <unistd.h>

#include "ems_crawler.h"

/* placeholder url */
#define POST_URL "https://bsky.app/profile/raysan5.bsky.social/post/3le4og7pvh22w"

int main(void) {
    get_everything(POST_URL);

    return 0;
}
