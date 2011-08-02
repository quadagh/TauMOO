/************************** MOO HTTP v 1.0 ****************************************
 * copyright 2011, Luc Somers. this program is released under the BSD license     *
 */

#include "ext-http.h"

static size_t
WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)data;
 
  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    exit(EXIT_FAILURE);
  }
 
  memcpy(&(mem->memory[mem->size]), ptr, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

static package bf_http_request( Var arglist, Byte next, void *vdata, Objid progr)
{
  CURL *curl_handle;
  CURLcode ok;
  package result;
  if (!is_wizard(progr))
  {
    free_var(arglist);
    return make_error_pack(E_PERM);
  }
  int nargs = arglist.v.list[0].v.num;
  const char *address = arglist.v.list[1].v.str;
  const char *agent="",*postfields="",*cookies="";
  int headers = 0;
  switch(nargs)
  {
    case 5:
        cookies = arglist.v.list[5].v.str;
    case 4:
	postfields = arglist.v.list[4].v.str;
    case 3:
	agent = arglist.v.list[3].v.str;
    case 2:
	headers = arglist.v.list[2].v.num;
  }
  if(!strlen(agent))
    agent = "MOO-http/1.0";
  const char delimiters[] = "\n";
  free_var(arglist);

  struct MemoryStruct chunk;

  chunk.memory = malloc(1);
  chunk.size = 0;

  curl_global_init(CURL_GLOBAL_ALL);

  curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_URL, address);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, agent);
  curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 5);
  curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 3);
  curl_easy_setopt(curl_handle, CURLOPT_FTP_RESPONSE_TIMEOUT, 3);
  curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5);
  curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(curl_handle, CURLOPT_MAXFILESIZE, 1048576);
  if(strlen(postfields))
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, postfields);
  if(strlen(cookies))
    curl_easy_setopt(curl_handle, CURLOPT_COOKIE, cookies);
  if(headers)
    curl_easy_setopt(curl_handle, CURLOPT_HEADER, 1);
  ok = curl_easy_perform(curl_handle);
  curl_easy_cleanup(curl_handle);

  if(ok == CURLE_OK && strlen(chunk.memory) != chunk.size)
    ok = CURLE_BAD_CONTENT_ENCODING;   // binary !!!

  if(ok == CURLE_OK)
  {
    char *token,*p=chunk.memory;
    Var r;
    r.type = TYPE_LIST;
    r = new_list(0);
    token = strsep(&p, delimiters);
    while( token != NULL )
    {
	Var line;
	line.type = TYPE_STR;
	if(token[strlen(token)-1] == '\r')
	    token[strlen(token)-1] = '\0';
	//run it through utf8_substr to get rid of invalid utf8
	line.v.str = (char *)utf8_substr(token,1,utf8_strlen(token));
	r = listappend(r, var_dup(line));
	token = strsep(&p, delimiters);
	free_var(line);
    }
    result = make_var_pack(r);
  }
  else
  {
    Var r;
    r.type = TYPE_INT;
    r.v.num = ok;
    result = make_raise_pack(E_INVARG,
			curl_easy_strerror(ok),
			r);
  }

  if(chunk.memory)
    free(chunk.memory);
    
  curl_global_cleanup();
  
  return result;
}

static package bf_urlencode( Var arglist, Byte next, void *vdata, Objid progr)
{
  Var r;
  CURL *curl_handle;
  const char *string = arglist.v.list[1].v.str;
  free_var(arglist);
  
  curl_global_init(CURL_GLOBAL_ALL);
  curl_handle = curl_easy_init();

  r.type = TYPE_STR;
  char *output = curl_easy_escape(curl_handle,string,0);
  if(output == NULL)
  {
    curl_free(output);
    return make_error_pack(E_INVARG);
  }
  r.v.str = str_dup(output);
  curl_free(output);

  curl_easy_cleanup(curl_handle);
  curl_global_cleanup();

  return make_var_pack(r);
}

static package bf_urldecode( Var arglist, Byte next, void *vdata, Objid progr)
{
  Var r;
  CURL *curl_handle;
  const char *string = arglist.v.list[1].v.str;
  free_var(arglist);
  
  curl_global_init(CURL_GLOBAL_ALL);
  curl_handle = curl_easy_init();

  r.type = TYPE_STR;
  char *output = curl_easy_unescape(curl_handle,string,0,NULL);
  if(output == NULL)
  {
    curl_free(output);
    return make_error_pack(E_INVARG);
  }
  r.v.str = str_dup(output);
  curl_free(output);

  curl_easy_cleanup(curl_handle);
  curl_global_cleanup();

  return make_var_pack(r);
}


void register_http(void)
{
	(void) register_function("http_request", 1, 5, bf_http_request, TYPE_STR, TYPE_INT, TYPE_STR, TYPE_STR, TYPE_STR);
	(void) register_function("urlencode", 1, 1, bf_urlencode, TYPE_STR);
	(void) register_function("urldecode", 1, 1, bf_urldecode, TYPE_STR);

}
