#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <jansson.h>

#define V_HTTP 1
#define V_PROFILE 1
#define V_APP 1
#define V_INSECURE 1
#define V_MAX_APPNAME 256
#define V_URL_BASE "https://api.test.vements.io"
#define V_URL_MAX 2048
#define V_AUTH_USERNAME "7scmjqeznftepc4dg52pcuxvsei8xaad"
#define V_AUTH_PASSWORD "7XNR7Z6neyWayc86CzWt8ZUaxFFe2xtD"
#define V_ERR_MAX 512
#define V_ERR_BAD_JSON "Could not parse JSON."
#define V_ERR_CREATE "Could not create %s."
#define V_ERR_GET "Could not get %s."
#define V_ERR_INCORRECT_JSON "Incorrect JSON response."
#define V_ERR_NOT_FOUND "URL not found."
#define V_ERR_ACCESS_DENIED "URL Access denied."


static char _errmsg[V_ERR_MAX];
/* temp internal buffer for sprintf, limited by IE */
static char url_buf[V_URL_MAX];


struct vements_buffer {
  char *ptr;
  size_t size;
  size_t pos;
};

/* store these here to avoid extra calls to malloc */
static struct vements_buffer *_recv_buffer = NULL;
static struct vements_buffer *_send_buffer = NULL;

static struct vements_buffer *_buffer_create(size_t size) {
  struct vements_buffer *x;
  x = malloc(sizeof(struct vements_buffer));
  x->ptr = (char *) malloc(size);
  x->size = size;
  x->pos = 0;
  return x;
}

static void _buffer_reset(struct vements_buffer *x) {
  for(size_t i=0; i < x->size; i++) {
    x->ptr[i] = 0;
  }
  x->pos = 0;
}
static size_t _buffer_write(struct vements_buffer *x, const void *d, size_t n) {
  /* grow buffer size as needed */
  size_t new_len = x->pos + n;
  if(new_len > x->size) {
    x->ptr = (char *) realloc(x->ptr, new_len+1);
    x->size = new_len + 1;
  }
  if(x->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }
  memcpy(x->ptr + x->pos, d, n);
  return n;
}

static size_t _buffer_read(struct vements_buffer *x, void *d, size_t n) {
  size_t to_read = 0;
  if(n > x->pos - x->size) {
    to_read = x->pos - x->size;
  } else {
    to_read = n;
  }
  memcpy(d, x->ptr, n);
  return n;
}

static void _buffer_free(struct vements_buffer *x) {
  if(x) {
    if(x->ptr) {
      free(x->ptr);
    }
    free(x);
  }
}


static size_t _cb_write_buffer(void *ptr, size_t size, size_t n, void *buf) {
  return _buffer_write((struct vements_buffer *) buf, ptr, size * n);
}

static size_t _cb_read_buffer(void *ptr, size_t size, size_t n, void *buf) {
  return _buffer_read((struct vements_buffer *) buf, ptr, size * n);
}

/* returned buffer value is only valid until next http call! */
static const char *get_json(const char *url) {

  CURL *curl = NULL;
  CURLcode res;
  if(!(curl = curl_easy_init())) {
    return NULL;
  }

  _buffer_reset(_recv_buffer);

  printf("********** get_json: %s\n", url);

  /* init the curl session */ 
  curl = curl_easy_init();

#if V_INSECURE
  /* ignore ssl host check */
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#endif
 
  /* set URL to get */ 
  curl_easy_setopt(curl, CURLOPT_URL, url);
 
  /* no progress meter please (stdout?) */ 
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
 
  /* send all data to this function  */ 
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _cb_write_buffer);

  /** callback data **/
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, _recv_buffer);

  /* http call is blocking */
  res = curl_easy_perform(curl);

  return _recv_buffer->ptr;
}

static const char *put_json(const char *url, const char *data) {

  CURL *curl = NULL;
  CURLcode res;
  if(!(curl = curl_easy_init())) {
    sprintf(_errmsg, "Could not create curl object\n");
    return NULL;
  }

  _buffer_reset(_send_buffer);
  _buffer_reset(_recv_buffer);

#if V_INSECURE
  /* ignore ssl host check */
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#endif

  /* enable uploading */ 
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
 
  /* HTTP PUT please */ 
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
 
  /* specify target URL, and note that this URL should include a file
     name, not only a directory */ 
  curl_easy_setopt(curl, CURLOPT_URL, url);

  /* we want to use our own read function */ 
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, _cb_read_buffer);
  
  /* now specify which file to upload */ 
  curl_easy_setopt(curl, CURLOPT_READDATA, _send_buffer);
 
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
    return NULL;
  }

  /* always cleanup */ 
  curl_easy_cleanup(curl);

  return _recv_buffer->ptr;
}



