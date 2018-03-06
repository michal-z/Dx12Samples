#include "Pch.h"
#include "Dx12SamplesLib.h"


double Lib::GetTime()
{
	static LARGE_INTEGER frequency;
	static LARGE_INTEGER startCounter;
	if (frequency.QuadPart == 0)
	{
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&startCounter);
	}
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return (counter.QuadPart - startCounter.QuadPart) / (double)frequency.QuadPart;
}

std::vector<unsigned char> Lib::LoadFile(const char *fileName)
{
	FILE *file = fopen(fileName, "rb");
	assert(file);
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	assert(size != -1);
	std::vector<unsigned char> content(size);
	fseek(file, 0, SEEK_SET);
	fread(&content[0], 1, content.size(), file);
	fclose(file);
	return content;
}

void Lib::UpdateFrameTime(HWND window, const char *windowText, double *time, float *deltaTime)
{
	static double lastTime = -1.0;
	static double lastFpsTime = 0.0;
	static unsigned frameCount = 0;

	if (lastTime < 0.0)
	{
		lastTime = GetTime();
		lastFpsTime = lastTime;
	}

	*time = GetTime();
	*deltaTime = (float)(*time - lastTime);
	lastTime = *time;

	if ((*time - lastFpsTime) >= 1.0)
	{
		double fps = frameCount / (*time - lastFpsTime);
		double ms = (1.0 / fps) * 1000.0;
		char text[256];
		snprintf(text, sizeof(text), "[%.1f fps  %.3f ms] %s", fps, ms, windowText);
		SetWindowText(window, text);
		lastFpsTime = *time;
		frameCount = 0;
	}
	frameCount++;
}
