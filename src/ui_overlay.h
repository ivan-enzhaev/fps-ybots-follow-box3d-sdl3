#pragma once

#include "app.h"
#include <imgui.h>
#include <cmath>

inline void renderUI(App *app, int windowW, int windowH)
{
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)windowW, (float)windowH);

    ImDrawList *draw_list = ImGui::GetForegroundDrawList();

    // =========================================================================
    // 1. First-Person Shooter Crosshair (Center Screen)
    // =========================================================================
    float cx = (float)windowW * 0.5f;
    float cy = (float)windowH * 0.5f;
    float chSize = 7.0f * app->scale_factor;
    float chGap = 4.0f * app->scale_factor;
    ImU32 chColor = IM_COL32(255, 255, 255, 200);

    draw_list->AddLine(ImVec2(cx - chSize - chGap, cy), ImVec2(cx - chGap, cy), chColor, 1.5f);
    draw_list->AddLine(ImVec2(cx + chGap, cy), ImVec2(cx + chSize + chGap, cy), chColor, 1.5f);
    draw_list->AddLine(ImVec2(cx, cy - chSize - chGap), ImVec2(cx, cy - chGap), chColor, 1.5f);
    draw_list->AddLine(ImVec2(cx, cy + chGap), ImVec2(cx, cy + chSize + chGap), chColor, 1.5f);
    draw_list->AddCircleFilled(ImVec2(cx, cy), 1.5f, IM_COL32(255, 50, 50, 220));

    // =========================================================================
    // 2. Settings Window
    // =========================================================================
    ImGui::SetNextWindowPos(ImVec2(20.0f * app->scale_factor, 20.0f * app->scale_factor), ImGuiCond_FirstUseEver);
    ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Checkbox("Show Robots", &app->showRobot);
    ImGui::Checkbox("Show Physics / AI Debug (P)", &app->showPhysicsDebug);
    ImGui::End();

    // =========================================================================
    // 3. Web & Desktop Cursor Unlock Hint Banner
    // =========================================================================
    if (!app->isMobile && !app->mouseCaptured)
    {
        ImGui::SetNextWindowPos(ImVec2((float)windowW * 0.5f - 160.0f * app->scale_factor, 20.0f * app->scale_factor), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGuiWindowFlags hintFlags = ImGuiWindowFlags_NoDecoration |
                                     ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoInputs; // Lets clicks pass through to the game canvas!
        ImGui::Begin("WebHint", nullptr, hintFlags);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Click screen to Look & Aim (ESC to free cursor)");
        ImGui::End();
    }

    // =========================================================================
    // 4. Android Mobile Controls (Virtual Joystick & Jump Button)
    // =========================================================================
    if (app->isMobile)
    {
        // 4a. Left Floating Movement Joystick
        float baseRadius = 60.0f * app->scale_factor;
        float knobRadius = 24.0f * app->scale_factor;

        ImVec2 baseCenter;
        ImVec2 knobPos;

        if (app->moveTouchActive)
        {
            baseCenter = ImVec2(app->moveTouchStartX, app->moveTouchStartY);
            knobPos = ImVec2(app->moveTouchCurX, app->moveTouchCurY);

            // Clamp visual knob to base circle
            float dx = knobPos.x - baseCenter.x;
            float dy = knobPos.y - baseCenter.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > baseRadius)
            {
                knobPos.x = baseCenter.x + (dx / dist) * baseRadius;
                knobPos.y = baseCenter.y + (dy / dist) * baseRadius;
            }
        }
        else
        {
            // Default resting position
            baseCenter = ImVec2(100.0f * app->scale_factor, (float)windowH - 100.0f * app->scale_factor);
            knobPos = baseCenter;
        }

        // Draw joystick base circle & outer ring
        draw_list->AddCircleFilled(baseCenter, baseRadius, IM_COL32(40, 45, 55, 110));
        draw_list->AddCircle(baseCenter, baseRadius, IM_COL32(255, 255, 255, 130), 32, 2.0f);

        // Draw thumb knob
        draw_list->AddCircleFilled(knobPos, knobRadius, IM_COL32(240, 245, 255, app->moveTouchActive ? 220 : 140));
        draw_list->AddCircle(knobPos, knobRadius, IM_COL32(0, 180, 255, 200), 24, 2.0f);

        // 4b. Right Jump Button
        ImVec2 jumpCenter((float)windowW - 90.0f * app->scale_factor, (float)windowH - 90.0f * app->scale_factor);
        float jumpRadius = 45.0f * app->scale_factor;

        draw_list->AddCircleFilled(jumpCenter, jumpRadius, IM_COL32(30, 120, 220, 140));
        draw_list->AddCircle(jumpCenter, jumpRadius, IM_COL32(255, 255, 255, 210), 32, 2.5f);

        const char *jumpText = "JUMP";
        ImVec2 textSize = ImGui::CalcTextSize(jumpText);
        draw_list->AddText(ImVec2(jumpCenter.x - textSize.x * 0.5f, jumpCenter.y - textSize.y * 0.5f),
                           IM_COL32(255, 255, 255, 240), jumpText);

        // 4c. Look Area Helper Hint
        const char *lookHint = "Swipe right area to look";
        ImVec2 lookHintSize = ImGui::CalcTextSize(lookHint);
        draw_list->AddText(ImVec2((float)windowW - lookHintSize.x - 30.0f * app->scale_factor, 30.0f * app->scale_factor),
                           IM_COL32(200, 200, 200, 120), lookHint);
    }
}
