#include "mintSprite.hpp"
#include "text.hpp"

constexpr int TEXT_AREA_MODES_LIMIT = 16;

enum class TextAreaMode {
    None,
    Jiggle,
    ZoomOut,
    ZoomIn,
    Rainbow,
    Wave
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
