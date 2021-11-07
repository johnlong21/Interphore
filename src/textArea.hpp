#include "mintSprite.h"
#include "text.h"

#define TEXT_AREA_MODES_LIMIT 16

enum TextAreaMode {
    TEXT_MODE_NONE,
    TEXT_MODE_JIGGLE,
    TEXT_MODE_ZOOM_OUT,
    TEXT_MODE_ZOOM_IN,
    TEXT_MODE_RAINBOW,
    TEXT_MODE_WAVE,
};

struct TextArea {
    bool exists;
    MintSprite *sprite;

    CharRenderDef defs[HUGE_STR];
    int defsNum;
    int textWidth;
    int textHeight;
    const char *defaultFont;
    int fontColour;

    TextAreaMode modes[TEXT_AREA_MODES_LIMIT];
    int modesNum;
    float modeStartTime;

    int jiggleX;
    int jiggleY;
    float zoomTime;
    int waveX;
    int waveY;
    float waveSpeed;

    void resize(int width, int height);
    void setFont(const char *fontName);
    void setText(const char *text);
    void addMode(TextAreaMode mode);
    void resetModes();
    void update();
};

void initTextArea(TextArea *area);
