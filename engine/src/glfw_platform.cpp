#include "platform.h"
#include "engine.h"

void *initPlatform(int mem) {
	printf("Plat initing\n");

	/* Initialize the library */
	if (!glfwInit()) {
		printf("Could not initialize library\n");
		// return -1;
	}

	/* Create a windowed mode window and its OpenGL context */
	if(glfwOpenWindow(640, 480, 0,0,0,0,16,0, GLFW_WINDOW) != GL_TRUE) {
		printf("Could not create OpenGL window\n");
		glfwTerminate();
		// return -1;
	}

	platPlatform = PLAT_GLFW;

	void *memory = Malloc(mem);
	memset(memory, 0, mem);
	return memory;
}

void platformStartFrame() {
}

void platformEndFrame() {
	glfwSwapBuffers();
}

unsigned long platformGetTime() {
	return 0;
}

void platformSleepMs(int millis) {
}

void platformSaveToTemp(char *str) {
}

char *platformLoadFromTemp() {
	return NULL;
}

void platformSaveToDisk(char *str) {
}

void platformLoadFromDisk(void (*loadCallback)(char *)) {
}

void platformClearTemp() {
}

void gotoUrl(const char *url) {
}
