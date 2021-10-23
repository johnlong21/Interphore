#include "renderer.h"
#include "engine.h"

RendererData *renderer;

void buildShader(const char *vertSrc, const char *fragSrc, ShaderProgram *prog);
GLuint genGlTexture(int width, int height, unsigned char *data);

void setShaderProgram(ShaderProgram *program);
void setArrayBuffer(GLuint buffer);
void setTexture(GLuint texture, int slot=0);
void setFramebufferTexture(GLuint texture);
void setFramebuffer(GLuint framebuffer);
void setViewport(float x, float y, float w, float h);
GLuint genGlArrayBuffer(float x, float y, float width, float height);
void changeArrayBuffer(GLuint glBuffer, float x, float y, float width, float height);
void destroyGlTexture(GLuint tex);

void checkGlError(int lineNum, const char *context);

void renderTextureToTexture(GLuint src, int srcWidth, int srcHeight, GLuint dest, int destWidth, int destHeight, RenderProps *props);
void renderTextureToSprite(GLuint texture, int textureWidth, int textureHeight, MintSprite *spr, RenderProps *props);
void simpleTextureRender(GLuint src, int srcWidth, int srcHeight);

void initRenderer(void *rendererMemory) {
	printf("Gl renderer initing\n");

	renderer = (RendererData *)rendererMemory;

	// GLint n=0; 
	// glGetIntegerv(GL_NUM_EXTENSIONS, &n); 
	// for (GLint i=0; i<n; i++) { 
	// 	const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
	// 	printf("Ext %d: %s\n", i, extension); 
	// } 

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifdef SEMI_ANDROID
	// glEnable(GL_MULTISAMPLE_ARB); //@incomplete Make this work
#else
	glEnable(GL_MULTISAMPLE);
#endif

	checkGlError(__LINE__, "gl init");

#ifdef SEMI_GL_CORE
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
#endif
	checkGlError(__LINE__, "set vertex object");

	/// Init shaders
	buildShader(
		(char *)getAsset("shader/default.vert", SHADER_ASSET)->data,
		(char *)getAsset("shader/default.frag", SHADER_ASSET)->data,
		&renderer->defaultProgram
	);

	buildShader(
		(char *)getAsset("shader/renderTexture.vert", SHADER_ASSET)->data,
		(char *)getAsset("shader/renderTexture.frag", SHADER_ASSET)->data,
		&renderer->renderTextureProgram
	);

	buildShader(
		(char *)getAsset("shader/tilemap.vert", SHADER_ASSET)->data,
		(char *)getAsset("shader/tilemap.frag", SHADER_ASSET)->data,
		&renderer->tilemapProgram
	);

	buildShader(
		(char *)getAsset("shader/particle.vert", SHADER_ASSET)->data,
		(char *)getAsset("shader/particle.frag", SHADER_ASSET)->data,
		&renderer->particleProgram
	);
	checkGlError(__LINE__, "building shaders");

	renderer->a_position = glGetAttribLocation(renderer->defaultProgram.program, "a_position");
	renderer->a_texcoord = glGetAttribLocation(renderer->defaultProgram.program, "a_texcoord");
	renderer->u_matrix = glGetUniformLocation(renderer->defaultProgram.program, "u_matrix");
	renderer->u_uv = glGetUniformLocation(renderer->defaultProgram.program, "u_uv");
	renderer->u_colour = glGetUniformLocation(renderer->defaultProgram.program, "u_colour");
	renderer->u_tint = glGetUniformLocation(renderer->defaultProgram.program, "u_tint");
	renderer->u_alpha = glGetUniformLocation(renderer->defaultProgram.program, "u_alpha");
	renderer->u_texture = glGetUniformLocation(renderer->defaultProgram.program, "u_texture");

	renderer->rt_a_position = glGetAttribLocation(renderer->renderTextureProgram.program, "rt_a_position");
	renderer->rt_a_texcoord = glGetAttribLocation(renderer->renderTextureProgram.program, "rt_a_texcoord");
	renderer->rt_u_matrix = glGetUniformLocation(renderer->renderTextureProgram.program, "rt_u_matrix");
	renderer->rt_u_texture = glGetUniformLocation(renderer->renderTextureProgram.program, "rt_u_texture");
	renderer->rt_u_tint = glGetUniformLocation(renderer->renderTextureProgram.program, "rt_u_tint");
	renderer->rt_u_bleed = glGetUniformLocation(renderer->renderTextureProgram.program, "rt_u_bleed");

	renderer->tm_a_position = glGetAttribLocation(renderer->tilemapProgram.program, "tm_a_position");
	renderer->tm_u_matrix = glGetUniformLocation(renderer->tilemapProgram.program, "tm_u_matrix");
	renderer->tm_u_tilesetTexture = glGetUniformLocation(renderer->tilemapProgram.program, "tm_u_tilesetTexture");
	renderer->tm_u_tilemapTexture = glGetUniformLocation(renderer->tilemapProgram.program, "tm_u_tilemapTexture");
	renderer->tm_u_tilesetSize = glGetUniformLocation(renderer->tilemapProgram.program, "tm_u_tilesetSize");
	renderer->tm_u_tilemapSize = glGetUniformLocation(renderer->tilemapProgram.program, "tm_u_tilemapSize");
	renderer->tm_u_resultSize = glGetUniformLocation(renderer->tilemapProgram.program, "tm_u_resultSize");
	renderer->tm_u_pixelRatio = glGetUniformLocation(renderer->tilemapProgram.program, "tm_u_pixelRatio");

	renderer->p_a_position = glGetAttribLocation(renderer->particleProgram.program, "p_a_position");
	renderer->p_a_texcoord = glGetAttribLocation(renderer->particleProgram.program, "p_a_texcoord");
	renderer->p_u_matrix = glGetUniformLocation(renderer->particleProgram.program, "p_u_matrix");
	renderer->p_u_texture = glGetUniformLocation(renderer->particleProgram.program, "p_u_texture");
	renderer->p_u_offset = glGetUniformLocation(renderer->particleProgram.program, "p_u_offset");

	checkGlError(__LINE__, "bound shader params");

	setShaderProgram(&renderer->defaultProgram);

	/// Init render texture
	glGenFramebuffers(1, &renderer->textureFramebuffer);

	renderer->emptyTexture = genGlTexture(TEXTURE_WIDTH_LIMIT, TEXTURE_HEIGHT_LIMIT, NULL);
	renderer->tempVerts = genGlArrayBuffer(0, 0, 1, 1);
	renderer->emptyTexcoords = genGlArrayBuffer(0, 0, 1, 1);
	// strcpy(emptyTexture->key, "renderTexture");

	renderer->defaultTexcoords = genGlArrayBuffer(0, 0, 1, 1);

	checkGlError(__LINE__, "created emptyTexture");

	/// Init view texture
	renderer->viewTexture = genGlTexture(engine->width, engine->height, NULL);
}

