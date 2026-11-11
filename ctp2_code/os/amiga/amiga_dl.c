/*
 * The map generator plugins, linked in (include/dlfcn.h). The game asks for
 * "dll\map\<name>.dll" (profile.txt: MapPlugin0..3), turned into ".so"
 * (gs/world/wldgen.cpp, World::LoadMapPlugin); the name before the extension
 * picks the plugin, and dlsym(handle, "CoCreateMapGenerator") its factory,
 * which the makefile compiled under its own name (os/amiga/common.mk).
 */
#include <string.h>

#include <dlfcn.h>

struct IMapGenerator;

struct IMapGenerator *CoCreateMapGenerator_crater(void);
struct IMapGenerator *CoCreateMapGenerator_fault(void);
struct IMapGenerator *CoCreateMapGenerator_geometric(void);
struct IMapGenerator *CoCreateMapGenerator_plasma2(void);

typedef struct IMapGenerator *(*Factory)(void);

typedef struct Plugin
{
	const char * name;
	Factory      create;
} Plugin;

static const Plugin s_plugins[] =
{
	{ "crater",    CoCreateMapGenerator_crater },
	{ "fault",     CoCreateMapGenerator_fault },
	{ "geometric", CoCreateMapGenerator_geometric },
	{ "plasma2",   CoCreateMapGenerator_plasma2 },
};

static const char * s_error = "";

static int same_nocase(const char *a, const char *b, size_t n)
{
	size_t i;
	for (i = 0; i < n; ++i)
	{
		char x = a[i], y = b[i];
		if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
		if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
		if (x != y) return 0;
		if (!x) return 1;
	}
	return 1;
}

void *ctp2_dlopen(const char *file, int mode)
{
	const char * base = file;
	const char * p;
	size_t       len;
	size_t       i;

	(void)mode;
	if (!file) { s_error = "no file name"; return NULL; }

	for (p = file; *p; ++p)
	{
		if (*p == '/' || *p == '\\' || *p == ':') base = p + 1;
	}

	p   = strchr(base, '.');
	len = p ? (size_t)(p - base) : strlen(base);

	for (i = 0; i < sizeof(s_plugins) / sizeof(s_plugins[0]); ++i)
	{
		if (strlen(s_plugins[i].name) == len && same_nocase(base, s_plugins[i].name, len))
		{
			return (void *)&s_plugins[i];
		}
	}

	s_error = "no such map generator in this build";
	return NULL;
}

void *ctp2_dlsym(void *handle, const char *name)
{
	if (!handle || !name || strcmp(name, "CoCreateMapGenerator") != 0)
	{
		s_error = "no such symbol";
		return NULL;
	}
	return (void *)((const Plugin *)handle)->create;
}

int ctp2_dlclose(void *handle)
{
	(void)handle;
	return 0;
}

const char *ctp2_dlerror(void)
{
	return s_error;
}
