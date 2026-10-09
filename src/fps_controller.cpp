#include "fps_controller.h"
#include "app.h"
#include <cmath>
#include <algorithm>

void handleFpsEvents(App *app, SDL_Event *event)
{
    // =========================================================================
    // Android / Touchscreen Multi-Touch Controls (Dual-Zone Mobile Shooter)
    // =========================================================================
    if (event->type == SDL_EVENT_FINGER_DOWN)
    {
        int winW, winH;
        SDL_GetWindowSizeInPixels(app->window, &winW, &winH);
        float touchPixelX = event->tfinger.x * (float)winW;
        float touchPixelY = event->tfinger.y * (float)winH;

        // Check Jump Button (Bottom Right Circle)
        float jumpBtnX = (float)winW - 90.0f * app->scale_factor;
        float jumpBtnY = (float)winH - 90.0f * app->scale_factor;
        float jumpRadius = 45.0f * app->scale_factor;
        float dJumpX = touchPixelX - jumpBtnX;
        float dJumpY = touchPixelY - jumpBtnY;

        if ((dJumpX * dJumpX + dJumpY * dJumpY) <= (jumpRadius * jumpRadius))
        {
            app->jumpRequested = true;
            app->jumpFingerId = event->tfinger.fingerID;
            return;
        }

        // Left half of screen -> Movement Joystick
        if (event->tfinger.x < 0.5f)
        {
            if (app->moveFingerId == -1)
            {
                app->moveFingerId = event->tfinger.fingerID;
                app->moveTouchActive = true;
                app->moveTouchStartX = touchPixelX;
                app->moveTouchStartY = touchPixelY;
                app->moveTouchCurX = touchPixelX;
                app->moveTouchCurY = touchPixelY;
                app->moveX_joystick = 0.0f;
                app->moveZ_joystick = 0.0f;
            }
        }
        // Right half of screen -> Touch Look / Aim Drag
        else
        {
            if (app->lookFingerId == -1)
            {
                app->lookFingerId = event->tfinger.fingerID;
                app->lookTouchActive = true;
                app->lookTouchLastX = touchPixelX;
                app->lookTouchLastY = touchPixelY;
            }
        }
    }
    else if (event->type == SDL_EVENT_FINGER_MOTION)
    {
        int winW, winH;
        SDL_GetWindowSizeInPixels(app->window, &winW, &winH);
        float touchPixelX = event->tfinger.x * (float)winW;
        float touchPixelY = event->tfinger.y * (float)winH;

        // Update Movement Joystick
        if (event->tfinger.fingerID == app->moveFingerId && app->moveTouchActive)
        {
            app->moveTouchCurX = touchPixelX;
            app->moveTouchCurY = touchPixelY;

            float dx = app->moveTouchCurX - app->moveTouchStartX;
            float dy = app->moveTouchCurY - app->moveTouchStartY;
            float maxRadius = 65.0f * app->scale_factor;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist > 0.0f)
            {
                float clampedDist = std::min(dist, maxRadius);
                app->moveX_joystick = (dx / dist) * (clampedDist / maxRadius);
                app->moveZ_joystick = (dy / dist) * (clampedDist / maxRadius);
            }
        }

        // Update Look / Aim Rotation
        if (event->tfinger.fingerID == app->lookFingerId && app->lookTouchActive)
        {
            float dx = touchPixelX - app->lookTouchLastX;
            float dy = touchPixelY - app->lookTouchLastY;
            app->lookTouchLastX = touchPixelX;
            app->lookTouchLastY = touchPixelY;

            app->yaw += dx * app->touchSensitivity;
            app->pitch -= dy * app->touchSensitivity;

            if (app->pitch > 89.0f) app->pitch = 89.0f;
            if (app->pitch < -89.0f) app->pitch = -89.0f;
        }
    }
    else if (event->type == SDL_EVENT_FINGER_UP || event->type == SDL_EVENT_FINGER_CANCELED)
    {
        if (event->tfinger.fingerID == app->moveFingerId)
        {
            app->moveFingerId = -1;
            app->moveTouchActive = false;
            app->moveX_joystick = 0.0f;
            app->moveZ_joystick = 0.0f;
        }
        if (event->tfinger.fingerID == app->lookFingerId)
        {
            app->lookFingerId = -1;
            app->lookTouchActive = false;
        }
        if (event->tfinger.fingerID == app->jumpFingerId)
        {
            app->jumpFingerId = -1;
        }
    }

    // =========================================================================
    // Desktop Mouse & Keyboard Controls
    // =========================================================================
    if (event->type == SDL_EVENT_MOUSE_MOTION && app->mouseCaptured)
    {
        app->yaw += event->motion.xrel * app->mouseSensitivity;
        app->pitch -= event->motion.yrel * app->mouseSensitivity;

        if (app->pitch > 89.0f) app->pitch = 89.0f;
        if (app->pitch < -89.0f) app->pitch = -89.0f;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (!app->isMobile && !app->mouseCaptured)
        {
            // Do not lock if user clicked on the ImGui "Settings" window
            if (app->io && app->io->WantCaptureMouse)
            {
                return;
            }
            SDL_SetWindowRelativeMouseMode(app->window, true);
            app->mouseCaptured = true;
        }
    }

    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
    {
        switch (event->key.scancode)
        {
            case SDL_SCANCODE_SPACE:
                app->jumpRequested = true;
                break;
            case SDL_SCANCODE_ESCAPE:
                app->mouseCaptured = false;
                SDL_SetWindowRelativeMouseMode(app->window, false);
                break;
            case SDL_SCANCODE_P:
            case SDL_SCANCODE_F1:
                app->showPhysicsDebug = !app->showPhysicsDebug;
                break;
            default:
                break;
        }
    }
}

