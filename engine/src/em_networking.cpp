#include "networking.h"
#include <emscripten/fetch.h>

void emDownloadSucceeded(emscripten_fetch_t *fetch);
void emDownloadFailed(emscripten_fetch_t *fetch);

void (*emLoadCallback)(char *);

void loadFromUrl(const char *url, void (*loadCallback)(char *)) {
	emLoadCallback = loadCallback;
	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);
	strcpy(attr.requestMethod, "GET");
	attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	attr.onsuccess = emDownloadSucceeded;
	attr.onerror = emDownloadFailed;
	emscripten_fetch(&attr, url);
}

void emDownloadSucceeded(emscripten_fetch_t *fetch) {
	printf("Finished downloading %llu bytes from URL %s.\n", fetch->numBytes, fetch->url);
	// The data is now available at fetch->data[0] through fetch->data[fetch->numBytes-1];
	emscripten_fetch_close(fetch); // Free data associated with the fetch.
}

void emDownloadFailed(emscripten_fetch_t *fetch) {
	printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);
	emscripten_fetch_close(fetch); // Also free data on failure.
}