void buildShader(const char *vertSrc, const char *fragSrc, ShaderProgram *prog) {
#ifdef SEMI_GLES
	const char *versionLine = "#version 300 es\n";
#else
	const char *versionLine = "#version 330\n";
#endif

	prog->vert = glCreateShader(GL_VERTEX_SHADER);
	char *realVert = (char *)Malloc((strlen(vertSrc) + strlen(versionLine) + 1) * sizeof(char));
	strcpy(realVert, versionLine);
	strcat(realVert, vertSrc);
	int vertLen = strlen(realVert);
	glShaderSource(prog->vert, 1, (const char **)&realVert, &vertLen);
	glCompileShader(prog->vert);
	Free(realVert);

	char errLog[SHADER_LOG_LIMIT];
	int errLogNum;

	int vertReturn;
	glGetShaderiv(prog->vert, GL_COMPILE_STATUS, &vertReturn);
	if (!vertReturn) {
		glGetShaderInfoLog(prog->vert, SHADER_LOG_LIMIT, &errLogNum, errLog);
		printf("Vertex result is: %d\nError(%d):\n%s\n", vertReturn, errLogNum, errLog);
	}

	prog->frag = glCreateShader(GL_FRAGMENT_SHADER);
	char *realFrag = (char *)Malloc((strlen(fragSrc) + strlen(versionLine) + 1) * sizeof(char));
	strcpy(realFrag, versionLine);
	strcat(realFrag, fragSrc);
	int fragLen = strlen(realFrag);
	glShaderSource(prog->frag, 1, (const char **)&realFrag, &fragLen);
	glCompileShader(prog->frag);
	Free(realFrag);

	int fragReturn;
	glGetShaderiv(prog->frag, GL_COMPILE_STATUS, &fragReturn);
	if (!fragReturn) {
		glGetShaderInfoLog(prog->frag, SHADER_LOG_LIMIT, &errLogNum, errLog);
		printf("Fragment result is: %d\nError:\n%s\n", fragReturn, errLog);
		printf("Shader: \n%s\n", fragSrc);
	}

	prog->program = glCreateProgram();
	glAttachShader(prog->program, prog->vert);
	glAttachShader(prog->program, prog->frag);
	glLinkProgram(prog->program);

	int progReturn;
	glGetProgramiv(prog->program, GL_LINK_STATUS, &progReturn);
	if (!progReturn) {
		printf("There was an error compiling the shader program\n");
		// glGetShaderInfoLog(prog->program, SHADER_LOG_LIMIT, &errLogNum, errLog);
		// printf("Program result is: %d\nError:\n%s\n", progReturn, errLog);
		// printf("Shader: \n%s\n", vertSrc);
	}
}

