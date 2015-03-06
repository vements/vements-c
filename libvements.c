#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define URL "https://vements.ngrok.com/"


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


/* temp internal buffer for sprintf */
static char *url_buf[4096];

vements_profile *vements_profile_alloc() {
  vements_profile *x = malloc(sizeof(vements_profile));
  bzero(x, sizeof(vements_profile));
  return x;
}

void vements_profile_dealloc(vements_profile *x) {
  if(x) {
    if(x->display_name) dealloc(x->display_name);
    if(x->external_url) dealloc(x->external_url);
    if(x->image_url) dealloc(x->image_url);
    if(x->location) dealloc(x->location);

    /* ,,, etc */
      
    dealloc(x);
  }
}

vements_profile *vements_profile_get(const char *name) {
  sprintf(url_buf, "http://vements.ngrok.com/profile/%s", name);

  const char *s_json = NULL;
  json_t *json = NULL;
  vements_profile *profile = vements_profile_alloc();

  /* just psuedocode */
  json = parse_json(get_json(url_buf));
  if(json == NULL)
    return;
  for(int i=0; i < json->n_keys; i++) {
    if(strcmp(json->keys[i], "display_name") == 0) {
      strcpy(profile->display_name, json->values[i]);
    }
    else if(strcmp(json->keys[i], "external_url") == 0) {
      strcpy(profile->external_url, json->values[i]);
    }
    else if(strcmp(json->keys[i], "image_url") == 0) {
      strcpy(profile->image_url, json->values[i]);
    }
  }
}

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

  sprintf(url_buffer, "https://vements.ngrok.com/profile/%s/namespace/%s/app/%s",
          profile, namespace, appname);
  s_json = post_json(url_buffer);
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
  /* just psuedocode */
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

void *vements_app_dealloc(vements_app *x) {
  if(x) {
    if(x->root) dealloc(x->root);
    if(x->namespace) dealloc(x->namespace);
    if(x->app) dealloc(x->app);
    if(x->display_name) dealloc(x->display_name);
    if(x->description) dealloc(x->description);
    if(x->image_url) dealloc(x->image_url);
    if(x->external_url) dealloc(x->external_url);      
    dealloc(x);
  }
}



/** FILL OUT THE REST OF THE API TYPES HERE **/

 /*******************************
               DEV / TEST
 *************************************/

int main() {

  const char *get_url = "https://vements.ngrok.com/profile/alice-appleseed";
  const char *get_ret = NULL;

  const char *put_url = "";
  const char *put_data = "{ \"hey\": \"you\", \"guys\": 12345 }";
  int put_err = -1;

  vements_profile *alice = NULL;
  vements_app *my_new_app = NULL;

  /* called once per app */
  vements_init();

  /* fetch a profile */
  get_ret = get_json(get_url);
  printf("GET: %s\n", get_ret);

  /* put_data = "{ \"hey\": \"you\", \"guys\": 12345 }"; */
  /* put_err = put_json(put_url, put_data); */
  /* printf("PUT: %s\n", get_ret); */

  /* test vements API */

  /* test vements profile */
  alice = vements_profile_get("alice-appleseed");
  printf("ALICE display_name: %s\n", alice->display_name);
  printf("ALICE external_url: %s\n", alice->external_url);
  vements_profile_dealloc(alice);

  /* test vements app */
  my_new_app = vements_app_create("my-new-app");
  /* do something with it here? */
  vements_app_dealloc(my_new_app);

  my_new_app = vements_app_get("my-new-app");
  printf("APP display_name: %s\n", my_new_app->display_name);
  printf("APP root: %s\n", my_new_app->root);
  printf("APP namespace: %s\n", my_new_app->namespace);
  vements_app_dealloc(my_new_app);

  /* called once per app */
  vements_cleanup();
}
