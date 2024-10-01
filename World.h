#pragma once

#include "CPURenderer.h"

#include <stdint.h>

class World
{
public:
	World(uint32_t InWorldWidth, uint32_t InWorldHeight, uint32_t MaxStarCount);
	~World();

	void Tick(float DeltaTime, CPURenderer& RenderBuffer);

	static const uint32_t MinStarCount = 100;
	static const uint32_t DefaultStarCount = 300;
	static const uint32_t MaxStarCount = 500;

private:
	uint32_t WorldWidth;
	uint32_t WorldHeight;

	uint32_t StarsMax = DefaultStarCount;
	float MaxPercentSpawnRate = 0.1f;
	float MaxLifetime = 5.0f;
	uint32_t SizeMax = 5;

	uint32_t ActiveStarsCount = 0;
	Star* ActiveStarsArray = nullptr;
};