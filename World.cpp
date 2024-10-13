#include "World.h"

World::World(uint32_t InWorldWidth, uint32_t InWorldHeight, uint32_t MaxStarCount)
{
    WorldWidth = InWorldWidth;
    WorldHeight = InWorldHeight;
    StarsMax = MaxStarCount;

    ActiveStarsArray = new Star[StarsMax];
}

World::~World()
{
    if (ActiveStarsArray)
    {
        delete[] ActiveStarsArray;
    }
}

void World::Tick(float DeltaTime, CPURenderer& RenderBuffer)
{
    //Determine how many star we want to add this frame
    uint32_t StarsToAdd = 0;
    uint32_t StarCountBelowMax = StarsMax - ActiveStarsCount;
    if (StarCountBelowMax > 0)
    {
        StarsToAdd = (uint32_t)(RandomFloat() * StarsMax * MaxPercentSpawnRate);
        if (StarsToAdd == 0)
        {
            StarsToAdd = (uint32_t)((float)StarsMax * (MaxPercentSpawnRate * 0.5f));

            //Ensure we add something even if StarsMax * MaxPercentSpawnRate * 0.5 would result in 0
            if (StarsToAdd == 0)
            {
                StarsToAdd = 1;
            }
        }

        //Clamp
        if (StarsToAdd > StarCountBelowMax)
        {
            StarsToAdd = StarCountBelowMax;
        }
    }

    //Splitting tick and render could potentially result in faster execution and allow for much easier vectorization, but that seems a bit overkill for the context
    //Same for adding of new stars it may be faster to handle it as a separate loop to avoid branching
    //In any case it would require profiling to see at what point it would be faster
    for (uint32_t Index = 0; Index < StarsMax; Index++)
    {
        Star& ActiveStar = ActiveStarsArray[Index];

        //If star is "alive" tick and render
        if (ActiveStar.RemainingLifetime > 0.0f)
        {
            ActiveStar.Tick(DeltaTime);
            ActiveStar.Render(RenderBuffer);

            if (ActiveStar.RemainingLifetime <= 0.0f)
            {
                ActiveStarsCount--;
            }
        }
        //If star is not "alive" and we have more stars we need to add this frame do it here, just reinitialize the star which result in randomizing it's values
        else if (StarsToAdd > 0)
        {
            ActiveStar.Initialize(WorldWidth, WorldHeight, SizeMax, MaxLifetime);
            ActiveStar.Render(RenderBuffer);

            ActiveStarsCount++;
            StarsToAdd--;
        }
    }
}