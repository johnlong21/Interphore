#include "mintParticleSystem.h"

MintParticleSystem *createMintParticleSystem(const char *assetId, int maxParticles) {
	MintParticleSystemData *systemData = &engine->particleSystemData;

	MintParticleSystem *system = NULL;

	int slot;
	for (slot = 0; slot < PARTICLE_SYSTEM_LIMIT; slot++)
		if (!systemData->systems[slot].exists)
			break;

	system = &systemData->systems[slot];
	memset(system, 0, sizeof(MintParticleSystem));

	system->exists = true;
	system->assetId = assetId;
	system->particlesMax = maxParticles;
	system->particles = (MintParticle *)Malloc(system->particlesMax * sizeof(MintParticle));
	memset(system->particles, 0, system->particlesMax * sizeof(MintParticle));

	system->sprite = createMintSprite();
	system->sprite->setupEmpty(engine->width, engine->height);

	Asset *sprFileAsset = getSprFileAsset(system->assetId);
	if (!sprFileAsset) {
		printf("Particle system needs a spr file\n");
		Assert(0);
	}

	parseSprAsset(sprFileAsset, &system->frames, &system->framesNum);
	return system;
}

void MintParticleSystem::emit() {
	MintParticleSystem *system = this;

	MintParticle *curParticle = NULL;
	for (int i = 0; i < system->particlesMax; i++) {
		if (!system->particles[i].exists) {
			curParticle = &system->particles[i];
			break;
		}
	}

	if (!curParticle) {
		printf("Ran out of particles\n");
		return;
	}

	memset(curParticle, 0, sizeof(MintParticle));
	curParticle->exists = true;
	curParticle->pos.setTo(system->x, system->y);
	curParticle->velo.setTo(0, -5);
};

void MintParticleSystem::update() {
	MintParticleSystem *system = this;

	for (int i = 0; i < system->particlesMax; i++) {
		MintParticle *curParticle = &system->particles[i];
		if (!curParticle->exists) continue;

		curParticle->pos.x += curParticle->velo.x;
		curParticle->pos.y += curParticle->velo.y;
	}

	drawParticles(system);
}
