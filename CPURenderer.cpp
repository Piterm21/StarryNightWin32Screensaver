#include "CPURenderer.h"

#define STAR_EXPANSION_FREQUENCY 0.1f

void Star::Initialize(uint32_t WorldWidth, uint32_t WorldHeight, uint32_t InMaxSize, float InMaxLifetime)
{
    XPos = (uint32_t)(RandomFloat() * (WorldWidth - 1));
    YPos = (uint32_t)(RandomFloat() * (WorldHeight - 1));
    
    Size = (uint32_t)(RandomFloat() * (InMaxSize));
    if (Size == 0)
    {
        Size = 1;
    }
    
    bShouldProgress = RandomFloat() <= STAR_EXPANSION_FREQUENCY;
    Shape = StarShape::Square;

    MaxLifetime = RandomFloat() * InMaxLifetime;
    //Clamp to 0.25 of InMaxLifetime at the minimum
    if (MaxLifetime < InMaxLifetime * 0.25f)
    {
        MaxLifetime = InMaxLifetime * 0.25f;
    }

    RemainingLifetime = MaxLifetime;
}

void Star::Tick(float DeltaTime)
{
    RemainingLifetime -= DeltaTime;

    if (!bShouldProgress)
    {
        return;
    }

    float RemiainingLifetimePercent = RemainingLifetime / MaxLifetime;
    //There probably is a better way to handle expansion thresholds, but can't think of it right now
    if ((ExpandStage == 0 && RemiainingLifetimePercent <= 0.4f) 
        || (ExpandStage == 1 && RemiainingLifetimePercent <= 0.3f) 
        || (ExpandStage == 2 && RemiainingLifetimePercent <= 0.2f) 
        || (ExpandStage == 3 && RemiainingLifetimePercent <= 0.1f) 
        || (ExpandStage == 4 && RemiainingLifetimePercent <= 0.05f)
    )
    {
        ExpandStage++;
        switch (ExpandStage)
        {
            case 1:
            case 2:
            {
                //In case Size was 1 we need to increment the size instead of multiplying
                if (Size == 1)
                {
                    Size++;
                }
                else
                {
                    Size = (uint32_t)(Size * 1.5f);
                }

                Shape = StarShape::Circle;
            } break;

            case 3:
            {
                Size = (uint32_t)(Size * 2.0f);
                Shape = StarShape::Diamond;
            } break;

            case 4:
            {
                Shape = StarShape::Square;
            } break;

            case 5:
            {
                Size = (uint32_t)(Size * 1.2f);
                Shape = StarShape::Twinkle;
            } break;
        }
    }
}

Color Star::GetColor() const
{
    float RemiainingLifetimePercent = RemainingLifetime / MaxLifetime;

    if (RemiainingLifetimePercent <= 0.0f)
    {
        return { 0,0,0 };
    }

    if (RemiainingLifetimePercent >= 0.9f)
    {
        return { 155, 155, 155 };
    }

    return { 255,255,255 };
}

void Star::Render(CPURenderer& Renderer) const
{
    if (RemainingLifetime <= 0.0f)
    {
        return;
    }

    Color ColorToSet = GetColor();
    float HalfSize = (float)Size * 0.5f;
    float QuaterSize = HalfSize * 0.5f;
    float EigthSize = QuaterSize * 0.5f;
    float RadiusSquared = HalfSize * HalfSize;

    for (int32_t YIndex = (int32_t)(-1.0f * HalfSize); YIndex < (int32_t)(HalfSize + 0.5f); YIndex++)
    {
        for (int32_t XIndex = (int32_t)(-1.0f * HalfSize); XIndex < (int32_t)(HalfSize + 0.5f); XIndex++)
        {
            bool bShouldRenderPixel = true;
            switch (Shape)
            {
                case StarShape::Circle:
                {
                    bShouldRenderPixel = (YIndex * YIndex + XIndex * XIndex) <= RadiusSquared;
                } break;

                case StarShape::Diamond:
                {
                    float YIndexAbs = (float)((YIndex > 0.0f) ? YIndex : -1 * YIndex);
                    float XIndexAbs = (float)((XIndex > 0.0f) ? XIndex : -1 * XIndex);

                    bShouldRenderPixel = 0.0f < (-XIndexAbs + HalfSize - YIndexAbs);
                } break;

                //Diamond - center + diagonals
                case StarShape::Twinkle:
                {
                    float YIndexAbs = (float)((YIndex > 0.0f) ? YIndex : -1 * YIndex);
                    float XIndexAbs = (float)((XIndex > 0.0f) ? XIndex : -1 * XIndex);
                    bool bIsDiagonal = false;

                    if (EigthSize >= 2.0f)
                    {
                        bIsDiagonal = (0.0f > (XIndexAbs - EigthSize - YIndexAbs)) && (0.0f < (XIndexAbs + EigthSize - YIndexAbs));
                    }
                    else
                    {
                        bIsDiagonal = (YIndexAbs == XIndexAbs);
                    }

                    bool bIsOutsideOfCenter = (0.0f > (-XIndexAbs + QuaterSize - YIndexAbs));

                    bShouldRenderPixel = bIsOutsideOfCenter && ((0.0f < (-XIndexAbs + HalfSize - YIndexAbs)) || bIsDiagonal);
                } break;
            }

            if (bShouldRenderPixel)
            {
                uint32_t YPixelPos = YPos + YIndex;
                uint32_t XPixelPos = XPos + XIndex;

                //Bounds checking
                if (YPixelPos >= 0 && YPixelPos < Renderer.Height && XPixelPos >= 0 && XPixelPos < Renderer.Width)
                {
                    uint32_t& Pixel = *(Renderer.RenderBuffer + YPixelPos * Renderer.Width + XPixelPos);
                    Pixel = ColorToSet.B | ColorToSet.G << 8 | ColorToSet.R << 16;
                }
            }
        }
    }
}

CPURenderer::CPURenderer(uint32_t InWidth, uint32_t InHeight)
{
    Width = InWidth;
    Height = InHeight;

    Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
    Info.bmiHeader.biWidth = Width;
    Info.bmiHeader.biHeight = -(int32_t)Height;
    Info.bmiHeader.biPlanes = 1;
    Info.bmiHeader.biBitCount = 32;
    Info.bmiHeader.biCompression = BI_RGB;

    RenderBuffer = new uint32_t[Width * Height];
    Clear();
}

CPURenderer::~CPURenderer()
{
    delete[] RenderBuffer;
}

void CPURenderer::Clear()
{
    memset(RenderBuffer, 0, Width * Height * sizeof(*RenderBuffer));
}

void CPURenderer::Present(HWND WindowHandle) const
{
    HDC DeviceContext = GetDC(WindowHandle);

    StretchDIBits(DeviceContext,
        0, 0, Width, Height,
        0, 0, Width, Height,
        RenderBuffer,
        &Info,
        DIB_RGB_COLORS, SRCCOPY);

    ReleaseDC(WindowHandle, DeviceContext);
}