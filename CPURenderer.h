#pragma once

#include "Globals.h"

class CPURenderer
{
public:
    CPURenderer(uint32_t InWidth, uint32_t InHeight);
    ~CPURenderer();

    void Clear();
    void Present(HWND WindowHandle) const;

    uint32_t* RenderBuffer;
    uint32_t Width;
    uint32_t Height;

private:
    BITMAPINFO Info;
};

struct Color
{
    uint8_t R;
    uint8_t G;
    uint8_t B;
};

enum StarShape
{
    Square,
    Circle,
    Diamond,
    Twinkle,
};

//Treating Star as a mix of render primitive and world object
class Star
{
public:
    Star() {};
    void Initialize(uint32_t WorldWidth, uint32_t WorldHeight, uint32_t InMaxSize, float InMaxLifetime);

    void Tick(float DeltaTime);
    void Render(CPURenderer& Renderer) const;

    Color GetColor() const;
    
    //Used to determine if Star should tick/render
    float RemainingLifetime = 0.0f;

private:

    uint32_t XPos = 0;
    uint32_t YPos = 0;
    uint32_t Size = 0;
    StarShape Shape = StarShape::Square;
    bool bShouldProgress = false; //Should star expand and twinkle

    float MaxLifetime = 0;
    
    uint8_t ExpandStage = 0;
};