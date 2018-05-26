namespace Writer {
	void addImage(const char *path, const char *name);
	void addRectImage(const char *name, int width, int height, int colour);
	Image *getImage(const char *name);
	void removeImage(const char *name);
	void removeImage(Image *img);
	void permanentImage(const char *name);
	void alignImage(const char *name, const char *gravity=CENTER);
	float getImageX(const char *name);
	float getImageY(const char *name);
	int getImageHeight(const char *name);
	int getImageWidth(const char *name);
	void scaleImage(const char *name, float scaleX, float scaleY);
	void moveImage(const char *name, float xoff, float yoff);
	void moveImagePx(const char *name, float x, float y);
	void rotateImage(const char *name, float rotation);
	void tintImage(const char *name, int tint);
	bool isImageReleasedOnce(const char *name);

	float getImageX(const char *name) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't get the x of image named %s because it doesn't exist", MSG_ERROR, name);
			return 0;
		}

		return img->sprite->x;
	}

	float getImageY(const char *name) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't get the y of image named %s because it doesn't exist", MSG_ERROR, name);
			return 0;
		}

		return img->sprite->y;
	}

	int getImageWidth(const char *name) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't get the width of image named %s because it doesn't exist", MSG_ERROR, name);
			return 0;
		}

		return img->sprite->getWidth();
	}

	int getImageHeight(const char *name) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't get the height of image named %s because it doesn't exist", MSG_ERROR, name);
			return 0;
		}

		return img->sprite->getHeight();
	}

	void addImage(const char *path, const char *name) {
		int slot;
		for (slot = 0; slot < IMAGES_MAX; slot++)
			if (!writer->images[slot].exists)
				break;

		if (slot >= IMAGES_MAX) {
			msg("Too many images", MSG_ERROR);
			return;
		}

		Image *img = &writer->images[slot];
		img->exists = true;
		img->name = stringClone(name);
		img->sprite = createMintSprite(path);
		// img->sprite->centerPivot = true;
		writer->bg->addChild(img->sprite);
	}

	void addRectImage(const char *name, int width, int height, int colour) {
		int slot;
		for (slot = 0; slot < IMAGES_MAX; slot++)
			if (!writer->images[slot].exists)
				break;

		if (slot >= IMAGES_MAX) {
			msg("Too many images", MSG_ERROR);
			return;
		}

		Image *img = &writer->images[slot];
		img->exists = true;
		img->name = stringClone(name);
		img->sprite = createMintSprite();
		img->sprite->setupRect(width, height, colour);
		// img->sprite->centerPivot = true;
		writer->bg->addChild(img->sprite);
	}

	Image *getImage(const char *name) {
		for (int i = 0; i < IMAGES_MAX; i++)
			if (writer->images[i].exists)
				if (streq(writer->images[i].name, name))
					return &writer->images[i];

		return NULL;
	}

	void alignImage(const char *name, const char *gravity) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't align image named %s because it doesn't exist", MSG_ERROR, name);
			return;
		}

		Dir8 dir = DIR8_CENTER;
		if (streq(gravity, CENTER)) dir = DIR8_CENTER;
		if (streq(gravity, TOP)) dir = DIR8_UP;
		if (streq(gravity, BOTTOM)) dir = DIR8_DOWN;
		if (streq(gravity, LEFT)) dir = DIR8_LEFT;
		if (streq(gravity, RIGHT)) dir = DIR8_RIGHT;

		img->sprite->alignInside(dir);
	}

	void tintImage(const char *name, int tint) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't tint image named %s because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->sprite->tint = tint;
	}

	void rotateImage(const char *name, float rotation) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't rotate image named %s because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->sprite->rotation = rotation;
	}

	void moveImage(const char *name, float x, float y) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't move image named %s because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->sprite->x += img->sprite->getWidth() * x;
		img->sprite->y += img->sprite->getHeight() * y;
	}

	void moveImagePx(const char *name, float x, float y) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't movePx image named %s because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->sprite->x = x;
		img->sprite->y = y;
	}

	void scaleImage(const char *name, float scaleX, float scaleY) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't scale image named %s because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->sprite->scale(scaleX, scaleY);
	}


	void removeImage(const char *name) {
		Image *img = getImage(name);

		if (!img || !img->exists) {
			msg("Can't remove image named %s because it doesn't exist", MSG_ERROR, name);
			return;
		}

		removeImage(img);
	}

	void removeImage(Image *img) {
		if (!img || !img->exists) {
			msg("Can't remove image because it doesn't exist or is NULL", MSG_ERROR);
			return;
		}

		img->exists = false;
		img->sprite->destroy();
		Free(img->name);
	}

	void permanentImage(const char *name) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't make image named %s permanent because it doesn't exist", MSG_ERROR, name);
			return;
		}

		img->permanent = true;
	}

	bool isImageReleasedOnce(const char *name) {
		Image *img = getImage(name);

		if (!img) {
			msg("Can't test image named %s because it doesn't exist", MSG_ERROR, name);
			return 0;
		}

		return img->sprite->justReleased;
	}
}
