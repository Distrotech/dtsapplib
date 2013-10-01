#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include "dtsapp.h"

static void *curl_isinit = NULL;
static CURL *curl = NULL;

static struct curl_progress {
	void *data;
	curl_progress_func cb;
	curl_progress_newdata d_cb;
	curl_progress_pause p_cb;
} *curlprogress = NULL;


struct curl_post {
	struct curl_httppost *first;
	struct curl_httppost *last;
};

static size_t bodytobuffer(void *ptr, size_t size, size_t nmemb, void *userdata) {
	size_t bufsize = size * nmemb;
	struct curlbuf *mem = (struct curlbuf *)userdata;

	if (!(mem->body = realloc(mem->body, mem->bsize + bufsize + 1))) {
		return 0;
	}
	memcpy(&(mem->body[mem->bsize]), ptr, bufsize);
	mem->bsize += bufsize;
	mem->body[mem->bsize] = '\0';
	return bufsize;
}

static size_t headertobuffer(void *ptr, size_t size, size_t nmemb, void *userdata) {
	size_t bufsize = size * nmemb;
	struct curlbuf *mem = (struct curlbuf *)userdata;

	if (!(mem->header = realloc(mem->header, mem->hsize + bufsize + 1))) {
		return 0;
	}
	memcpy(&(mem->header[mem->hsize]), ptr, bufsize);
	mem->hsize += bufsize;
	mem->header[mem->hsize] = '\0';
	return bufsize;
}

static void curlfree(void *data) {
	if (curl) {
		curl_easy_cleanup(curl);
	}
}

int curlinit(void) {
	if (curl_isinit) {
		return objref(curl_isinit);
	}

	if (!(curl_isinit = objalloc(sizeof(void *),curlfree))) {
		return 0;
	}

	objlock(curl_isinit);
	if (!(curl = curl_easy_init())) {
		objunlock(curl_isinit);
		objunref(curl_isinit);
		return 0;
	}

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0 [Distro Solutions]");

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bodytobuffer);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headertobuffer);
	objunlock(curl_isinit);
	return 1;
}

void curlclose(void) {
	objunref(curl_isinit);
	if (curlprogress) {
		objunref(curlprogress);
	}
}

static void emptybuffer(void *data) {
	struct curlbuf *writebuf = data;

	if (!writebuf) {
		return;
	}

	if (writebuf->body) {
		free(writebuf->body);
	}

	if (writebuf->header) {
		free(writebuf->header);
	}

	writebuf->body = NULL;
	writebuf->header = NULL;
	writebuf->bsize = 0;
	writebuf->hsize = 0;
}

static struct curlbuf *curl_sendurl(const char *def_url, struct basic_auth *bauth, struct curl_post *post, curl_authcb authcb,void *auth_data) {
	long res;
	int i = 0;
	struct basic_auth *auth = bauth;
	struct curlbuf *writebuf;
	char userpass[64];
	char *url;
	void *p_data = NULL;
	/*    char buffer[1024];
	    struct curl_slist *cookies, *nc;*/

	if (!curlinit()) {
		return NULL;
	}

	if (!(writebuf = objalloc(sizeof(*writebuf), emptybuffer))) {
		objunref(curl_isinit);
		return NULL;
	}

	objlock(curl_isinit);
	curl_easy_setopt(curl, CURLOPT_URL, def_url);
	/*    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, buffer);*/

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, writebuf);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, writebuf);

	if (post) {
		objlock(post);
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, post->first);
	}

	if (auth && auth->user && auth->passwd) {
		snprintf(userpass, 63, "%s:%s", auth->user, auth->passwd);
	   	curl_easy_setopt(curl, CURLOPT_USERPWD, userpass);
		i++;
	} else if (!auth) {
		auth = curl_newauth(NULL, NULL);
	}

	if (curlprogress && ((p_data = curlprogress->d_cb(curlprogress->data)))) {
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
		curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curlprogress->cb);
		curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, p_data);
	}

	do {
		if (!(res = curl_easy_perform(curl))) {
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);
			switch (res) {
				/*needs auth*/
				case 401:
					if (curlprogress && curlprogress->p_cb) {
						curlprogress->p_cb(p_data, 1);
					}
					if ((authcb) && ((auth = authcb((auth) ? auth->user : "", (auth) ? auth->passwd : "", auth_data)))) {
						snprintf(userpass, 63, "%s:%s", auth->user, auth->passwd);
						curl_easy_setopt(curl, CURLOPT_USERPWD, userpass);
						emptybuffer(writebuf);
					} else {
						i=3;
					}

					if (curlprogress && curlprogress->p_cb) {
						curlprogress->p_cb(p_data, 0);
					}
					break;
				/*not found*/
				case 300:
					i=3;
					break;
				/*redirect*/
				case 301:
					curl_easy_getinfo(curl,CURLINFO_REDIRECT_URL, &url);
					curl_easy_setopt(curl, CURLOPT_URL, url);
					emptybuffer(writebuf);
					i--;
					break;
				/*ok*/
				case 200:
					curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &writebuf->c_type);
					break;
			}
		}
		i++;
	} while ((res != 200) && (i < 3));

	/*    curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies);
	    for(nc = cookies; nc; nc=nc->next) {
	        printf("%s\n", nc->data);
	    }*/

	if (!bauth) {
		objunref(auth);
	}

	if (post) {
		objunlock(post);
		objunref(post);
	}

	if (curlprogress && curlprogress->p_cb) {
		curlprogress->p_cb(p_data, -1);
	}

	if (p_data) {
		objunref(p_data);
	}

	objunlock(curl_isinit);
	objunref(curl_isinit);
	return writebuf;
}

