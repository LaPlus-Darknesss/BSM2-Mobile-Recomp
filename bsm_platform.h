#pragma once
#include <cstddef>
#include <cstdint>
struct Arm64Ctx;
uint64_t BsmResolve(const char* name);
bool BsmJniHandle(size_t slot, const char* method, Arm64Ctx* ctx);
uint64_t BsmNative(const char* name);
size_t BsmNativeCount();
void BsmSetDimensions(int width, int height);
void BsmBindWindow(void* window,void* context);
void BsmCaptureNext(const char* path);
void BsmAudioPump();
void BsmMusicPump();
