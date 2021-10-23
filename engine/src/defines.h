#define Gigabytes(x) (Megabytes(x)*1024)
#define Megabytes(x) (Kilobytes(x)*1024)
#define Kilobytes(x) ((x)*1024)

#define MEMORY_LIMIT Megabytes(100)
#define ASSET_LIMIT 2048
#define ASSET_BULK_LOAD_LIMIT 128
#define PATH_LIMIT 256
#define URL_LIMIT 2083
#define SHADER_LOG_LIMIT 512
#define SPRITE_LIMIT 512
#define CHILDREN_LIMIT 512
#define TEXTURE_LIMIT 512
#define PARTICLE_SYSTEM_LIMIT 512
#define TEXTURE_WIDTH_LIMIT 4095
#define TEXTURE_HEIGHT_LIMIT 4095

#define SHORT_STR 64
#define MED_STR 256
#define LARGE_STR 2048
#define HUGE_STR 10240

#define FRAME_LIMIT 8192
#define ANIMATION_FRAME_LIMIT 1024
#define ANIMATION_LIMIT 32

#define KEY_LIMIT 512

#define TILED_LAYERS_LIMIT 64
#define TILES_WIDE_LIMIT 128
#define TILES_HIGH_LIMIT 128
#define TILEMAP_SPRITE_LAYER_LIMIT 16
#define TILEMAP_META_LIMIT 128
#define TILEMAP_PROPERTY_LIMIT 32

#define CHANNEL_LIMIT 512
#define SOUND_LIMIT 512
#define SOUND_MEMORY_LIMIT (Megabytes(50))

#ifdef SEMI_ANDROID
# define SAMPLE_BUFFER_LIMIT (4096*3)
#else
# define SAMPLE_BUFFER_LIMIT (4096*2)
#endif

#define SERIAL_SIZE Megabytes(2)

#define PATHFIND_PATH_LIMIT 64
#define PATHFIND_DISTANCE_LIMIT 32

#define REGION_LIMIT 128
#define CHARS_LIMIT 512
#define KERNS_LIMIT 4096

#define SEMI_STRINGIFYX(val) #val
#define SEMI_STRINGIFY(val) SEMI_STRINGIFYX(val)

#define ForEach(item_f3jfs9, array_sdff3) \
	for(int keep_dsfsdjfi = 1, \
		count_dsffsd = 0,\
		size_dfjsdfkl = sizeof (array_sdff3) / sizeof *(array_sdff3); \
		keep_dsfsdjfi && count_dsffsd != size_dfjsdfkl; \
		keep_dsfsdjfi = !keep_dsfsdjfi, count_dsffsd++) \
		for(item_f3jfs9 = (array_sdff3) + count_dsffsd; keep_dsfsdjfi; keep_dsfsdjfi = !keep_dsfsdjfi)

//@hack I need these defs here
void engineAssert(bool expr, const char *filename, int lineNum);
void *engineMalloc(size_t size, const char *filename, int lineNum);
void engineFree(void *mem, const char *filename, int lineNum);

#ifdef SEMI_INTERNAL
# define Assert(expr) engineAssert(expr, __FILE__, __LINE__)
# if 1
#  define Malloc(expr) engineMalloc(expr, __FILE__, __LINE__)
#  define Free(expr) engineFree(expr, __FILE__, __LINE__)
# else
#  define Malloc(expr) malloc(expr)
#  define Free(expr) free(expr)
# endif
#else
# define Assert(expr) if (!(expr)) printf("Assert error!\n");
# define Malloc(expr) malloc(expr)
# define Free(expr) free(expr)
#endif

#ifdef SEMI_FLASH
#define printf(...) flashPrintf(__VA_ARGS__)

int flashPrintf(const char *fmt, ...) {
	char buffer[HUGE_STR];

	va_list argptr;
	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);

	int bufferLen = strlen(buffer);
	if (buffer[bufferLen-1] == '\n') bufferLen--;

	inline_as3(
		"trace(\"trace: \" + CModule.readString(%0, %1));\n"
		: : "r"(buffer), "r"(bufferLen)
	);
	return 0;
}
#endif

#ifdef SEMI_ANDROID
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "semiphore", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "semiphore", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "semiphore", __VA_ARGS__))
#define printf(...) androidPrintf(__VA_ARGS__)

void androidPrintf(const char *fmt, ...) {
	char buffer[10240];

	va_list argptr;
	va_start(argptr, fmt);
	int bufferLen = vsprintf(buffer, fmt, argptr);
	va_end(argptr);

	if (buffer[bufferLen-1] == '\n') {
		buffer[bufferLen-1] = '\0';
		bufferLen--;
	}

	LOGI("%s\n", buffer);
}
#endif