void updateFpsController(App *app, float deltaTime)
{
    // 1. Calculate Camera Front Vector
    float yawRad = glm_rad(app->yaw);
    float pitchRad = glm_rad(app->pitch);

    app->cameraFront[0] = std::cos(yawRad) * std::cos(pitchRad);
    app->cameraFront[1] = std::sin(pitchRad);
    app->cameraFront[2] = std::sin(yawRad) * std::cos(pitchRad);
    glm_vec3_normalize(app->cameraFront);

    // 2. Horizontal Forward and Right Directions
    vec3 forwardDir = { app->cameraFront[0], 0.0f, app->cameraFront[2] };
    glm_vec3_normalize(forwardDir);

    vec3 upDir = { 0.0f, 1.0f, 0.0f };
    vec3 rightDir;
    glm_vec3_cross(forwardDir, upDir, rightDir);
    glm_vec3_normalize(rightDir);

    // 3. Accumulate Movement
    vec3 moveDir = { 0.0f, 0.0f, 0.0f };

    // Keyboard (WASD)
    const bool *keys = SDL_GetKeyboardState(nullptr);
    if (keys)
    {
        if (keys[SDL_SCANCODE_W]) glm_vec3_add(moveDir, forwardDir, moveDir);
        if (keys[SDL_SCANCODE_S]) glm_vec3_sub(moveDir, forwardDir, moveDir);
        if (keys[SDL_SCANCODE_D]) glm_vec3_add(moveDir, rightDir, moveDir);
        if (keys[SDL_SCANCODE_A]) glm_vec3_sub(moveDir, rightDir, moveDir);
    }

    // Android Touch Joystick (Z joystick < 0 is forward)
    if (std::abs(app->moveX_joystick) > 0.05f || std::abs(app->moveZ_joystick) > 0.05f)
    {
        vec3 joyForward, joyRight;
        glm_vec3_scale(forwardDir, -app->moveZ_joystick, joyForward);
        glm_vec3_scale(rightDir, app->moveX_joystick, joyRight);
        glm_vec3_add(moveDir, joyForward, moveDir);
        glm_vec3_add(moveDir, joyRight, moveDir);
    }

    // 4. Set Linear Velocity
    float moveSpeed = 5.5f;
    b3Vec3 curVel = b3Body_GetLinearVelocity(app->capsuleId);
    float targetVx = 0.0f;
    float targetVz = 0.0f;

    float len = glm_vec3_norm(moveDir);
    if (len > 0.001f)
    {
        glm_vec3_scale(moveDir, 1.0f / len, moveDir);
        targetVx = moveDir[0] * moveSpeed;
        targetVz = moveDir[2] * moveSpeed;
    }

    // 5. Jump
    float vy = curVel.y;
    if (app->jumpRequested && std::abs(curVel.y) < 0.25f)
    {
        vy = 5.2f;
        app->jumpRequested = false;
    }
    else
    {
        app->jumpRequested = false;
    }

    b3Body_SetLinearVelocity(app->capsuleId, (b3Vec3){ targetVx, vy, targetVz });
}
