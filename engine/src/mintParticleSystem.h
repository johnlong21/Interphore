#pragma once

struct MintParticle {
	bool exists;
	int frame;
	Point pos;
	Point velo;
	float life;
};

struct MintParticleSystem {
	bool exists;
	const char *assetId;
	float x;
	float y;

	MintParticle *particles;
	int particlesMax;

	MintSprite *sprite;
	Frame *frames;
	int framesNum;

	void emit();
	void update();
};

struct MintParticleSystemData {
	MintParticleSystem systems[PARTICLE_SYSTEM_LIMIT];
};

MintParticleSystem *createMintParticleSystem(const char *assetId, int maxParticles);
