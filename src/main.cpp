#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include "app.h"
#include "anim_player.h"
#include "enemy_ai.h"
#include "file_utils.h"
#include "fps_controller.h"
#include "model_loader.h"
#include "msaa_fbo.h"
#include "physics_setup.h"
#include "projection_utils.h"
#include "scene_renderer.h"
#include "ui_overlay.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
EM_JS(bool, EMSCRIPTEN_IsMobileBrowser, (), {
    return new RegExp("iPhone|iPad|iPod|Android|webOS|BlackBerry|IEMobile|Opera Mini", "i").test(navigator.userAgent);
});
// Check if the browser actually holds pointer lock
EM_JS(bool, EMSCRIPTEN_IsPointerLocked, (), {
    return document.pointerLockElement !== null;
});
#endif

bool isMobilePlatform()
{
#if defined(__ANDROID__) || defined(Q_OS_IOS)
    return true;
#elif defined(__EMSCRIPTEN__)
    return EMSCRIPTEN_IsMobileBrowser();
#else
    return false;
#endif
}

SDL_AppResult SDL_AppInit(void **appState, int argc, char *argv[])
{
    App *app = new App();
    *appState = app;

#ifndef __EMSCRIPTEN__
    if (!SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60"))
        return SDL_APP_FAILURE;
#endif

    if (!SDL_Init(SDL_INIT_VIDEO) || !TTF_Init())
        return SDL_APP_FAILURE;

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    int w = 800, h = 600;
#if defined(__EMSCRIPTEN__)
    w = 0; h = 0;
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#else
    if (isMobilePlatform()) { flags |= SDL_WINDOW_FULLSCREEN; w = 0; h = 0; }
#endif

    app->window = SDL_CreateWindow("Y-Bot Follower FPS (Box3D + Recast + BT.CPP + SDL3)", w, h, flags);
    if (!app->window) return SDL_APP_FAILURE;

    app->glContext = SDL_GL_CreateContext(app->window);
    if (!app->glContext) return SDL_APP_FAILURE;

    SDL_GL_MakeCurrent(app->window, app->glContext);
    SDL_GL_SetSwapInterval(1);

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
        return SDL_APP_FAILURE;
#endif

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Initialize Subsystems
    if (!initSceneShaders(app)) return SDL_APP_FAILURE;
    if (!initTextOverlay(app)) return SDL_APP_FAILURE;

    updateProjectionAndMVP(app);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    app->io = &ImGui::GetIO();
    ImGui::StyleColorsDark();

    app->scale_factor = SDL_GetWindowDisplayScale(app->window);
    if (app->scale_factor <= 0.0f) app->scale_factor = 1.0f;

    const char *font_path = getAssetPath("assets/fonts/LiberationSans-Regular.ttf");
    size_t font_size = 0;
    void *font_data = SDL_LoadFile(font_path, &font_size);
    if (font_data)
        app->io->Fonts->AddFontFromMemoryTTF(font_data, (int)font_size, 22.0f * app->scale_factor);
    else
        app->io->Fonts->AddFontDefault();
    ImGui::GetStyle().ScaleAllSizes(app->scale_factor);

    ImGui_ImplSDL3_InitForOpenGL(app->window, app->glContext);
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    ImGui_ImplOpenGL3_Init("#version 300 es");
#else
    ImGui_ImplOpenGL3_Init("#version 330 core");
#endif

    // Load Assets, Models & Animations
    loadCubeModel(app);
    loadYBotModel(app);
    loadPlayerAnimations(app);
    initPhysicsWorld(app);

    app->showRobot = true;
    app->showPhysicsDebug = true;
    app->isMobile = isMobilePlatform();

    // -------------------------------------------------------------------------
    // Mouse Capture on Startup
    // On Native Desktop: auto-lock if desired.
    // On Web (Emscripten) & Mobile: ALWAYS keep the cursor visible at first run.
    // -------------------------------------------------------------------------
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
    if (!app->isMobile)
    {
        SDL_SetWindowRelativeMouseMode(app->window, true);
        app->mouseCaptured = true;
    }
#else
    app->mouseCaptured = false;
    SDL_SetWindowRelativeMouseMode(app->window, false);
#endif

    initEnemyAI(app, 3);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appState, SDL_Event *event)
{
    App *app = (App *)appState;
    ImGui_ImplSDL3_ProcessEvent(event);

    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;

    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event->type == SDL_EVENT_WINDOW_RESIZED)
    {
        updateProjectionAndMVP(app);
        if (float s = SDL_GetWindowDisplayScale(app->window); s > 0.0f)
            app->scale_factor = s;
    }

    if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST)
    {
        app->mouseCaptured = false;
        SDL_SetWindowRelativeMouseMode(app->window, false);
        SDL_ShowCursor();
    }

    handleFpsEvents(app, event);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appState)
{
    App *app = (App *)appState;

    // -------------------------------------------------------------------------
    // Synchronize Web / OS pointer lock state
    // Handles the browser's asynchronous lock handshake properly, and catches
    // the first ESC press the instant the browser unlocks!
    // -------------------------------------------------------------------------
#if defined(__EMSCRIPTEN__)
    static bool wasActuallyLocked = false;
    bool isLockedNow = EMSCRIPTEN_IsPointerLocked();

    if (isLockedNow)
    {
        wasActuallyLocked = true;
        app->mouseCaptured = true;
    }
    else if (wasActuallyLocked && !isLockedNow)
    {
        // Browser just released lock (user pressed ESC the first time)
        wasActuallyLocked = false;
        app->mouseCaptured = false;
        SDL_SetWindowRelativeMouseMode(app->window, false);
        SDL_ShowCursor();
    }
#else
    if (app->mouseCaptured && !SDL_GetWindowRelativeMouseMode(app->window))
    {
        app->mouseCaptured = false;
        SDL_ShowCursor();
    }
#endif

    int pixelW = 800, pixelH = 600;
    SDL_GetWindowSizeInPixels(app->window, &pixelW, &pixelH);
    if (pixelW <= 0) pixelW = 800;
    if (pixelH <= 0) pixelH = 600;

    int windowW = pixelW, windowH = pixelH;
    SDL_GetWindowSize(app->window, &windowW, &windowH);

    setupMSAAFramebuffer(app, pixelW, pixelH);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    renderUI(app, windowW, windowH);

    static Uint64 last_ticks = SDL_GetTicks();
    Uint64 current_ticks = SDL_GetTicks();
    float delta_time = (float)(current_ticks - last_ticks) / 1000.0f;
    last_ticks = current_ticks;

    // 1. Step FPS Controller, Enemy AI, and Physics
    updateFpsController(app, delta_time);
    updateEnemyAI(app, delta_time);
    b3World_Step(app->worldId, 1.0f / 60.0f, 5);

    // 2. Camera View & Projection using cglm
    b3Vec3 capPos = b3Body_GetPosition(app->capsuleId);
    vec3 cameraPos = { capPos.x, capPos.y + 0.65f, capPos.z };

    float aspect = (float)pixelW / (float)(pixelH == 0 ? 1 : pixelH);
    mat4 projMatrix, viewMatrix;
    glm_perspective(glm_rad(65.0f), aspect, 0.1f, 500.0f, projMatrix);

    vec3 targetPos;
    glm_vec3_add(cameraPos, app->cameraFront, targetPos);
    vec3 upDir = { 0.0f, 1.0f, 0.0f };
    glm_lookat(cameraPos, targetPos, upDir, viewMatrix);

    glm_mat4_mul(projMatrix, viewMatrix, app->projView3D);

    ImGui::Render();

    // PASS 1: Render 3D Scene into MSAA FBO (4x MSAA)
    glBindFramebuffer(GL_FRAMEBUFFER, app->msaaFBO);
    glViewport(0, 0, pixelW, pixelH);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.16f, 0.18f, 0.20f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (app->showPhysicsDebug)
    {
        b3World_Draw(app->worldId, &app->dd, B3_DEFAULT_MASK_BITS);
        renderEnemyAIDebug(app);
    }
    if (app->showRobot)
    {
        // Only render follower enemy robots so player's own model does not block FPS camera
        renderEnemyRobots(app);
    }

    // PASS 2: Blit (Resolve) MSAA FBO to Window Default Framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, app->msaaFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, pixelW, pixelH, 0, 0, pixelW, pixelH, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // PASS 3: Render 2D UI / ImGui on Screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, pixelW, pixelH);
    glDisable(GL_DEPTH_TEST);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(app->window);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appState, SDL_AppResult result)
{
    App *app = (App *)appState;
    if (app)
    {
        cleanupMSAAFramebuffer(app);
        cleanupEnemyAI(app);
        if (b3World_IsValid(app->worldId))
            b3DestroyWorld(app->worldId);
        glDeleteBuffers(1, &app->ybotNormalVbo);
        delete app;
    }
    TTF_Quit();
    SDL_Quit();
}
