#pragma once
struct Arm64Ctx;
void BsmCursorPoll(void* window);
void BsmCursorDiagnostic(float x,float y);
extern "C" void BsmCursorTarget(Arm64Ctx* ctx);