void rendererStartFrame() {
	checkGlError(__LINE__, "plat starting frame");
}

void renderSprites(MintSprite **sprites, int spritesNum, Matrix *scaleMatrix, Matrix *scrollMatrix) {
#if 0 
	bool do2pass = true;
#else
	bool do2pass = false;
#endif

	checkGlError(__LINE__, "engine starting frame");

	if (do2pass) {
		setFramebuffer(renderer->textureFramebuffer);
		setFramebufferTexture(renderer->viewTexture);
	} else {
		setFramebuffer(0);

		setViewport(0, 0, engine->width, engine->height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		setShaderProgram(&renderer->defaultProgram);
	}

	checkGlError(__LINE__, "frame inited");

	for (int i = 0; i < spritesNum; i++) {
		if (do2pass) {
			setViewport(0, 0, engine->width, engine->height);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			setShaderProgram(&renderer->defaultProgram);
		}
		MintSprite *spr = sprites[i];
		if (!spr->visible || spr->multipliedAlpha == 0) continue;

		glDisable(GL_SCISSOR_TEST);
		if (!spr->clipRect.isZero()) {
			glScissor(spr->clipRect.x, engine->height - spr->clipRect.height - spr->clipRect.y, spr->clipRect.width, spr->clipRect.height);
			glEnable(GL_SCISSOR_TEST);
		}

		if (spr->animated) setTexture(getTextureAsset(spr->assetId)->glTexture);
		else setTexture(spr->texture);

		glUniform1i(renderer->u_texture, 0);

		int r, g, b, a;
		hexToArgb(spr->tint, &a, &r, &g, &b);
		glUniform4f(
			renderer->u_tint,
			r / 255.0,
			g / 255.0,
			b / 255.0,
			a / 255.0
		);

		glUniform1f(renderer->u_alpha, spr->multipliedAlpha);

		Matrix uv;
		matrixIdentity(&uv);

		Matrix realTrans;
		matrixIdentity(&realTrans);
		matrixProject(&realTrans, engine->width, engine->height);
		if (spr->zooms) matrixMultiply(&realTrans, scaleMatrix);
		if (spr->scrolls) matrixMultiply(&realTrans, scrollMatrix);
		matrixMultiply(&realTrans, &spr->transform);
		matrixMultiply(&realTrans, &spr->offsetTransform);

		if (spr->curFrameRect.width != 0) {
			matrixTranslate(&realTrans, spr->curOffsetPoint.x, spr->curOffsetPoint.y);
			matrixTranslate(&uv, spr->curFrameRect.x/spr->textureWidth, spr->curFrameRect.y/spr->textureHeight);
			matrixScale(&uv, spr->curFrameRect.width/spr->textureWidth, spr->curFrameRect.height/spr->textureHeight);

			changeArrayBuffer(renderer->tempVerts, 0, 0, spr->curFrameRect.width, spr->curFrameRect.height);
		} else {
			changeArrayBuffer(renderer->tempVerts, 0, 0, spr->width, spr->height);
		}

		glEnableVertexAttribArray(renderer->a_position);
		setArrayBuffer(renderer->tempVerts);
		glVertexAttribPointer(renderer->a_position, 2, GL_FLOAT, false, 0, NULL);

		glEnableVertexAttribArray(renderer->a_texcoord);
		setArrayBuffer(renderer->defaultTexcoords);
		glVertexAttribPointer(renderer->a_texcoord, 2, GL_FLOAT, false, 0, NULL);

		glUniformMatrix3fv(renderer->u_matrix, 1, false, (float *)realTrans.data);
		glUniformMatrix3fv(renderer->u_uv, 1, false, (float *)uv.data);

		glDrawArrays(GL_TRIANGLES, 0, 2*3);
		glDisable(GL_SCISSOR_TEST);
		checkGlError(__LINE__, "drew sprite");

		if (do2pass) {
			setFramebuffer(0);
			simpleTextureRender(renderer->viewTexture, renderer->currentViewport.width, renderer->currentViewport.height);
		}
	}
}

void simpleTextureRender(GLuint src, int srcWidth, int srcHeight) {
	printf("Did simple render with %d\n", src);
	setShaderProgram(&renderer->defaultProgram);

	setTexture(src);
	glUniform1i(renderer->u_texture, 0);

	glUniform4f(renderer->u_tint, 0, 0, 0, 0);

	glUniform1f(renderer->u_alpha, 1);

	Matrix uv;
	matrixIdentity(&uv);

	Matrix realTrans;
	matrixIdentity(&realTrans);
	matrixProject(&realTrans, engine->width, engine->height);

	changeArrayBuffer(renderer->tempVerts, 0, 0, srcWidth, srcHeight);
	glEnableVertexAttribArray(renderer->a_position);
	glVertexAttribPointer(renderer->a_position, 2, GL_FLOAT, false, 0, 0);

	glEnableVertexAttribArray(renderer->a_texcoord);
	setArrayBuffer(renderer->defaultTexcoords);
	glVertexAttribPointer(renderer->a_texcoord, 2, GL_FLOAT, false, 0, NULL);

	glUniformMatrix3fv(renderer->u_matrix, 1, false, (float *)realTrans.data);
	glUniformMatrix3fv(renderer->u_uv, 1, false, (float *)uv.data);

	glDrawArrays(GL_TRIANGLES, 0, 2*3);
	checkGlError(__LINE__, "did simple render");
}

void checkGlError(int lineNum, const char *context) {
	{
		GLenum err;
		while ((err = glGetError()) != GL_NO_ERROR) {
			if (renderer->errorCount < 10) {
				printf("gl error: %x(%d) at line %d (%s)\n", err, err, lineNum, context);
				renderer->errorCount++;

				if (renderer->errorCount == 10) {
					printf("Max gl errors exceeded, no more will be shown\n");
					break;
				}
			}
		}
	}

#ifdef SEMI_EGL
	{
		EGLenum err;
		while ((err = eglGetError()) != EGL_SUCCESS) {
			if (renderer->errorCount < 10) {
				printf("egl error: %x(%d) at line %d (%s)\n", err, err, lineNum, context);
				renderer->errorCount++;

				if (renderer->errorCount == 10) {
					printf("Max egl errors exceeded, no more will be shown\n");
					break;
				}
			}
		}
	}
#endif
}

GLuint genGlTexture(int width, int height, unsigned char *data) {
	GLuint tex;
	glGenTextures(1, &tex);
	setTexture(tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if (!data) data = (unsigned char *)&renderer->emptyData;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	return tex;
}

void initEmptySprite(MintSprite *spr) {
	int width = spr->width;
	int height = spr->height;
	spr->texture = genGlTexture(width, height, NULL);
	spr->textureWidth = width;
	spr->textureHeight = height;
}

void initAnimatedSprite(MintSprite *spr) {
	int width = spr->width;
	int height = spr->height;
	//@cleanup This should set textureWidth/textureHeight
}

void deinitSprite(MintSprite *spr) {
	destroyGlTexture(spr->texture);
	// if (spr->uniqueTexture) destroyGlTexture(spr->texture);
}

void destroyTexture(Asset *asset) {
	destroyGlTexture(asset->glTexture);
}

void generateTexture(void *rgba, Asset *asset, bool argb) {
	Assert(argb == 0); // Not supported in gl
	asset->glTexture = genGlTexture(asset->width, asset->height, (unsigned char *)rgba);
	Free(rgba); //@todo Figure out if this should actually be here???
}

void renderSpriteToSprite(MintSprite *source, MintSprite *dest, RenderProps *props) {
	renderTextureToSprite(source->texture, source->textureWidth, source->textureHeight, dest, props);
}

void renderTextureToSprite(const char *assetId, MintSprite *spr, RenderProps *props) {
	renderTextureToSprite(getTextureAsset(assetId), spr, props);
}

void renderTextureToSprite(Asset *tex, MintSprite *spr, RenderProps *props) {
	renderTextureToSprite(tex->glTexture, tex->width, tex->height, spr, props);
}

void renderTextureToSprite(GLuint texture, int textureWidth, int textureHeight, MintSprite *spr, RenderProps *props) {
	renderTextureToTexture(texture, textureWidth, textureHeight, spr->texture, spr->width, spr->height, props);
}

void renderTextureToTexture(GLuint src, int srcWidth, int srcHeight, GLuint dest, int destWidth, int destHeight, RenderProps *props) {
	int canvasWidth = destWidth;
	int canvasHeight = destHeight;

	setFramebuffer(renderer->textureFramebuffer);
	setFramebufferTexture(dest);
	// if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Framebuffer is not ready!\n");

	setShaderProgram(&renderer->renderTextureProgram);
	setViewport(0, 0, canvasWidth, canvasHeight);

	changeArrayBuffer(renderer->tempVerts, 0, 0, props->width, props->height);
	glEnableVertexAttribArray(renderer->rt_a_position);
	glVertexAttribPointer(renderer->rt_a_position, 2, GL_FLOAT, false, 0, 0);

	float texX = (float)props->x/srcWidth;
	float texY = (float)props->y/srcHeight;
	float texW = (float)props->width/srcWidth;
	float texH = (float)props->height/srcHeight;
	changeArrayBuffer(renderer->emptyTexcoords, texX, texY, texX + texW, texY + texH);
	glEnableVertexAttribArray(renderer->rt_a_texcoord);
	glVertexAttribPointer(renderer->rt_a_texcoord, 2, GL_FLOAT, false, 0, 0);

	glUniform4f(
		renderer->rt_u_tint,
		((props->tint >> 16) & 0xff) / 255.0,
		((props->tint >> 8 ) & 0xff) / 255.0,
		( props->tint        & 0xff) / 255.0,
		((props->tint >> 24) & 0xff) / 255.0
	);

	glUniform1i(renderer->rt_u_bleed, props->bleed?1:0);

	Matrix trans;
	matrixIdentity(&trans);
	matrixScale(&trans, 1, -1);
	matrixProject(&trans, canvasWidth, canvasHeight);
	matrixTranslate(&trans, props->dx, props->dy);
	matrixScale(&trans, props->scaleX, props->scaleY);
	glUniformMatrix3fv(renderer->rt_u_matrix, 1, false, trans.data);

	setTexture(src);
	glUniform1i(renderer->rt_u_texture, 0);

	glDrawArrays(GL_TRIANGLES, 0, 2*3);

	checkGlError(__LINE__, "end of renderToTexture");
}

void drawTilesOnSprite(MintSprite *spr, const char *assetId, int tileWidth, int tileHeight, int tilesWide, int tilesHigh, int *tiles) {
	unsigned char *texData = (unsigned char *)Malloc(tilesWide * tilesHigh * 4);
	memset(texData, 0, tilesWide * tilesHigh * 4);
	for (int i = 0; i < tilesWide * tilesHigh; i++) {
		texData[i*4 + 0] = ((tiles[i]) & 0xff);
		texData[i*4 + 1] = ((tiles[i] >> 8) & 0xff);
		// texData[i*4 + 2] = ((tiles[i] >> 16) & 0xff);
		// texData[i*4 + 3] = ((tiles[i] >> 24) & 0xff);
		// printf("%d %d\n", texData[i*4 + 0], texData[i*4 + 1]);
	}

	GLuint tempTex = genGlTexture(tilesWide, tilesHigh, texData);
	checkGlError(__LINE__, "generated tile data texture");
	Free(texData);

	Asset *tilesetTextureAsset = getTextureAsset(assetId);

	setFramebuffer(renderer->textureFramebuffer);
	checkGlError(__LINE__, "");
	setFramebufferTexture(spr->texture);
	checkGlError(__LINE__, "");
	// if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Framebuffer is not ready!\n");

	setShaderProgram(&renderer->tilemapProgram);
	checkGlError(__LINE__, "");
	setViewport(0, 0, spr->width, spr->height);

	glEnableVertexAttribArray(renderer->tm_a_position);
	changeArrayBuffer(renderer->tempVerts, 0, 0, spr->width, spr->height);
	setArrayBuffer(renderer->tempVerts);
	glVertexAttribPointer(renderer->tm_a_position, 2, GL_FLOAT, false, 0, 0);
	checkGlError(__LINE__, "");

	Matrix projectMat;
	matrixIdentity(&projectMat);
	checkGlError(__LINE__, "");
	matrixProject(&projectMat, renderer->currentViewport.width, renderer->currentViewport.height);
	// matrixScale(&projectMat, engineWidth/engineCamera.width,  engineHeight/engineCamera.height);
	// if (sprite->scrolls) matrixTranslate(&projectMat, -engineCamera.x, -engineCamera.y);
	glUniformMatrix3fv(renderer->tm_u_matrix, 1, false, (float *)(&projectMat.data));
	checkGlError(__LINE__, "");

	setTexture(tempTex, 0);
	glUniform1i(renderer->tm_u_tilemapTexture, 0);

	setTexture(tilesetTextureAsset->glTexture, 1);
	glUniform1i(renderer->tm_u_tilesetTexture, 1);

	glUniform2i(renderer->tm_u_tilemapSize, tilesWide, tilesHigh);
	glUniform2i(renderer->tm_u_tilesetSize, tilesetTextureAsset->width, tilesetTextureAsset->height);
	glUniform2i(renderer->tm_u_resultSize, (float)tilesWide*tileWidth, (float)tilesHigh*tileHeight);
	glUniform2i(renderer->tm_u_pixelRatio, tileWidth, tileHeight);

	glDrawArrays(GL_TRIANGLES, 0, 2*3);
	destroyGlTexture(tempTex);
	checkGlError(__LINE__, "end of drawTilesFast");
}

void drawParticles(MintParticleSystem *system) {
	MintSprite *sprite = system->sprite;
	Asset *textureAsset = getTextureAsset(system->assetId);

	setFramebuffer(renderer->textureFramebuffer);
	checkGlError(__LINE__, "");
	setFramebufferTexture(sprite->texture);
	checkGlError(__LINE__, "");
	// if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Framebuffer is not ready!\n");

	setShaderProgram(&renderer->particleProgram);
	checkGlError(__LINE__, "");
	setViewport(0, 0, sprite->width, sprite->height);

	glEnableVertexAttribArray(renderer->p_a_position);
	glEnableVertexAttribArray(renderer->p_a_texcoord);

	Matrix projectMat;
	matrixIdentity(&projectMat);
	matrixProject(&projectMat, renderer->currentViewport.width, renderer->currentViewport.height);
	glUniformMatrix3fv(renderer->p_u_matrix, 1, false, (float *)(&projectMat.data));
	checkGlError(__LINE__, "");

	setTexture(textureAsset->glTexture, 0);
	glUniform1i(renderer->p_u_texture, 0);

	for (int i = 0; i < system->particlesMax; i++) {
		MintParticle *curParticle = &system->particles[i];
		if (!curParticle) continue;

		float texX = (float)0/sprite->width;
		float texY = (float)0/sprite->height;
		float texW = (float)100/sprite->width;
		float texH = (float)100/sprite->height;
		changeArrayBuffer(renderer->emptyTexcoords, 10*i, 10, 20, 20);
		glVertexAttribPointer(renderer->p_a_position, 2, GL_FLOAT, false, 0, 0);
		checkGlError(__LINE__, "");

		// changeArrayBuffer(renderer->emptyTexcoords, 0, 0, 20, 20); // This does weird stuff
		glVertexAttribPointer(renderer->p_a_texcoord, 2, GL_FLOAT, false, 0, NULL);
		checkGlError(__LINE__, "");

		glUniform2f(renderer->p_u_offset, 10, 10);

		glDrawArrays(GL_TRIANGLES, 0, 2*3);
		checkGlError(__LINE__, "end of particle");
		// printf("Drew particle %d\n", i);
	}
	// printf("Drew system\n");
}

void switchSpriteFrame(MintSprite *spr) {
}

void fillSpritePixels(MintSprite *spr, int x, int y, int width, int height, int argb) {
	if (x+width > spr->width) width = spr->width-x;
	if (x+height > spr->height) height = spr->height-x;

	RenderProps props = {};
	props.x = 0;
	props.y = 0;
	props.width = width;
	props.height = height;
	props.dx = x;
	props.dy = y;
	props.tint = argb;
	props.scaleX = 1;
	props.scaleY = 1;
	props.bleed = 1;

	renderTextureToSprite(renderer->emptyTexture, TEXTURE_WIDTH_LIMIT, TEXTURE_HEIGHT_LIMIT, spr, &props);
}

void clearSprite(MintSprite *spr) {
	setFramebuffer(renderer->textureFramebuffer);
	setFramebufferTexture(spr->texture);

	setViewport(0, 0, spr->width, spr->height);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0, 0.0, 0.0, 1.0);
}

void setShaderProgram(ShaderProgram *program) {
	if (renderer->currentProgram == program) return;
	renderer->currentProgram = program;
	glUseProgram(program->program);
	checkGlError(__LINE__, "set program");
}

void setArrayBuffer(GLuint buffer) {
	if (renderer->currentArrayBuffer == buffer) return;
	renderer->currentArrayBuffer = buffer;
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	checkGlError(__LINE__, "set array buffer");
}

void setTexture(GLuint texture, int slot) {
	if (renderer->currentTexture == texture) return;
	renderer->currentTexture = texture;
	if (slot == 0) glActiveTexture(GL_TEXTURE0);
	if (slot == 1) glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, texture);
	checkGlError(__LINE__, "bound texture");
}

