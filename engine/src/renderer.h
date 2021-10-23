#pragma once
#include "mintSprite.h"
#include "mintParticleSystem.h"

#ifdef SEMI_GL
struct ShaderProgram {
	GLuint vert;
	GLuint frag;
	GLuint program;
};

struct RendererData {
	unsigned char emptyData[TEXTURE_WIDTH_LIMIT*TEXTURE_HEIGHT_LIMIT*4];

	ShaderProgram defaultProgram;
	ShaderProgram renderTextureProgram;
	ShaderProgram tilemapProgram;
	ShaderProgram particleProgram;
	ShaderProgram *currentProgram;

	GLint a_position;
	GLint a_texcoord;
	GLint u_matrix;
	GLint u_uv;
	GLint u_colour;
	GLint u_tint;
	GLint u_alpha;
	GLint u_hasTexture;
	GLint u_texture;

	GLint rt_a_position;
	GLint rt_a_texcoord;
	GLint rt_u_matrix;
	GLint rt_u_texture;
	GLint rt_u_tint;
	GLint rt_u_bleed;

	GLint tm_a_position;
	GLint tm_u_matrix;
	GLint tm_u_tilesetTexture;
	GLint tm_u_tilemapTexture;
	GLint tm_u_tilesetSize;
	GLint tm_u_tilemapSize;
	GLint tm_u_resultSize;
	GLint tm_u_pixelRatio;

	GLint p_a_position;
	GLint p_a_texcoord;
	GLint p_u_matrix;
	GLint p_u_texture;
	GLint p_u_offset;

	GLuint tempVerts;
	GLuint textureFramebuffer;
	GLuint emptyTexture;
	GLuint emptyTexcoords;
	GLuint defaultTexcoords;

	GLuint viewTexture;
	GLuint viewVerts;

	GLuint currentFramebuffer;
	GLuint currentTexture;
	GLuint currentFramebufferTexture;
	Rect currentViewport;
	GLuint currentArrayBuffer;

	int errorCount;
};
#endif

#ifdef SEMI_FLASH
struct RendererData {
	void *tempAnimMem;
	int tempAnimMemSize;
};
#endif

struct RenderProps {
	int x;
	int y;
	int width;
	int height;
	int dx;
	int dy;
	int tint;
	float scaleX;
	float scaleY;
	bool bleed;
};

#include "mintSprite.h"

void initRenderer(void *rendererMemory);
void rendererStartFrame();
void renderSprites(MintSprite **sprites, int spritesNum, Matrix *scaleMatrix, Matrix *scrollMatrix);
void initEmptySprite(MintSprite *spr);
void generateTexture(void *data, Asset *asset, bool argb=false);
void renderTextureToSprite(const char *assetId, MintSprite *spr, RenderProps *props);
void renderTextureToSprite(Asset *tex, MintSprite *spr, RenderProps *props);
void renderSpriteToSprite(MintSprite *source, MintSprite *dest, RenderProps *props);
void switchSpriteFrame(MintSprite *spr);
void initAnimatedSprite(MintSprite *spr);
void drawTilesOnSprite(MintSprite *spr, const char *assetId, int tileWidth, int tileHeight, int tilesWide, int tilesHigh, int *tiles);
void drawParticles(MintParticleSystem *system);
void fillSpritePixels(MintSprite *spr, int x, int y, int width, int height, int argb);
void clearSprite(MintSprite *spr);
void deinitSprite(MintSprite *spr);
void destroyTexture(Asset *asset);
