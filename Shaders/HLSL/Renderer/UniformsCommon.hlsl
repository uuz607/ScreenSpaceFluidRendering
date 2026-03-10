#pragma once

#define THREAD_COUNT_2D_X 16
#define THREAD_COUNT_2D_Y 16

#define THREAD_COUNT_1D_X 256

struct CameraParams
{
    // Matrices
    float4x4 view;
    float4x4 proj;
    float4x4 projView;

    float4x4 invView; 
    float4x4 invProj;
    float4x4 viewTranspose;

    // Position
    float4 position;
};

struct FluidParams
{
    float4x4 model;
    float4 color;

    int2 screenSize;
    float pointSize;
    float thicknessFactor;
};

struct CausticsParams
{
    float4 ior;
    
    float photonEnergy;
    float photonSize;
    float depthBias;
    int minPhotonCount;

    int2 textureSize;
    int padding[2];
};