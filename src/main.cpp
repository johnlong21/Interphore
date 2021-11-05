#include "gameEngine.h"
#include "debugOverlay.cpp"
#include "textArea.cpp"
#include "unzipper.cpp"
#include "game.cpp"

void updateMain();
void initMain();
void exitMain();
extern "C" void entryPoint();

#ifdef SEMI_WIN32
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    platformArgC = __argc;
    platformArgV = __argv;
#else
int main(int argc, char **argv) {
	platformArgC = argc;
	platformArgV = argv;
#endif
	entryPoint();
	return 0;
}

void entryPoint() {
	initEngine(initMain, updateMain);
}

void initMain() {
	engine->exitCallback = exitMain;

	initGame();
}

void updateMain() {
	updateGame();
}

void exitMain() {
	deinitGame();
}
