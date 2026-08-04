#pragma once
// Minimal SDL_ttf.h compatibility shim.
//
// This machine has the SYSTEM-INSTALLED runtime library
// (libSDL2_ttf-2.0.so.0, package version 2.0.18) but not the matching
// development header, and there's no sudo access to install it. Rather than
// depend on fetching an exact upstream header, this shim declares only the
// small set of functions this project actually calls — signatures that have
// been stable, unchanged, and part of the public SDL_ttf ABI since its
// earliest 2.x releases through the installed 2.0.18.

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TTF_Font TTF_Font;

int TTF_Init(void);
void TTF_Quit(void);

TTF_Font* TTF_OpenFont(const char *file, int ptsize);
void TTF_CloseFont(TTF_Font *font);

SDL_Surface* TTF_RenderText_Blended(TTF_Font *font, const char *text, SDL_Color fg);

#ifdef __cplusplus
}
#endif