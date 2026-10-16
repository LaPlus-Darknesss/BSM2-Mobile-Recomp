#include "window.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
int main(){
  _putenv_s("BSM2_BOOT","1");arc::Window window;std::string error;
  if(!window.Open("BSM2 window check",540,960,&error)){printf("%s\n",error.c_str());return 1;}
  auto* native=static_cast<SDL_Window*>(window.native_window());
  int minw,minh,maxw,maxh;SDL_GetWindowMinimumSize(native,&minw,&minh);SDL_GetWindowMaximumSize(native,&maxw,&maxh);
  if((SDL_GetWindowFlags(native)&SDL_WINDOW_RESIZABLE)||minw!=540||minh!=960||maxw!=540||maxh!=960)return 2;
  printf("Fixed game window passed: resizable flag absent, min/max 540x960\n");window.Close();return 0;
}