/* http://curl.haxx.se/libcurl/c/post-callback.html */
static const char *post_json(const char *url, const char *data) {

  CURL *curl = NULL;
  CURLcode res;
  struct curl_httppost *formpost=NULL;
  struct curl_httppost *lastptr=NULL;
  struct curl_slist *headerlist=NULL;
  static const char buf[] = "Expect:";

  printf("********** post_json: %s, %s\n", url, data);


  if(!(curl = curl_easy_init())) {
    sprintf(_errmsg, "Could not create curl object\n");
    return NULL;
  }

  _buffer_reset(_send_buffer);
  _buffer_write(_send_buffer, data, strlen(data)); /* realloc */
  _send_buffer->pos = 0;

#if V_INSECURE
  /* ignore ssl host check */
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#endif

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
 
  /* send data */
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, _cb_read_buffer);
  curl_easy_setopt(curl, CURLOPT_READDATA, _send_buffer);

  /* recieve data */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _cb_write_buffer); 
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, _recv_buffer);

  /* basic auth */
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_USERNAME, V_AUTH_USERNAME);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, V_AUTH_PASSWORD);

  /* Set the expected POST size. If you want to POST large amounts of data,
     consider CURLOPT_POSTFIELDSIZE_LARGE */ 
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, _send_buffer->size);
 
  /* get verbose debug output please */ 
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  {
    struct curl_slist *chunk = NULL;
  
    chunk = curl_slist_append(chunk, "Expect:");
    res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    /* use curl_slist_free_all() after the *perform() call to free this
       list again */ 
  }

  /* report errors in return value */
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);

  /* http call is blocking */
  res = curl_easy_perform(curl);  

  /* always cleanup */ 
  curl_easy_cleanup(curl);

  if(res == CURLE_OK) {
    return _recv_buffer->ptr;
  } else if(res == CURLE_HTTP_RETURNED_ERROR) {
    sprintf(_errmsg, V_ERR_NOT_FOUND); /* altough could be other than 404 */
  }
    return NULL;
}

/*************************************
                 PUBLIC 
**************************************/


/* call this if a method returns (int) non-0 or (void *) NULL */
const char *vements_error() {
  return _errmsg;
}


int vements_init() {
  curl_global_init(CURL_GLOBAL_ALL);
  _recv_buffer = _buffer_create(4096);
  _send_buffer = _buffer_create(4096);
  return 0;
}

int vements_cleanup() {
  _buffer_free(_recv_buffer);
  _buffer_free(_send_buffer);
  _recv_buffer = NULL; /* to debug superfluous calls */
  _send_buffer = NULL;
  curl_global_cleanup();
  return 0;
}


#if V_PROFILE
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

  bzero(url_buf, V_URL_MAX);
  sprintf(url_buf, "%s/profile/%s", V_URL_BASE, name);
  s_json = get_json(url_buf);
  if(s_json == NULL) {
    sprintf(_errmsg, "There was an error getting JSON from the server for the profile %s.", name);
    return NULL;
  }

  json = json_loads(s_json, 0, &error);
  if(json == NULL || !json_is_object(json)) {
    sprintf(_errmsg, V_ERR_BAD_JSON);
    return NULL;
  }

  profile = _vements_profile_alloc();
  STRING_VAL(json, "display_name", profile->display_name);
  STRING_VAL(json, "external_url", profile->external_url);
  STRING_VAL(json, "image_url", profile->image_url);
  json_decref(json);
  return profile;
}

#endif

#if V_APP

typedef struct {
  char *root;
  char *namespace;
  char *app;
  char *display_name;
  char *description;
  char *image_url;
  char *external_url;
} vements_app;

vements_app *vements_app_alloc() {
  vements_app *x = malloc(sizeof(vements_app));
  bzero(x, sizeof(vements_app));
  return x;
}


vements_app *vements_app_from_json(const char *s_json) {

  json_t *json;
  json_error_t error;
  vements_app *app = NULL;

  json = json_loads(s_json, 0, &error);
  if(json == NULL) {
    sprintf(_errmsg, V_ERR_BAD_JSON);
    return NULL;
  };

  if(!json_is_object(json)) {
    sprintf(_errmsg, V_ERR_INCORRECT_JSON);
    json_decref(json);
    return NULL;
  }

  app = vements_app_alloc();
  STRING_VAL(json, "root", app->root);
  STRING_VAL(json, "namespace", app->namespace);
  STRING_VAL(json, "root", app->app);
  STRING_VAL(json, "display_name", app->display_name);
  STRING_VAL(json, "description", app->description);
  STRING_VAL(json, "image_url", app->image_url);
  STRING_VAL(json, "external_url", app->external_url);
  json_decref(json);
  return app;
}

