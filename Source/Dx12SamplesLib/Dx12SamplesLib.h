#pragma once

#define VHR(hr) if (FAILED(hr)) { assert(0); }
#define SAFE_RELEASE(obj) if ((obj)) { (obj)->Release(); (obj) = nullptr; }

namespace Lib
{

double GetTime();
std::vector<unsigned char> LoadFile(const char *fileName);
void UpdateFrameTime(HWND window, const char *windowText, double *time, float *deltaTime);

}
