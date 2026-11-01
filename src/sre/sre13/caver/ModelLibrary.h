#ifndef SRE13_MODELLIBRARY_H
#define SRE13_MODELLIBRARY_H

#include "../core/hook.h"
#include "../core/stdstring.h"

typedef struct ModelLibrary {
	char _pad[1];
} ModelLibrary;

DL_SYMBOL_DECL(ModelLibrary_sharedLibrary, ModelLibrary*, (void));
DL_SYMBOL_DECL(ModelLibrary_ModelForName, void*, (ModelLibrary *lib, String *name));
DL_SYMBOL_DECL(ModelLibrary_Clear, void, (ModelLibrary *lib));
DL_SYMBOL_DECL(ModelLibrary_SetSharedLibrary, void, (ModelLibrary *lib));

#endif /* SRE13_MODELLIBRARY_H */
