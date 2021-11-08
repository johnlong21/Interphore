#include "mintSprite.hpp"

struct DebugOverlay {
    bool active;
    int bottomLayer;

    MintSprite *info;

    void update();
};

void initDebugOverlay(DebugOverlay *newOverlay);
