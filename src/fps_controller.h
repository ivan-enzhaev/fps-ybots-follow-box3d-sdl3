#pragma once

#include <SDL3/SDL.h>

struct App;

void handleFpsEvents(App *app, SDL_Event *event);
void updateFpsController(App *app, float deltaTime);