void setFramebuffer(GLuint framebuffer) {
	if (renderer->currentFramebuffer == framebuffer) return;
	renderer->currentFramebuffer = framebuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	checkGlError(__LINE__, "set framebuffer");
}

void setFramebufferTexture(GLuint texture) {
	// if (renderer->currentFramebufferTexture == texture) return;
	// renderer->currentFramebufferTexture = texture;
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	checkGlError(__LINE__, "set framebuffer texture");
}

void setViewport(float x, float y, float w, float h) {
	if (renderer->currentViewport.x == x && renderer->currentViewport.y == y && renderer->currentViewport.width == w && renderer->currentViewport.height == h) return;
	renderer->currentViewport.setTo(x, y, w, h);
	glViewport(x, y, w, h);
	checkGlError(__LINE__, "set viewport");
}

void changeArrayBuffer(GLuint glBuffer, float x, float y, float width, float height) {
	setArrayBuffer(glBuffer);

	float bufferData[12] = {
		x, y,
		width, y,
		width, height,
		x, y,
		x, height,
		width, height
	};

	glBufferData(GL_ARRAY_BUFFER, sizeof(bufferData), bufferData, GL_DYNAMIC_DRAW);
}

void destroyGlTexture(GLuint tex) {
	renderer->currentTexture = -1;
	renderer->currentFramebufferTexture = -1;
	glDeleteTextures(1, &tex);
}

GLuint genGlArrayBuffer(float x, float y, float width, float height) {
	GLuint bufferId;
	glGenBuffers(1, &bufferId);
	changeArrayBuffer(bufferId, x, y, width, height);
	return bufferId;
}