struct curlbuf *curl_geturl(const char *def_url, struct basic_auth *bauth, curl_authcb authcb,void *auth_data) {
	return curl_sendurl(def_url, bauth, NULL, authcb, auth_data);
}

struct curlbuf *curl_posturl(const char *def_url, struct basic_auth *bauth, struct curl_post *post, curl_authcb authcb,void *auth_data) {
	return curl_sendurl(def_url, bauth, post, authcb, auth_data);
}

struct curlbuf *curl_ungzip(struct curlbuf *cbuf) {
	uint8_t *gzbuf;
	uint32_t len;

	if (is_gzip((uint8_t *)cbuf->body, cbuf->bsize) &&
			((gzbuf = gzinflatebuf((uint8_t *)cbuf->body, cbuf->bsize, &len)))) {
		free(cbuf->body);
		cbuf->body = gzbuf;
		cbuf->bsize = len;
	}
	return cbuf;
}

static void curl_freeauth(void *data) {
	struct basic_auth *bauth = (struct basic_auth *)data;
	if (!bauth) {
		return;
	}
	if (bauth->user) {
		memset((void *)bauth->user, 0, strlen(bauth->user));
		free((void *)bauth->user);
	}
	if (bauth->passwd) {
		memset((void *)bauth->passwd, 0, strlen(bauth->passwd));
		free((void *)bauth->passwd);
	}
}

struct basic_auth *curl_newauth(const char *user, const char *passwd) {
	struct basic_auth *bauth;

	if (!(bauth = (struct basic_auth *)objalloc(sizeof(*bauth), curl_freeauth))) {
		return NULL;
	}
	if (user) {
		bauth->user = strdup(user);
	} else {
		bauth->user = strdup("");
	}
	if (passwd) {
		bauth->passwd = strdup(passwd);
	} else {
		bauth->passwd = strdup("");
	}
	return bauth;
}

void free_post(void *data) {
	struct curl_post *post = data;
	if (post->first) {
		curl_formfree(post->first);
	}
}

extern struct curl_post *curl_newpost(void) {
	struct curl_post *post;
	if (!(post = objalloc(sizeof(*post), free_post))) {
		return NULL;
	}
	post->first = NULL;
	post->last = NULL;
	return post;
}

void curl_postitem(struct curl_post *post, const char *name, const char *item) {
	if (!name || !item) {
		return;
	}
	objlock(post);
	curl_formadd(&post->first, &post->last,
		CURLFORM_COPYNAME, name,
		CURLFORM_COPYCONTENTS, item,
		CURLFORM_END);
	objunlock(post);
}

extern char *url_escape(char *url) {
	char *esc;

	if (!curlinit()) {
		return NULL;
	}

	objlock(curl_isinit);
	esc = curl_easy_escape(curl, url, 0);
	objunlock(curl_isinit);
	objunref(curl_isinit);
	return esc;
}

extern char *url_unescape(char *url) {
	char *uesc;

	if (!curlinit()) {
		return NULL;
	}

	objlock(curl_isinit);
	uesc = curl_easy_unescape(curl, url, 0, 0);
	objunlock(curl_isinit);
	objunref(curl_isinit);
	return uesc;
}

void free_progress(void *data) {
	if (data) {
		objunref(data);
	}
}

void curl_setprogress(curl_progress_func cb, curl_progress_pause p_cb, curl_progress_newdata d_cb, void *data) {
	if (curlprogress) {
		objunref(curlprogress);
		curlprogress = NULL;
	}

	if (!(curlprogress = objalloc(sizeof(*curlprogress), free_progress))) {
		return;
	}
	curlprogress->cb = cb;
	curlprogress->d_cb = d_cb;
	curlprogress->p_cb = p_cb;
	if (data && objref(data)) {
		curlprogress->data = data;
	}
}
