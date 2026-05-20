#include "jeode_native.h"

#include "MinHook.h"
#include <nlohmann/json.hpp>
#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <string>

// literally just two pointers, no need to get the whole header
typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef unsigned int EGLBoolean;

typedef EGLBoolean(__stdcall *swap_fn_t)(EGLDisplay, EGLSurface);
typedef void(WINAPI *sleep_fn_t)(DWORD);

static const struct JeodeNativeAPI *napi = nullptr;
static std::atomic<int> fps_cap{240};
static std::atomic<DWORD> render_tid{0};

static swap_fn_t real_swap = nullptr;
static sleep_fn_t real_sleep = nullptr;
static void *swap_addr = nullptr;
static void *sleep_addr = nullptr;

static void plog(const char *fmt, ...) {
	if (!napi) return;
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	napi->log("fps-unlocker", buf);
}

static EGLBoolean __stdcall swap_hook(EGLDisplay d, EGLSurface s) {
	if (render_tid.load() == 0) {
		DWORD t = GetCurrentThreadId();
		render_tid.store(t);
		plog("render thread captured (tid=%lu)", t);
	}
	return real_swap(d, s);
}

static void WINAPI sleep_hook(DWORD ms) {
	if (ms > 0 && GetCurrentThreadId() == render_tid.load()) {
		int cap = fps_cap.load();
		if (cap > 60) {
			DWORD diff = (DWORD)(1000.0 / 60.0 - 1000.0 / (double)cap);
			ms = ms > diff ? ms - diff : 0;
		}
	}
	real_sleep(ms);
}

static void load_settings(const char *mod_path) {
	std::string p = std::string(mod_path) + "/settings.json";
	std::ifstream f(p);
	if (!f.is_open()) {
		plog("settings.json not found at %s using default", p.c_str());
		return;
	}
	try {
		auto j = nlohmann::json::parse(f);
		if (j.contains("target_fps") && j["target_fps"].is_number_integer()) {
			int v = j["target_fps"].get<int>();
			if (v < 60) v = 60;
			fps_cap.store(v);
			plog("target_fps=%d", v);
		} else {
			plog("settings.json: 'target_fps' missing or not an int???");
		}
	} catch (const std::exception &e) {
		plog("settings.json parse err: %s", e.what());
	}
}

extern "C" JEODE_EXPORT int JEODE_CALL jeode_native_init(const struct JeodeNativeAPI *api) {
	if (api->api_version != JEODE_NATIVE_API_VERSION) return 1;
	napi = api;

	plog("loading");
	load_settings(api->mod_path);

	MH_STATUS s = MH_Initialize();
	if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) {
		plog("MH_Initialize failed: %s", MH_StatusToString(s));
		return 0;
	}

	// hook eglSwapBuffers so we figure out which thread is doing the rendering
	// sleep is only hooked on this thread to not ruin other game timers
	HMODULE egl = GetModuleHandleA("libEGL.dll");
	if (!egl) {
		plog("libEGL.dll not loaded");
		return 0;
	}
	swap_addr = (void *)GetProcAddress(egl, "eglSwapBuffers");
	if (!swap_addr) {
		plog("eglSwapBuffers export not found");
		return 0;
	}
	s = MH_CreateHook(swap_addr, (void *)&swap_hook, (void **)&real_swap);
	if (s != MH_OK) {
		plog("MH_CreateHook(eglSwapBuffers) failed: %s", MH_StatusToString(s));
		return 0;
	}
	s = MH_EnableHook(swap_addr);
	if (s != MH_OK) {
		plog("MH_EnableHook(eglSwapBuffers) failed: %s", MH_StatusToString(s));
		return 0;
	}
	plog("eglSwapBuffers hook installed");

	// MinHook seems to handle double hooks fine even on seperate instances.
	// an extra jmp per frame isnt gonna kill anyone
	HMODULE k32 = GetModuleHandleA("kernel32.dll");
	if (!k32) {
		plog("kernel32.dll not loaded");
		return 0;
	}
	sleep_addr = (void *)GetProcAddress(k32, "Sleep");
	if (!sleep_addr) {
		plog("Sleep export not found");
		return 0;
	}
	s = MH_CreateHook(sleep_addr, (void *)&sleep_hook, (void **)&real_sleep);
	if (s != MH_OK) {
		plog("MH_CreateHook(Sleep) failed: %s", MH_StatusToString(s));
		return 0;
	}
	s = MH_EnableHook(sleep_addr);
	if (s != MH_OK) {
		plog("MH_EnableHook(Sleep) failed: %s", MH_StatusToString(s));
		return 0;
	}
	plog("Sleep hook installed");

	return 0;
}

extern "C" JEODE_EXPORT void JEODE_CALL jeode_native_shutdown(void) {
	if (sleep_addr) MH_DisableHook(sleep_addr);
	if (swap_addr) MH_DisableHook(swap_addr);
	plog("shutdown");
}
