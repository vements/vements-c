#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>

#define V_APP 0
#define URL_BASE "https://vements.ngrok.com"


static char _errmsg[1024];


typedef struct  {
  char *ptr;
  size_t size;
  size_t pos;
} _buffer_t;

static _buffer_t *_buffer = NULL;

static void _buffer_init() {
  _buffer = malloc(sizeof(_buffer_t));
  _buffer->ptr = (char *) malloc(4096);
  _buffer->size = 4096;
}

static void _buffer_reset() {
  for(size_t i=0; i < _buffer->size; i++) {
    _buffer->ptr[i] = 0;
  }
}
static void _buffer_write(void *d, size_t n) {
  /* grow buffer size as needed */
  size_t new_len = _buffer->pos + n;
  if(new_len > _buffer->size) {
    _buffer->ptr = (char *) realloc(_buffer->ptr, new_len+1);
    _buffer->size = new_len + 1;
  }
  if(_buffer->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }
  memcpy(_buffer->ptr + _buffer->pos, d, n);
}


static size_t _read_callback(void *ptr, size_t size, size_t n, void *cb_data /* NULL */) {
  _buffer_write(ptr, size * n);
  return size * n;
}

/* returned buffer value is only valid until next http call! */
static const char *get_json(const char *url) {

  CURL *curl = NULL;
  CURLcode res;
  if(!(curl = curl_easy_init())) {
    return NULL;
  }

  _buffer_reset();

  printf("get_json: %s\n", url);


  /* init the curl session */ 
  curl = curl_easy_init();
 
  /* set URL to get */ 
  curl_easy_setopt(curl, CURLOPT_URL, url);
 
  /* no progress meter please (stdout?) */ 
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
 
  /* send all data to this function  */ 
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _read_callback);

  /** callback data **/
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

  /* http call is blocking */
  res = curl_easy_perform(curl);

  return _buffer->ptr;
}


static int put_json(const char *url, const char *data) {

  CURL *curl = NULL;
  CURLcode res;
  if(!(curl = curl_easy_init())) {
    sprintf(_errmsg, "Could not create curl object\n");
    return 1;
  }

  _buffer_reset();


  /* we want to use our own read function */ 
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, _read_callback);
 
  /* enable uploading */ 
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
 
  /* HTTP PUT please */ 
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
 
  /* specify target URL, and note that this URL should include a file
     name, not only a directory */ 
  curl_easy_setopt(curl, CURLOPT_URL, url);
 
  /* now specify which file to upload */ 
  /* curl_easy_setopt(curl, CURLOPT_READDATA, hd_src); */
 
  /* provide the size of the upload, we specicially typecast the value
     to curl_off_t since we must be sure to use the correct data size */ 
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                   (curl_off_t) strlen(data));

  /* Now run off and do what you've been told! */ 
  res = curl_easy_perform(curl);
  /* Check for errors */ 
  if(res != CURLE_OK) {
    sprintf(_errmsg, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    return 1;
  }

  /* always cleanup */ 
  curl_easy_cleanup(curl);

  return 0;
}


/* http://curl.haxx.se/libcurl/c/post-callback.html */
static int post_json(const char *url, const char *data) {

  CURL *curl = NULL;
  CURLcode res;
  struct curl_httppost *formpost=NULL;
  struct curl_httppost *lastptr=NULL;
  struct curl_slist *headerlist=NULL;
  static const char buf[] = "Expect:";

  if(!(curl = curl_easy_init())) {
    sprintf(_errmsg, "Could not create curl object\n");
    return 1;
  }

  _buffer_reset();


  

  /* initalize custom header list (stating that Expect: 100-continue is not
     wanted */ 
  headerlist = curl_slist_append(headerlist, buf);


  /* always cleanup */ 
  curl_easy_cleanup(curl);

  return 0;
}

/*************************************
                 PUBLIC 
**************************************/


/* call this if a method returns (int) non-0 or (void *) NULL */
const char *vements_get_error_msg() {
  return _errmsg;
}


int vements_init() {
  curl_global_init(CURL_GLOBAL_ALL);
  _buffer_init();
  return 0;
}

int vements_cleanup() {
  curl_global_cleanup();
  return 0;
}


typedef struct {

  char *display_name;
  char *external_url;
  char *image_url;
  char *location;
  char *owner;
  char *perm_delete;
  char *perm_read;
  char *perm_update;
  char *profile;

  char *created;
  char *updated;
  
} vements_profile;


static vements_profile *_vements_profile_alloc() {
  vements_profile *x = malloc(sizeof(vements_profile));
  bzero(x, sizeof(vements_profile));
  return x;
}

void vements_profile_free(vements_profile *x) {
  if(x) {
    if(x->display_name) free(x->display_name);
    if(x->external_url) free(x->external_url);
    if(x->image_url) free(x->image_url);
    if(x->location) free(x->location);
    if(x->owner) free(x->owner);
    if(x->perm_delete) free(x->perm_delete);
    if(x->perm_read) free(x->perm_read);
    if(x->perm_update) free(x->perm_update);
    if(x->profile) free(x->profile);
    if(x->created) free(x->created);
    if(x->updated) free(x->updated);
    free(x);
  }
}