vements_app *vements_app_create(const char *profile,
                                const char *namespace,
                                const char *appname) {
  const char *response = NULL;
  char send_buf[V_MAX_APPNAME];

  bzero(url_buf, V_URL_MAX);
  sprintf(url_buf, "%s/profile/%s/namespace/%s/app",
          V_URL_BASE, profile, namespace);
  bzero(send_buf, V_MAX_APPNAME);
  sprintf(send_buf, "{ 'app': '%s' }", appname);
  if(response == post_json(url_buf, send_buf)) {
    sprintf(_errmsg, V_ERR_CREATE, "app");
    return NULL;
  }

  return vements_app_from_json(response);
}

vements_app *vements_app_get(const char *profile, const char *namespace, const char *appname) {
  const char *response = NULL;

  sprintf(url_buf, "%s/profile/%s/namespace/%s/app/%s",
          V_URL_BASE, profile, namespace, appname);
  response = get_json(url_buf);
  if(response == NULL) {
    sprintf(_errmsg, V_ERR_GET, "app");
    return NULL;
  }

  return vements_app_from_json(response);
}

void vements_app_free(vements_app *x) {
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


void print_app(vements_app *app) {
  printf("APP root: %s\n", app->root);
  printf("APP namespace: %s\n", app->namespace);
  printf("APP description: %s\n", app->description);
  printf("APP external_url: %s\n", app->external_url);
  printf("APP display_name: %s\n", app->display_name);
  printf("APP image_url: %s\n", app->image_url);
  printf("APP app: %s\n", app->app);
}

int main() {

  time_t timekey = time(NULL);
  size_t tmp_size = 256;
  char tmp_buf[tmp_size];
  char namespace_name[256];
  const char *response = NULL;
  vements_profile *profile = NULL;
  vements_app *app = NULL;

  /* called once per app */
  vements_init();

  sprintf(namespace_name, "zz5-%ld", timekey);

#if V_HTTP
  /* fetch a profile */
  sprintf(url_buf, "%s/profile/alice-appleseed", V_URL_BASE);
  response = get_json(url_buf);
  printf("GET: %s\n", response);

  /* put_data = "{ \"hey\": \"you\", \"guys\": 12345 }"; */
  /* put_err = put_json(put_url, put_data); */
  /* printf("PUT: %s\n", get_ret); */

  /* create a namespace */ 
  sprintf(url_buf, "%s/profile/alice-appleseed/namespace", V_URL_BASE);
  sprintf(tmp_buf, "{ 'namespace': '%s' }", namespace_name);
  response = post_json(url_buf, tmp_buf);
  if(response) {
    printf("NAMESPACE (post): %s\n", response);
  } else {
    printf("NAMESPACE (post) [error]: %s\n", vements_error());
    goto end_error;
  }

  /* create an app */ 
  sprintf(url_buf, "%s/profile/alice-appleseed/namespace/%s/app", V_URL_BASE, namespace_name);
  sprintf(tmp_buf, "{ 'app': 'my-new-app-%ld' }", timekey);
  response = post_json(url_buf, tmp_buf);
  if(response) {
    printf("APP (post): %s\n", response);
  } else {
    printf("APP (post) [error]: %s\n", vements_error());
    goto end_error;
  }
#endif
  

  return 0;

  /* test vements API */

#if V_PROFILE
  /* test vements profile */
  profile = vements_profile_get("alice-appleseed");
  printf("ALICE display_name: %s\n", profile->display_name);
  printf("ALICE external_url: %s\n", profile->external_url);
  vements_profile_free(profile);
#endif

#if V_APP
  /* test vements app */
  bzero(tmp_buf, tmp_size);
  sprintf(tmp_buf, "my-new-app-%ld", timekey);
  app = vements_app_create("alice-appleseed", "zz5", tmp_buf);
  if(app == NULL)
    goto end_error;
  print_app(app);
  vements_app_free(app);

  /* now try to retrieve our new app */
  app = vements_app_get("alice-appleseed", "zz5", tmp_buf);
  if(app == NULL)
    goto end_error;
  print_app(app);
  vements_app_free(app);
#endif


end_error:
  /* called once per app */
  vements_cleanup();
  return 1;

end:
  /* called once per app */
  vements_cleanup();
  return 0;
}
