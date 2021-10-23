#include "platform.h"
#include "networking.h"
#include "curl/curl.h"

struct CurlMemoryStruct {
	char *memory;
	size_t size;
};

size_t curlWriteFunc(void *contents, size_t size, size_t nmemb, void *userp);

void loadFromUrl(const char *url, void (*loadCallback)(char *, int)) {
	CURL *curl = curl_easy_init();
	Assert(curl);

	CurlMemoryStruct chunk;
	chunk.memory = (char *)Malloc(1);
	chunk.size = 0; 

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFunc);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

	CURLcode res = curl_easy_perform(curl);

	if (res != CURLE_OK) printf("curl_easy_perform() failed: %s(%d)\n", curl_easy_strerror(res), res);

	curl_easy_cleanup(curl);
	// printf("%lu bytes retrieved\n", (long)chunk.size);
	platformLoadedStringSize = chunk.size;
	loadCallback(chunk.memory, chunk.size);
}

size_t curlWriteFunc(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	CurlMemoryStruct *mem = (struct CurlMemoryStruct *)userp;

	char *newMem = (char *)Malloc(mem->size + realsize + 1);
	memcpy(newMem, mem->memory, mem->size);
	Free(mem->memory);
	mem->memory = newMem;
	// mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL) {
		printf("not enough memory (realloc returned NULL)\n");
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}
