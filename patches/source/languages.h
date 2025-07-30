#include <gctypes.h>

// Warning: this function pointer will only be non-NULL on PAL 1.0 and PAL 1.2!
// Returns 0-5, corresponding to English, German, French, Spanish, Italian, and Dutch,
// which can be used to index BNR2 (multi-lingual) banners
extern u16 (*get_pal_banner_language)();