/* temp internal buffer for sprintf */
static char url_buf[4096];

static json_t *_tmp;
#define STRING_VAL(json, key, s) \
    _tmp = json_object_get(json, key); \
    if(json_is_string(_tmp)) { \
        const char *ss = json_string_value(_tmp); \
        s = (char *) malloc(strlen(ss)); \
        strcpy(s, ss); \
    }



vements_profile *vements_profile_get(const char *name) {

  const char *s_json = NULL;
  json_t *json = NULL;
  json_error_t error;
  vements_profile *profile;

  sprintf(url_buf, "%s/profile/%s", URL_BASE, name);
  s_json = get_json(url_buf);
  if(!s_json) {
    sprintf(_errmsg, "There was an error getting JSON from the server for the profile %s.", name);
    return NULL;
  }

  json = json_loads(s_json, 0, &error);
  if(json == NULL || !json_is_object(json)) {
    sprintf(_errmsg, "Could not parse json response from server.");
    return NULL;
  }

  profile = _vements_profile_alloc();
  STRING_VAL(json, "display_name", profile->display_name);
  STRING_VAL(json, "external_url", profile->external_url);
  STRING_VAL(json, "image_url", profile->image_url);

  json_decref(json);
  return profile;
}


#if V_APP

typedef struct {
  char *root;
  char *namespace;
  char *app;
  char *display_name;
  char *description;
  char *image_url;
  cahar *external_url;
} vements_app;

vements_app *vements_app_alloc() {
  vements_app *x = malloc(sizeof(vements_app));
  bzero(x, sizeof(vements_app));
  return x;
}

vements_app *vements_app_create(const char *profile,
                                const char *namespace,
                                const char *appname) {
  vements_app *app = NULL;
  const char *s_json = NULL;
  json_t *json = NULL;

  sprintf(url_buf, "https://vements.ngrok.com/profile/%s/namespace/%s/app/%s",
          profile, namespace, appname);
  s_json = post_json(url_buf);
  if(s_json == NULL) {
    sprintf(_errmsg, "Could not create app");
    return NULL;
  }
  
  json = parse_json(s_json);
  if(json == NULL) {
    sprintf(_errmsg, "Got bad json response from server");
    return NULL;
  }

  app = vements_app_alloc();
  json = parse_json(get_json(url_buf));
  if(json == NULL)
    return;
  for(int i=0; i < json->n_keys; i++) {
    if(strcmp(json->keys[i], "display_name") == 0) {
      strcpy(app->display_name, json->values[i]);
    }
    else if(strcmp(json->keys[i], "external_url") == 0) {
      strcpy(app->external_url, json->values[i]);
    }
    else if(strcmp(json->keys[i], "image_url") == 0) {
      strcpy(app->image_url, json->values[i]);
    }
  }  
  return x;
}

void *vements_app_free(vements_app *x) {
  if(x) {
    if(x->root) free(x->root);
    if(x->namespace) free(x->namespace);
    if(x->app) free(x->app);
    if(x->display_name) free(x->display_name);
    if(x->description) free(x->description);
    if(x->image_url) free(x->image_url);
    if(x->external_url) free(x->external_url);      
    free(x);
  }
}
#endif


/** FILL OUT THE REST OF THE API TYPES HERE **/

 /*******************************
               DEV / TEST
 *************************************/

int main() {

  const char *get_ret = NULL;

//  const char *put_url = "";
//  const char *put_data = "{ \"hey\": \"you\", \"guys\": 12345 }";
//  int put_err = -1;

  vements_profile *alice = NULL;
  /* vements_app *my_new_app = NULL; */

  /* called once per app */
  vements_init();

  /* fetch a profile */
  sprintf(url_buf, "%s/profile/alice-appleseed", URL_BASE);
  get_ret = get_json(url_buf);
  printf("GET: %s\n", get_ret);

  /* put_data = "{ \"hey\": \"you\", \"guys\": 12345 }"; */
  /* put_err = put_json(put_url, put_data); */
  /* printf("PUT: %s\n", get_ret); */

  /* test vements API */

  /* test vements profile */
  alice = vements_profile_get("alice-appleseed");
  printf("ALICE display_name: %s\n", alice->display_name);
  printf("ALICE external_url: %s\n", alice->external_url);
  vements_profile_free(alice);

#if V_APP
  /* test vements app */
  my_new_app = vements_app_create("my-new-app");
  /* do something with it here? */
  vements_app_free(my_new_app);

  my_new_app = vements_app_get("my-new-app");
  printf("APP display_name: %s\n", my_new_app->display_name);
  printf("APP root: %s\n", my_new_app->root);
  printf("APP namespace: %s\n", my_new_app->namespace);
  vements_app_free(my_new_app);
#endif

  /* called once per app */
  vements_cleanup();
}
