/************************** MOO HTTP v 1.0 ****************************************
 * copyright 2011, Luc Somers. this program is released under the BSD license     *
 */
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "my-time.h"
#include "my-string.h"
#include "config.h"
#include "functions.h"
#include "log.h"
#include "random.h"
#include "storage.h"
#include "utils.h"
#include "dirent.h"
#include "unparse.h"
#include "list.h"
#include "regexpr.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <errno.h>
#include "version.h"
#include <string.h>
#include <curl/curl.h>

struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t
WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data);


