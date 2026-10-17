#ifndef STUB_SDL_H
#define STUB_SDL_H
#include <stdint.h>
typedef uint8_t Uint8; typedef uint32_t Uint32; typedef int16_t Sint16;
typedef struct {int x,y,w,h;} SDL_Rect; typedef struct SDL_Window SDL_Window; typedef struct SDL_Renderer SDL_Renderer; typedef struct SDL_Texture SDL_Texture; typedef struct SDL_Joystick SDL_Joystick;
typedef union SDL_Event { uint32_t type; } SDL_Event;
#define SDL_QUIT 1
#define SDL_HAT_UP 1
#define SDL_HAT_RIGHT 2
#define SDL_HAT_DOWN 4
#define SDL_HAT_LEFT 8
#define SDL_INIT_VIDEO 1
#define SDL_INIT_JOYSTICK 2
#define SDL_RENDERER_PRESENTVSYNC 4
#define SDL_PIXELFORMAT_ARGB8888 1
#define SDL_TEXTUREACCESS_STATIC 0
#define SDL_BLENDMODE_BLEND 1
int SDL_Init(uint32_t); void SDL_Quit(void); int SDL_NumJoysticks(void); SDL_Joystick* SDL_JoystickOpen(int); void SDL_JoystickUpdate(void);
int SDL_JoystickNumButtons(SDL_Joystick*); int SDL_JoystickNumAxes(SDL_Joystick*); int SDL_JoystickNumHats(SDL_Joystick*);
Uint8 SDL_JoystickGetButton(SDL_Joystick*,int); Sint16 SDL_JoystickGetAxis(SDL_Joystick*,int); Uint8 SDL_JoystickGetHat(SDL_Joystick*,int);
SDL_Window* SDL_CreateWindow(const char*,int,int,int,int,uint32_t); SDL_Renderer* SDL_CreateRenderer(SDL_Window*,int,uint32_t);
SDL_Texture* SDL_CreateTexture(SDL_Renderer*,uint32_t,int,int,int); int SDL_UpdateTexture(SDL_Texture*,const SDL_Rect*,const void*,int);
int SDL_SetTextureBlendMode(SDL_Texture*,int); int SDL_SetTextureColorMod(SDL_Texture*,Uint8,Uint8,Uint8); int SDL_SetTextureAlphaMod(SDL_Texture*,Uint8);
int SDL_RenderCopy(SDL_Renderer*,SDL_Texture*,const SDL_Rect*,const SDL_Rect*); int SDL_SetRenderDrawBlendMode(SDL_Renderer*,int);
int SDL_PollEvent(SDL_Event*); int SDL_SetRenderDrawColor(SDL_Renderer*,Uint8,Uint8,Uint8,Uint8);
int SDL_RenderFillRect(SDL_Renderer*,const SDL_Rect*); void SDL_RenderPresent(SDL_Renderer*); void SDL_Delay(Uint32); Uint32 SDL_GetTicks(void);
#endif
