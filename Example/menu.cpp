#include <functions.h>
#include <imgui.h>
#include <global.h>
#include <entity.h>
#include "createcharacter.h"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

void menu() {
    ImGui::SetNextWindowSize(ImVec2(700, 700), ImGuiCond_FirstUseEver);
    ImGui::Begin("Mortal Online 2 - Cheat Menu", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("ImGui: %.3f FPS | DMA: %.3f MS", ImGui::GetIO().Framerate, AppSetting::get().last_update_ms.load());
    ImGui::SameLine(ImGui::GetWindowWidth() - 260);
    ImGui::Text("KmBox: %s", kmboxDetected.load() ? "Detected" : "Undetected");

    ImGui::Separator();

    if (ImGui::BeginTabBar("Bar")) {

        // ============================================================
        // MAPS — точки навигации по миру
        // ============================================================
        if (ImGui::BeginTabItem("Maps")) {

            ImGui::Checkbox("Draw Points##nav_draw", &g_NavPoints.enabledDraw);
            ImGui::SameLine();
            ImGui::TextDisabled("(%d/%d)", g_NavPoints.count, NAV_MAX_POINTS);

            ImGui::SeparatorText("Add Point");

            if (ImGui::Button("Add Point (my pos)##nav_add", ImVec2(200, 0)))
                NavPointAdd();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Puts a navigation point at your current world position");

            ImGui::SameLine();

            if (ImGui::Button("Clear All##nav_clear", ImVec2(120, 0)))
                NavPointsClear();

            ImGui::SameLine();
            ImGui::TextDisabled("Toggle per point: On / Label / Dot / m");

            ImGui::Separator();

            // ---- список точек (как список акторов для тепнутся) ----
            ImGui::BeginChild("navpoints_list", ImVec2(0, 0), true);

            if (g_NavPoints.count == 0) {
                ImGui::TextDisabled("(no points yet - click 'Add Point (my pos)')");
            }
            else {
                for (int i = 0; i < g_NavPoints.count; ++i) {
                    auto& p = g_NavPoints.points[i];
                    ImGui::PushID(i);

                    // вкл/выкл самой точки
                    ImGui::Checkbox("##navpt_on", &p.enabled);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show this point");

                    ImGui::SameLine();
                    ImGui::ColorEdit4("##navpt_col", p.color,
                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Point color");

                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(140.f);
                    ImGui::InputText("##navpt_name", p.name, NAV_POINT_NAME_LEN);

                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(70.f);
                    ImGui::InputFloat("##navpt_size", &p.size, 0.5f, 2.f, "%.0f");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Dot size (px)");

                    ImGui::SameLine();
                    ImGui::Checkbox("Lbl##navpt_lbl", &p.showLabel);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show name label");

                    ImGui::SameLine();
                    ImGui::Checkbox("Dot##navpt_dot", &p.showColor);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show colored dot (off = white)");

                    ImGui::SameLine();
                    ImGui::Checkbox("m##navpt_m", &p.showDist);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show distance (meters)");

                    ImGui::SameLine();
                    ImGui::TextDisabled("(%.0f, %.0f, %.0f)", p.pos.x, p.pos.y, p.pos.z);

                    ImGui::SameLine();
                    if (ImGui::Button("X##navpt_del")) {
                        NavPointRemove(i);
                        ImGui::PopID();
                        break;
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // ============================================================
        // COMBAT
        // ============================================================
        if (ImGui::BeginTabItem("Combat")) {

            // ============ AIMBOT ============
            ImGui::Checkbox("Enable Aimbot##aim", &g_Aimbot.enabled);

            {
                char keyBuf[32] = {};
                ImGui::SameLine();
                if (IsWaitingForBind(&g_Aimbot.aimKey)) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, .2f, .2f, 1.f));
                    if (ImGui::Button("PRESS ANY KEY...##aim_bind", ImVec2(180, 0))) CancelBind();
                    ImGui::PopStyleColor();
                    PollBindKey();
                }
                else {
                    char lbl[64];
                    snprintf(lbl, sizeof(lbl), "%s##aim_bind_btn",
                        GetKeyName(g_Aimbot.aimKey, keyBuf, sizeof(keyBuf)));
                    if (ImGui::Button(lbl, ImVec2(180, 0))) {
                        StartBind(&g_Aimbot.aimKey);
                    }
                }
            }

            ImGui::SeparatorText("Target");
            ImGui::SetNextItemWidth(140.f);
            ImGui::InputFloat("FOV (px)##aim_fov", &g_Aimbot.fovPixels, 5.f, 25.f, "%.0f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.f);
            ImGui::InputFloat("Max dist (m)##aim_dist", &g_Aimbot.maxDistM, 5.f, 25.f, "%.0f");

            ImGui::Checkbox("Skip friends##aim_skip", &g_Aimbot.skipFriends);
            ImGui::SameLine();
            ImGui::Checkbox("Draw FOV##aim_drawfov", &g_Aimbot.drawFovCircle);
            ImGui::SameLine();
            ImGui::Checkbox("Show aim point (preview)##aim_preview", &g_Aimbot.showAimPoint);

            ImGui::Checkbox("Lock target while holding##aim_lock", &g_Aimbot.lockTarget);
            ImGui::SameLine();
            ImGui::Checkbox("Block physical mouse##aim_block", &g_Aimbot.blockMouse);

            // ============ AIM HEIGHT ============
            ImGui::SeparatorText("Aim height");
            ImGui::TextDisabled("0%% = feet   50%% = torso   100%% = head");

            int heightPct = (int)std::lround(g_Aimbot.aimHeight * 100.f);
            ImGui::SetNextItemWidth(320.f);
            if (ImGui::SliderInt("##aim_height_slider", &heightPct, 0, 100, "%d%%")) {
                g_Aimbot.aimHeight = heightPct / 100.f;
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("Feet##aim_h_feet"))  g_Aimbot.aimHeight = 0.10f;
            ImGui::SameLine();
            if (ImGui::SmallButton("Legs##aim_h_legs"))  g_Aimbot.aimHeight = 0.30f;
            ImGui::SameLine();
            if (ImGui::SmallButton("Chest##aim_h_chest")) g_Aimbot.aimHeight = 0.60f;
            ImGui::SameLine();
            if (ImGui::SmallButton("Neck##aim_h_neck"))  g_Aimbot.aimHeight = 0.78f;
            ImGui::SameLine();
            if (ImGui::SmallButton("Head##aim_h_head"))  g_Aimbot.aimHeight = 0.92f;

            ImGui::SeparatorText("Trajectory (motor_synergy)");
            ImGui::SetNextItemWidth(160.f);
            ImGui::SliderFloat("Speed scale##aim_tspeed", &g_Aimbot.trajSpeedScale, 0.2f, 3.0f, "%.2f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.f);
            ImGui::SliderFloat("Curve scale##aim_tcurve", &g_Aimbot.curveScale, 0.0f, 3.0f, "%.2f");

            ImGui::SetNextItemWidth(160.f);
            ImGui::SliderFloat("Overshoot prob##aim_tovershoot", &g_Aimbot.overshootProb, 0.0f, 0.6f, "%.2f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.f);
            {
                float tw = (float)g_Aimbot.traj.target_width;
                if (ImGui::SliderFloat("Target width##aim_twidth", &tw, 4.f, 80.f, "%.0f"))
                    g_Aimbot.traj.target_width = (double)tw;
            }

            ImGui::SeparatorText("Calibration");
            ImGui::SetNextItemWidth(140.f);
            ImGui::InputFloat("Mouse scale X##aim_msx", &g_Aimbot.mouseScaleX, 0.05f, 0.25f, "%.3f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.f);
            ImGui::InputFloat("Mouse scale Y##aim_msy", &g_Aimbot.mouseScaleY, 0.05f, 0.25f, "%.3f");

            ImGui::SeparatorText("kmbox Send Mode");
            ImGui::RadioButton("Per-point (human curve)##aim_mode0", &g_Aimbot.mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Auto-move (1 packet)##aim_mode1", &g_Aimbot.mode, 1);

            if (g_Aimbot.mode == 0) {
                ImGui::SetNextItemWidth(160.f);
                ImGui::SliderInt("Max sends/frame##aim_sends", &g_Aimbot.maxSendsPerFrame, 1, 16);
            }
            else {
                ImGui::SetNextItemWidth(160.f);
                ImGui::SliderInt("Auto-move ms##aim_automs", &g_Aimbot.autoMoveMs, 20, 250);
            }

            // ============ TRIGGERBOT ============
            ImGui::SeparatorText("Triggerbot");

            ImGui::Checkbox("Enable Triggerbot##trig", &g_Triggerbot.enabled);

            // --- activation key ---
            {
                char kb[32] = {};
                ImGui::SameLine();
                if (IsWaitingForBind(&g_Triggerbot.activationKey)) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, .2f, .2f, 1.f));
                    if (ImGui::Button("PRESS ANY KEY...##trig_act_bind", ImVec2(180, 0)))
                        CancelBind();
                    ImGui::PopStyleColor();
                    PollBindKey();
                }
                else {
                    char lbl[64];
                    snprintf(lbl, sizeof(lbl), "%s##trig_act_btn",
                        GetKeyName(g_Triggerbot.activationKey, kb, sizeof(kb)));
                    if (ImGui::Button(lbl, ImVec2(180, 0))) {
                        StartBind(&g_Triggerbot.activationKey);
                    }
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(0 = always on)");

            // --- fire key ---
            ImGui::Text("Fire Key:");
            {
                char kb[32] = {};
                ImGui::SameLine();
                if (IsWaitingForBind(&g_Triggerbot.fireKey)) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, .2f, .2f, 1.f));
                    if (ImGui::Button("PRESS ANY KEY...##trig_fire_bind", ImVec2(180, 0)))
                        CancelBind();
                    ImGui::PopStyleColor();
                    PollBindKey();
                }
                else {
                    char lbl[64];
                    snprintf(lbl, sizeof(lbl), "%s##trig_fire_btn",
                        GetKeyName(g_Triggerbot.fireKey, kb, sizeof(kb)));
                    if (ImGui::Button(lbl, ImVec2(180, 0))) {
                        StartBind(&g_Triggerbot.fireKey);
                    }
                }
            }

            ImGui::SeparatorText("Trigger Area");
            ImGui::SetNextItemWidth(140.f);
            ImGui::InputFloat("Width (px)##trig_w", &g_Triggerbot.triggerWidth, 2.f, 10.f, "%.0f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.f);
            ImGui::InputFloat("Height (px)##trig_h", &g_Triggerbot.triggerHeight, 2.f, 10.f, "%.0f");

            ImGui::SetNextItemWidth(140.f);
            ImGui::InputFloat("Max dist (m)##trig_dist", &g_Triggerbot.maxDistM, 5.f, 25.f, "%.0f");
            ImGui::SameLine();
            ImGui::Checkbox("Draw trigger box##trig_draw", &g_Triggerbot.drawTriggerFov);

            ImGui::Checkbox("Skip friends##trig_skip", &g_Triggerbot.skipFriends);

            ImGui::SeparatorText("Timing");
            ImGui::SetNextItemWidth(140.f);
            ImGui::SliderInt("Delay ms##trig_delay", &g_Triggerbot.triggerDelayMs, 0, 300);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.f);
            ImGui::SliderInt("Hold ms##trig_hold", &g_Triggerbot.holdTimeMs, 10, 200);

            ImGui::SetNextItemWidth(140.f);
            ImGui::SliderInt("Cooldown ms##trig_cd", &g_Triggerbot.cooldownMs, 0, 500);
            ImGui::SameLine();
            ImGui::TextDisabled("(delay = reaction time)");

            ImGui::EndTabItem();
        }

        // ============================================================
        // VISUAL
        // ============================================================
        if (ImGui::BeginTabItem("Visual")) {

            ImGui::Checkbox("Enable ESP", &g_Esp.enabled);
            ImGui::SameLine();
            ImGui::Checkbox("Enable Radar", &g_Esp.radarEnabled);

            ImGui::Separator();

            if (ImGui::BeginTabBar("VisualSub")) {

                // ==================== ESP ====================
                if (ImGui::BeginTabItem("ESP")) {

                    ImGui::SeparatorText("Draw distance");
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputFloat("Min dist (m)##esp_mind", &g_Esp.nearDist, 1.f, 5.f, "%.0f");
                    if (g_Esp.nearDist < 0.0f) g_Esp.nearDist = 0.0f;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Do not draw actors closer than this (0 = draw all)");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputFloat("Max dist (m)##esp_maxd", &g_Esp.maxDrawDist, 5.f, 20.f, "%.0f");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Do not draw actors farther than this");
                    if (g_Esp.maxDrawDist < g_Esp.nearDist) g_Esp.nearDist = g_Esp.maxDrawDist;

                    ImGui::SeparatorText("Shapes");
                    ImGui::Checkbox("Box (players)##esp_box", &g_Esp.drawBox);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100.f);
                    ImGui::InputFloat("Box thick##esp_bt", &g_Esp.boxThickness, 0.2f, 1.f, "%.1f");
                    ImGui::Checkbox("Dot (NPC)##esp_dot", &g_Esp.drawDot);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100.f);
                    ImGui::InputFloat("Dot size##esp_ds", &g_Esp.dotSize, 0.2f, 1.f, "%.1f");

                    ImGui::SeparatorText("Health");
                    ImGui::Checkbox("Health Bar##esp_hpb", &g_Esp.drawHealthBar);
                    ImGui::SameLine();
                    ImGui::Checkbox("HP Text##esp_hpt", &g_Esp.drawHealthText);
                    ImGui::SameLine();
                    ImGui::Checkbox("HP only players##esp_hponly", &g_Esp.hpTextOnlyPlayers);

                    ImGui::SeparatorText("Text");

                    // --- чекбоксы: что рисовать ---
                    ImGui::Checkbox("Nick##esp_nick", &g_Esp.drawNick);
                    ImGui::SameLine();
                    ImGui::Checkbox("FName##esp_fn", &g_Esp.drawFName);
                    ImGui::SameLine();
                    ImGui::Checkbox("Attack##esp_atk", &g_Esp.drawAttack);
                    ImGui::SameLine();
                    ImGui::Checkbox("Distance##esp_dist", &g_Esp.drawDistance);
                    ImGui::SameLine();
                    ImGui::Checkbox("Icons##esp_icons", &g_Esp.drawIcons);

                    // --- слайдеры: размеры ---
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::SliderFloat("Text scale##esp_ts", &g_Esp.textScale, 0.5f, 3.0f, "%.2f");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::SliderFloat("Atk scale##esp_as", &g_Esp.atkTextScale, 0.5f, 3.0f, "%.2f");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::SliderFloat("Icon scale##esp_is", &g_Esp.iconScale, 0.5f, 4.0f, "%.2f");
                    ImGui::SeparatorText("Colors");
                    ImGui::ColorEdit4("Friend##esp_cf", g_Esp.friendColor);
                    ImGui::ColorEdit4("Actor (all non-friend)##esp_cn", g_Esp.npcColor);
                    ImGui::ColorEdit4("Text##esp_ct", g_Esp.textColor);
                    ImGui::ColorEdit4("Atk##esp_ca", g_Esp.atkColor);

                    // ============================================
                    // IGNORE CLASSES
                    // ============================================
                    ImGui::SeparatorText("Ignore Classes");
                    ImGui::TextDisabled("Actors whose class contains any of these strings are hidden");
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%d/%d)", g_Esp.ignoreCount, ESP_MAX_IGNORE);

                    // --- обновление списка классов для popup ---
                    static std::vector<std::string> ignoreClassNames;
                    static double lastIgnoreUpdate = 0.0;
                    double nowIgnore = ImGui::GetTime();
                    if (nowIgnore - lastIgnoreUpdate > 0.5) {
                        lastIgnoreUpdate = nowIgnore;
                        ignoreClassNames.clear();
                        auto snap = Entity::Snapshot();
                        if (snap) {
                            for (const auto& a : snap->actors) {
                                if (a.UEClassName.empty()) continue;
                                if (std::find(ignoreClassNames.begin(), ignoreClassNames.end(),
                                    a.UEClassName) == ignoreClassNames.end())
                                    ignoreClassNames.push_back(a.UEClassName);
                            }
                        }
                        std::sort(ignoreClassNames.begin(), ignoreClassNames.end());
                    }

                    // лямбды для проверок
                    auto isInIgnore = [&](const char* cls) -> bool {
                        for (int k = 0; k < g_Esp.ignoreCount; ++k) {
                            if (strncmp(g_Esp.ignoreClasses[k], cls, ESP_CLASS_LEN - 1) == 0)
                                return true;
                        }
                        return false;
                        };

                    auto isInRules = [&](const char* cls) -> bool {
                        for (int k = 0; k < g_Esp.classRuleCount; ++k) {
                            if (strncmp(g_Esp.classRules[k].namePart, cls, ESP_CLASS_LEN - 1) == 0)
                                return true;
                        }
                        return false;
                        };

                    // --- кнопки ---
                    if (ImGui::Button("Add Ignore") && g_Esp.ignoreCount < ESP_MAX_IGNORE)
                        ImGui::OpenPopup("PickIgnore");

                    ImGui::SameLine();

                    // Добавить ВСЕ классы (кроме тех что уже в Rules)
                    if (ImGui::Button("Add All Classes")) {
                        auto snap = Entity::Snapshot();

                        int added = 0, skippedDup = 0, skippedRules = 0;

                        if (snap) {
                            for (const auto& a : snap->actors) {
                                if (a.UEClassName.empty()) continue;
                                if (g_Esp.ignoreCount >= ESP_MAX_IGNORE) break;

                                const char* cls = a.UEClassName.c_str();

                                // уже в Ignore?
                                if (isInIgnore(cls)) { skippedDup++; continue; }

                                // уже в Class Rules? — пропускаем
                                if (isInRules(cls)) { skippedRules++; continue; }

                                int i = g_Esp.ignoreCount++;
                                strncpy_s(g_Esp.ignoreClasses[i], ESP_CLASS_LEN,
                                    cls, ESP_CLASS_LEN - 1);
                                added++;
                            }
                        }

                        // сортировка bubble sort
                        for (int i = 0; i < g_Esp.ignoreCount - 1; ++i) {
                            for (int j = 0; j < g_Esp.ignoreCount - i - 1; ++j) {
                                if (strcmp(g_Esp.ignoreClasses[j],
                                    g_Esp.ignoreClasses[j + 1]) > 0) {
                                    char tmp[ESP_CLASS_LEN];
                                    memcpy(tmp, g_Esp.ignoreClasses[j], ESP_CLASS_LEN);
                                    memcpy(g_Esp.ignoreClasses[j],
                                        g_Esp.ignoreClasses[j + 1], ESP_CLASS_LEN);
                                    memcpy(g_Esp.ignoreClasses[j + 1], tmp, ESP_CLASS_LEN);
                                }
                            }
                        }

                        printf("[ignore] added=%d  skipped_dup=%d  skipped_in_rules=%d  total=%d\n",
                            added, skippedDup, skippedRules, g_Esp.ignoreCount);
                    }

                    ImGui::SameLine();

                    // Очистить всё
                    if (ImGui::Button("Clear All")) {
                        g_Esp.ignoreCount = 0;
                        for (int k = 0; k < ESP_MAX_IGNORE; ++k)
                            memset(g_Esp.ignoreClasses[k], 0, ESP_CLASS_LEN);
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Refresh##ignore"))
                        lastIgnoreUpdate = 0.0;

                    // --- popup выбора класса ---
                    if (ImGui::BeginPopup("PickIgnore")) {
                        ImGui::Text("Pick class to ignore:");
                        ImGui::Separator();

                        static char ignFilter[64] = {};
                        ImGui::SetNextItemWidth(380.f);
                        ImGui::InputTextWithHint("##ignfilter", "filter...", ignFilter, sizeof(ignFilter));
                        ImGui::Separator();

                        if (ignoreClassNames.empty()) {
                            ImGui::TextDisabled("(no classes yet - wait for map load)");
                        }
                        else {
                            ImGui::BeginChild("ignore_list", ImVec2(400, 400), true);
                            for (const auto& cls : ignoreClassNames) {
                                if (ignFilter[0] != '\0' && cls.find(ignFilter) == std::string::npos)
                                    continue;

                                // показываем пометку, если класс уже в Rules
                                bool inRules = isInRules(cls.c_str());
                                bool inIgnore = isInIgnore(cls.c_str());

                                char label[128];
                                if (inRules)
                                    snprintf(label, sizeof(label), "%s  [in Rules]", cls.c_str());
                                else if (inIgnore)
                                    snprintf(label, sizeof(label), "%s  [already ignored]", cls.c_str());
                                else
                                    snprintf(label, sizeof(label), "%s", cls.c_str());

                                if (inRules) ImGui::BeginDisabled();

                                if (ImGui::Selectable(label, false)) {
                                    if (!inIgnore && !inRules &&
                                        g_Esp.ignoreCount < ESP_MAX_IGNORE) {
                                        int i = g_Esp.ignoreCount++;
                                        strncpy_s(g_Esp.ignoreClasses[i], ESP_CLASS_LEN,
                                            cls.c_str(), ESP_CLASS_LEN - 1);
                                    }
                                    ImGui::CloseCurrentPopup();
                                }

                                if (inRules) ImGui::EndDisabled();
                            }
                            ImGui::EndChild();
                        }
                        ImGui::EndPopup();
                    }

                    // --- список текущих правил игнора (скролл-блок) ---
                    ImGui::BeginChild("ignore_current", ImVec2(0, 150), true);
                    for (int i = 0; i < g_Esp.ignoreCount; ++i) {
                        ImGui::PushID(1000 + i);

                        // кнопка "→ Rules" — перевести класс в Class Rules
                        if (ImGui::Button("->R")) {
                            // проверяем, нет ли уже такого правила
                            bool alreadyRule = isInRules(g_Esp.ignoreClasses[i]);

                            if (!alreadyRule && g_Esp.classRuleCount < ESP_MAX_CLASS_RULES) {
                                int ri = g_Esp.classRuleCount++;
                                memset(&g_Esp.classRules[ri], 0, sizeof(EspClassRule));
                                strncpy_s(g_Esp.classRules[ri].namePart, ESP_CLASS_LEN,
                                    g_Esp.ignoreClasses[i], ESP_CLASS_LEN - 1);

                                g_Esp.classRules[ri].enabled = true;
                                g_Esp.classRules[ri].drawBox = true;
                                g_Esp.classRules[ri].drawHealthBar = true;
                                g_Esp.classRules[ri].drawHealthText = true;
                                g_Esp.classRules[ri].drawAttack = true;
                                g_Esp.classRules[ri].drawFName = true;
                                g_Esp.classRules[ri].drawNick = true;
                                g_Esp.classRules[ri].drawDistance = true;
                                g_Esp.classRules[ri].drawIcons = true;

                                g_Esp.classRules[ri].color[0] = 1.0f;
                                g_Esp.classRules[ri].color[1] = 0.0f;
                                g_Esp.classRules[ri].color[2] = 0.0f;
                                g_Esp.classRules[ri].color[3] = 1.0f;

                                printf("[ignore] moved to rules: %s\n", g_Esp.ignoreClasses[i]);
                            }
                            else if (alreadyRule) {
                                printf("[ignore] already in rules: %s\n", g_Esp.ignoreClasses[i]);
                            }

                            // удаляем из ignore в любом случае
                            for (int j = i; j < g_Esp.ignoreCount - 1; ++j)
                                memcpy(g_Esp.ignoreClasses[j],
                                    g_Esp.ignoreClasses[j + 1], ESP_CLASS_LEN);
                            g_Esp.ignoreCount--;
                            memset(g_Esp.ignoreClasses[g_Esp.ignoreCount], 0, ESP_CLASS_LEN);

                            ImGui::PopID();
                            break;
                        }

                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Move to Class Rules");

                        ImGui::SameLine();
                        ImGui::Text("%-40s", g_Esp.ignoreClasses[i]);
                        ImGui::SameLine();

                        if (ImGui::Button("X")) {
                            for (int j = i; j < g_Esp.ignoreCount - 1; ++j)
                                memcpy(g_Esp.ignoreClasses[j],
                                    g_Esp.ignoreClasses[j + 1], ESP_CLASS_LEN);
                            g_Esp.ignoreCount--;
                            memset(g_Esp.ignoreClasses[g_Esp.ignoreCount], 0, ESP_CLASS_LEN);
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndChild();

                    // ============================================
                    // CLASS RULES — выровненная таблица
                    // ============================================
                    ImGui::SeparatorText("Class Rules");
                    ImGui::TextDisabled("Pick class from list -> set color and what to draw");
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%d/%d)", g_Esp.classRuleCount, ESP_MAX_CLASS_RULES);

                    static std::vector<std::string> classNames;
                    static double lastClassUpdate = 0.0;

                    double nowSec = ImGui::GetTime();
                    if (nowSec - lastClassUpdate > 0.5) {
                        lastClassUpdate = nowSec;
                        classNames.clear();
                        auto snap = Entity::Snapshot();
                        if (snap) {
                            for (const auto& a : snap->actors) {
                                if (a.UEClassName.empty()) continue;
                                if (std::find(classNames.begin(), classNames.end(),
                                    a.UEClassName) == classNames.end())
                                    classNames.push_back(a.UEClassName);
                            }
                        }
                        std::sort(classNames.begin(), classNames.end());
                    }

                    if (ImGui::Button("Add Rule") && g_Esp.classRuleCount < ESP_MAX_CLASS_RULES)
                        ImGui::OpenPopup("PickClass");

                    if (ImGui::BeginPopup("PickClass")) {
                        ImGui::Text("Pick class:");
                        ImGui::Separator();

                        static char filterBuf[64] = {};
                        ImGui::SetNextItemWidth(380.f);
                        ImGui::InputTextWithHint("##filter", "filter...", filterBuf, sizeof(filterBuf));
                        ImGui::Separator();

                        if (classNames.empty()) {
                            ImGui::TextDisabled("(no classes yet - wait for map load)");
                        }
                        else {
                            ImGui::BeginChild("class_list", ImVec2(400, 400), true);
                            for (const auto& cls : classNames) {
                                if (filterBuf[0] != '\0' && cls.find(filterBuf) == std::string::npos)
                                    continue;

                                if (ImGui::Selectable(cls.c_str(), false)) {
                                    int i = g_Esp.classRuleCount++;
                                    memset(&g_Esp.classRules[i], 0, sizeof(EspClassRule));
                                    strncpy_s(g_Esp.classRules[i].namePart, ESP_CLASS_LEN,
                                        cls.c_str(), _TRUNCATE);
                                    g_Esp.classRules[i].enabled = true;
                                    g_Esp.classRules[i].drawBox = true;
                                    g_Esp.classRules[i].drawHealthBar = true;
                                    g_Esp.classRules[i].drawHealthText = true;
                                    g_Esp.classRules[i].drawAttack = true;
                                    g_Esp.classRules[i].drawFName = true;
                                    g_Esp.classRules[i].drawNick = true;
                                    g_Esp.classRules[i].drawDistance = true;
                                    g_Esp.classRules[i].drawIcons = true;
                                    g_Esp.classRules[i].color[0] = 1.0f;
                                    g_Esp.classRules[i].color[1] = 0.0f;
                                    g_Esp.classRules[i].color[2] = 0.0f;
                                    g_Esp.classRules[i].color[3] = 1.0f;
                                    for (int j = 0; j < g_Esp.ignoreCount; ) {
                                        if (strcmp(g_Esp.ignoreClasses[j], cls.c_str()) == 0) {
                                            for (int k = j; k < g_Esp.ignoreCount - 1; ++k)
                                                memcpy(g_Esp.ignoreClasses[k],
                                                    g_Esp.ignoreClasses[k + 1], ESP_CLASS_LEN);
                                            g_Esp.ignoreCount--;
                                            memset(g_Esp.ignoreClasses[g_Esp.ignoreCount], 0, ESP_CLASS_LEN);
                                        }
                                        else {
                                            ++j;
                                        }
                                    }
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                            ImGui::EndChild();
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Refresh Classes"))
                        lastClassUpdate = 0.0;

                    // ---------- заголовок таблицы ----------
                    const float COL_ON = 0.0f;
                    const float COL_CLS = 40.0f;
                    const float COL_BOX = 300.0f;
                    const float COL_HPB = 330.0f;
                    const float COL_HPT = 360.0f;
                    const float COL_ATK = 390.0f;
                    const float COL_FN = 420.0f;
                    const float COL_NK = 450.0f;
                    const float COL_DS = 480.0f;
                    const float COL_IC = 510.0f;
                    const float COL_COLOR = 540.0f;
                    const float COL_DEL = 600.0f;

                    ImGui::AlignTextToFramePadding();

                    ImGui::TextUnformatted("On");
                    ImGui::SameLine(COL_CLS);   ImGui::TextUnformatted("Class");
                    ImGui::SameLine(COL_BOX);   ImGui::TextUnformatted("Box");
                    ImGui::SameLine(COL_HPB);   ImGui::TextUnformatted("HPb");
                    ImGui::SameLine(COL_HPT);   ImGui::TextUnformatted("HPt");
                    ImGui::SameLine(COL_ATK);   ImGui::TextUnformatted("Atk");
                    ImGui::SameLine(COL_FN);    ImGui::TextUnformatted("FN");
                    ImGui::SameLine(COL_NK);    ImGui::TextUnformatted("Nk");
                    ImGui::SameLine(COL_DS);    ImGui::TextUnformatted("Ds");
                    ImGui::SameLine(COL_IC);    ImGui::TextUnformatted("Ic");
                    ImGui::SameLine(COL_COLOR); ImGui::TextUnformatted("Color");

                    ImGui::Separator();

                    // ---------- строки ----------
                    for (int i = 0; i < g_Esp.classRuleCount; ++i) {
                        auto& r = g_Esp.classRules[i];
                        ImGui::PushID(i);

                        ImGui::Checkbox("##en", &r.enabled);

                        ImGui::SameLine(COL_CLS);
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("%-30s", r.namePart);

                        ImGui::SameLine(COL_BOX);
                        ImGui::Checkbox("##box", &r.drawBox);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Box / dot");

                        ImGui::SameLine(COL_HPB);
                        ImGui::Checkbox("##hpb", &r.drawHealthBar);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("HP bar");

                        ImGui::SameLine(COL_HPT);
                        ImGui::Checkbox("##hpt", &r.drawHealthText);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("HP text");

                        ImGui::SameLine(COL_ATK);
                        ImGui::Checkbox("##atk", &r.drawAttack);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Attack direction");

                        ImGui::SameLine(COL_FN);
                        ImGui::Checkbox("##fn", &r.drawFName);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("FName");

                        ImGui::SameLine(COL_NK);
                        ImGui::Checkbox("##nk", &r.drawNick);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Nickname");

                        ImGui::SameLine(COL_DS);
                        ImGui::Checkbox("##ds", &r.drawDistance);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Distance");

                        ImGui::SameLine(COL_IC);
                        ImGui::Checkbox("##ic", &r.drawIcons);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Icons");

                        ImGui::SameLine(COL_COLOR);
                        ImGui::ColorEdit4("##col", r.color,
                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                        ImGui::SameLine(COL_DEL);
                        if (ImGui::Button("X##cls_del")) {
                            for (int j = i; j < g_Esp.classRuleCount - 1; ++j)
                                g_Esp.classRules[j] = g_Esp.classRules[j + 1];
                            g_Esp.classRuleCount--;
                            memset(&g_Esp.classRules[g_Esp.classRuleCount], 0, sizeof(EspClassRule));
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndTabItem();
                }

                // ==================== RADAR ====================
                if (ImGui::BeginTabItem("Radar")) {

                    ImGui::SeparatorText("Position / Size");
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputFloat("X##rad_x", &g_Esp.radarPosX, 5.f, 20.f, "%.0f");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputFloat("Y##rad_y", &g_Esp.radarPosY, 5.f, 20.f, "%.0f");
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputFloat("Size##rad_size", &g_Esp.radarSize, 10.f, 50.f, "%.0f");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputFloat("World (units)##rad_world", &g_Esp.radarWorldSize, 1000.f, 5000.f, "%.0f");

                    ImGui::SeparatorText("My Coords");
                    ImGui::Checkbox("Show My Coords##rad_showmy", &g_Esp.showMyCoords);
                    ImGui::ColorEdit4("Coords Color##rad_cc", g_Esp.myCoordsColor);

                    ImGui::SeparatorText("Distances");
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputFloat("Min dist (u)##rad_mind", &g_Esp.radarMinDist, 50.f, 200.f, "%.0f");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputFloat("Max dist (u)##rad_maxd", &g_Esp.radarMaxDist, 1000.f, 5000.f, "%.0f");

                    ImGui::SeparatorText("Display");
                    ImGui::Checkbox("Friends##rad_fr", &g_Esp.radarShowFriends);
                    ImGui::SameLine();
                    ImGui::Checkbox("Enemies##rad_en", &g_Esp.radarShowEnemies);
                    ImGui::SameLine();
                    ImGui::Checkbox("Border##rad_br", &g_Esp.radarShowRing);
                    ImGui::SameLine();
                    ImGui::Checkbox("Axes##rad_ax", &g_Esp.radarShowAxes);
                    ImGui::SameLine();
                    ImGui::Checkbox("Player dot##rad_pd", &g_Esp.radarShowPlayer);

                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputFloat("Dot size##rad_dsz", &g_Esp.radarDotSize, 0.5f, 1.f, "%.1f");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputFloat("Player dot size##rad_pdsz", &g_Esp.radarPlayerDotSize, 0.5f, 1.f, "%.1f");

                    ImGui::SeparatorText("Rings");
                    ImGui::SliderInt("Ring count##rad_rc", &g_Esp.radarRingCount, 0, 3);
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::SliderFloat("Ring 1##rad_r1", &g_Esp.radarRingSizes[0], 0.05f, 1.0f, "%.2f");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::SliderFloat("Ring 2##rad_r2", &g_Esp.radarRingSizes[1], 0.05f, 1.0f, "%.2f");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::SliderFloat("Ring 3##rad_r3", &g_Esp.radarRingSizes[2], 0.05f, 1.0f, "%.2f");

                    ImGui::SeparatorText("Colors");
                    ImGui::ColorEdit4("BG##rad_bg", g_Esp.radarBgColor);
                    ImGui::ColorEdit4("Border##rad_bc", g_Esp.radarBorderColor);
                    ImGui::ColorEdit4("Axis##rad_ac", g_Esp.radarAxisColor);
                    ImGui::ColorEdit4("Player##rad_pc", g_Esp.radarPlayerColor);
                    ImGui::ColorEdit4("Ring##rad_rc2", g_Esp.radarRingColor);

                    ImGui::Checkbox("Use ESP friend/enemy colors##rad_uec", &g_Esp.radarUseEspColors);
                    if (!g_Esp.radarUseEspColors) {
                        ImGui::ColorEdit4("Friend (radar)##rad_frc", g_Esp.radarFriendColor);
                        ImGui::ColorEdit4("Enemy (radar)##rad_enc", g_Esp.radarEnemyColor);
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::EndTabItem();
        }

        // ============================================================
        // MOVEMENT
        // ============================================================
        if (ImGui::BeginTabItem("Movement")) {

            if (ImGui::Button("Update Mount Info", ImVec2(160, 0)))
                MovementUpdate();

            const bool canWrite = g_MountData.valid && g_MountData.pMount != 0;

            ImGui::SeparatorText("Runtime");
            ImGui::Text("Stamina        : %.1f / %.1f", g_MountData.fCurStamina, g_MountData.fMountStaminaMax);
            ImGui::Text("Speed          : %.1f", g_MountData.fMountedSpeed);
            ImGui::Text("Throttle       : %d / %d", g_MountData.iThrottleTgt, g_MountData.iThrottleCap);
            ImGui::Text("MountPose      : %d", g_MountData.MountRiderPose);
            ImGui::Text("AllowedPass    : %d", g_MountData.bLocalPlayerIsAllowedAsPassenger);
            ImGui::Text("TickStamina    : %.3f", g_MountData.fTickStamina);
            ImGui::Text("CapRadius      : %.2f", g_MountData.DefaultCapsuleRadius);
            ImGui::Text("CapHalfHeight  : %.2f", g_MountData.DefaultCapsuleHalfHeight);

            ImGui::Separator();
            ImGui::SeparatorText("Editable");
            ImGui::BeginDisabled(!canWrite);

#define EDIT_FIELD(label, buf, field, offset)                       \
            ImGui::SetNextItemWidth(200.f);                              \
            ImGui::InputFloat(label, &buf.field, 0.f, 0.f, "%.3f");      \
            ImGui::SameLine();                                           \
            if (ImGui::Button("Apply##" label))                          \
                MovementWriteField(offset, buf.field);

            EDIT_FIELD("Stamina Max", g_MountEdit, fMountStaminaMax, offset_fMountStaminaMax)
                EDIT_FIELD("Swim Speed Mult", g_MountEdit, fSwimSpeedMultiplier, offset_fSwimSpeedMultiplier)
                EDIT_FIELD("Rider Weight Thr", g_MountEdit, fRiderWeightPenaltyThreshold, offset_fRiderWeightPenaltyThreshold)
                EDIT_FIELD("Armor Mod Speed", g_MountEdit, fMaxArmorWeight_ModSpeed, offset_fMaxArmorWeight_ModSpeed)
                EDIT_FIELD("Armor Mod Turn", g_MountEdit, fMaxArmorWeight_ModTurnRate, offset_fMaxArmorWeight_ModTurnRate)
                EDIT_FIELD("Throttle Speed", g_MountEdit, fMountThrottleSpeed, offset_fMountThrottleSpeed)
                EDIT_FIELD("Break Speed", g_MountEdit, fMountBreakSpeed, offset_fMountBreakSpeed)

#undef EDIT_FIELD

                ImGui::EndDisabled();
            ImGui::Separator();

            ImGui::SeparatorText("Mount Max Speed");
            int displayCount = g_MountData.vMountMaxSpeed_count;
            if (displayCount <= 0 || displayCount > 16) displayCount = 4;

            for (int i = 0; i < displayCount; ++i) {
                char label[32];   sprintf_s(label, "Speed[%d]##mnt_spd", i);
                char applyId[32]; sprintf_s(applyId, "Apply##spd%d", i);
                char resetId[32]; sprintf_s(resetId, "Reset##spd%d", i);

                ImGui::SetNextItemWidth(150.f);
                ImGui::InputFloat(label, &g_MountEdit.fMountMaxSpeed[i], 0.f, 0.f, "%.1f");

                ImGui::SameLine();
                ImGui::BeginDisabled(!canWrite);
                if (ImGui::Button(applyId))
                    MovementWriteMaxSpeed(i, g_MountEdit.fMountMaxSpeed[i]);
                ImGui::EndDisabled();

                ImGui::SameLine();
                if (ImGui::Button(resetId))
                    g_MountEdit.fMountMaxSpeed[i] = g_MountData.vMountMaxSpeed[i];
            }

            ImGui::Separator();
            ImGui::SeparatorText("Stamina Refill");

            ImGui::Checkbox("##stamEnable", &g_MountStamina.enabled);
            ImGui::SameLine();
            ImGui::Text("Stamina Refill");

            ImGui::SameLine();
            {
                char keyBuf[32] = {};
                if (IsWaitingForBind(&g_MountStamina.key)) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, .2f, .2f, 1.f));
                    if (ImGui::Button("PRESS ANY KEY...##stam", ImVec2(150, 0))) CancelBind();
                    ImGui::PopStyleColor();
                    PollBindKey();
                }
                else if (ImGui::Button(GetKeyName(g_MountStamina.key, keyBuf, sizeof(keyBuf)), ImVec2(150, 0))) {
                    StartBind(&g_MountStamina.key);
                }
            }

            ImGui::SameLine();
            ImGui::Text("Amount:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.f);
            ImGui::InputFloat("##stamAmount", &g_MountStamina.amount, 1.f, 10.f, "%.1f");

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Player")) {

            ImGui::Spacing();
            ImGui::Checkbox("Enable Friend##fr_en", &g_FriendSettings.enabled);
            ImGui::SameLine();

            char keyBuf[32] = {};
            if (IsWaitingForBind(&g_FriendSettings.friendKey))   // ← ФИКС
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, .2f, .2f, 1.f));
                if (ImGui::Button("PRESS ANY KEY...##fr_bind", ImVec2(200, 0))) CancelBind();
                ImGui::PopStyleColor();
                PollBindKey();
            }
            else {
                char lbl[64];
                snprintf(lbl, sizeof(lbl), "%s##fr_btn",
                    GetKeyName(g_FriendSettings.friendKey, keyBuf, sizeof(keyBuf)));
                if (ImGui::Button(lbl, ImVec2(200, 0)))
                    StartBind(&g_FriendSettings.friendKey);
            }

            ImGui::SeparatorText("Friend Pick");

            ImGui::SetNextItemWidth(120.f);
            ImGui::InputFloat("Max angle (deg)", &g_FriendSettings.maxAngle, 0.5f, 2.0f, "%.1f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.f);
            ImGui::InputFloat("Max dist (m)", &g_FriendSettings.maxDistance, 5.f, 20.f, "%.0f");

            ImGui::Checkbox("Only players (BP_PlayerCharacter_C)",
                &g_FriendSettings.playersOnly);

            ImGui::SeparatorText("Timer (Time Dilation)");

            ImGui::Checkbox("Enable Timer##tmr_en", &g_Timer.enabled);
            ImGui::SameLine();

            // --- bind ---
            {
                char keyBuf[32] = {};
                if (IsWaitingForBind(&g_Timer.key))              // ← уже правильный
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, .2f, .2f, 1.f));
                    if (ImGui::Button("PRESS ANY KEY...##tmr_bind", ImVec2(180, 0)))
                        CancelBind();
                    ImGui::PopStyleColor();
                    PollBindKey();
                }
                else {
                    char lbl[64];
                    snprintf(lbl, sizeof(lbl), "%s##tmr_btn",
                        GetKeyName(g_Timer.key, keyBuf, sizeof(keyBuf)));
                    if (ImGui::Button(lbl, ImVec2(180, 0)))
                        StartBind(&g_Timer.key);
                }
            }

            ImGui::SetNextItemWidth(200.f);
            ImGui::SliderFloat("Speed##tmr_speed", &g_Timer.speed, 1.0f, 3.0f, "%.2f x");

            // ============ CAMERA FULLBRIGHT ============
            ImGui::SeparatorText("Camera FullBright");

            ImGui::Checkbox("Enable##fb_en", &g_FullBright.enabled);
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]", FullBrightStatus());

            if (g_FullBright.enabled) {
                ImGui::SetNextItemWidth(240.f);
                ImGui::SliderFloat("Noor##fb_bias", &g_FullBright.camBias, -5.0f, 10.0f, "%.2f");
            }

            ImGui::EndTabItem();
        }

        // ============================================================
        // WORLD
        // ============================================================
        if (ImGui::BeginTabItem("World")) {

            // ============================================================
            // PULL POINT — свободная точка (тот же крутящийся треугольник),
            // ставится НА ПРИЦЕЛЕ (центр экрана), стрелками регулируется
            // дистанция от игрока; Enable Pull притягивает к ней
            // ============================================================
            ImGui::SeparatorText("Pull Point (crosshair + arrows)");

            if (ImGui::Button("Spawn Point##tp_spawn", ImVec2(140, 0)))
                TeleportSpawnPoint();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Spawns the pull triangle on your crosshair (screen center)\nat the distance below\n(arrows: Up/Down = distance from you, Left/Right = strafe)");

            ImGui::SameLine();

            if (g_Teleport.hasFrozen)
                ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "staged (%.0f, %.0f, %.0f) - %.1fm",
                    g_Teleport.frozenTarget.x,
                    g_Teleport.frozenTarget.y,
                    g_Teleport.frozenTarget.z,
                    g_Teleport.pointDist);
            else
                ImGui::TextDisabled("no point staged");

            ImGui::SetNextItemWidth(220.f);
            ImGui::SliderFloat("Arrow speed (m/s)##tp_pstep", &g_Teleport.pointStep, 0.5f, 200.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
            ImGui::SameLine();
            ImGui::TextDisabled("Up/Down = distance (farther/nearer), Left/Right = strafe");

            ImGui::SetNextItemWidth(220.f);
            ImGui::InputFloat("Spawn dist (m)##tp_pdist", &g_Teleport.pointDist, 0.5f, 2.0f, "%.1f");
            if (g_Teleport.pointDist < 1.0f) g_Teleport.pointDist = 1.0f;
            if (g_Teleport.pointDist > 500.0f) g_Teleport.pointDist = 500.0f;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Distance where the point spawns on your crosshair.\nArrows change it too - the next spawn reuses the last distance.");

            if (g_Teleport.enabled && g_Teleport.pointMode)
                ImGui::TextDisabled("pulling... arrows disabled until Enable Pull is off");

            // ============================================================
            // TARGET
            // ============================================================
            ImGui::SeparatorText("Target");

            if (g_Teleport.targetActor == 0)
                ImGui::Text("No target - pick from list below");
            else {
                ImGui::Text("Actor: 0x%p", (void*)g_Teleport.targetActor);
                if (g_Teleport.hasFrozen)
                    ImGui::Text("Frozen: (%.0f, %.0f, %.0f)",
                        g_Teleport.frozenTarget.x,
                        g_Teleport.frozenTarget.y,
                        g_Teleport.frozenTarget.z);
                else
                    ImGui::Text("Frozen: (not captured)");
            }

            ImGui::BeginDisabled(g_Teleport.targetActor == 0);
            if (ImGui::Button("Recapture##tp_recap", ImVec2(120, 0)))
                TeleportCapture();
            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGui::BeginDisabled(!g_Teleport.hasFrozen);
            ImGui::Checkbox("Enable Pull##tp_pull", &g_Teleport.enabled);
            ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("Clear##tp_clear", ImVec2(120, 0)))
                TeleportClear();

            // ============================================================
            // SETTINGS
            // ============================================================
            ImGui::SeparatorText("Settings");

            ImGui::SetNextItemWidth(280.f);
            ImGui::SliderFloat("Speed (m/s)##tp_spd", &g_Teleport.speed,
                1.0f, 1000.0f, "%.0f m/s", ImGuiSliderFlags_Logarithmic);

            if (ImGui::SmallButton("Walk 5"))    g_Teleport.speed = 5.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("Run 10"))    g_Teleport.speed = 10.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("Horse 20"))  g_Teleport.speed = 20.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("Fast 100"))  g_Teleport.speed = 100.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("Blink 500")) g_Teleport.speed = 500.f;

            ImGui::SetNextItemWidth(280.f);
            ImGui::InputFloat("Stop (m)##tp_stop", &g_Teleport.stopDistance,
                0.5f, 2.0f, "%.2f");

            ImGui::SetNextItemWidth(280.f);
            ImGui::InputFloat("Z offset (m)##tp_z", &g_Teleport.zOffset,
                0.1f, 0.5f, "%.2f");

            ImGui::Checkbox("Move Z (3D mode)##tp_mz", &g_Teleport.moveZ);
            ImGui::SameLine();
            if (g_Teleport.moveZ)
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[3D]");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[XY]");

            // ============================================================
            // ACTORS — кешируется, обновляется только по кнопке
            // ============================================================

            struct TeleportActorItem {
                uintptr_t   actor = 0;
                std::string cls;
                Vector3     pos;
                float       dist = 0.f;
            };

            // --- статические кеши (живут между кадрами) ---
            static std::vector<TeleportActorItem> s_items;
            static double s_lastRefreshTime = -1.0;
            static int    s_lastRefreshCount = 0;
            static char   s_tpFilter[64] = {};

            ImGui::Separator();
            ImGui::SeparatorText("Actors");

            // ---- кнопка refresh ----
            bool doRefresh = false;

            if (ImGui::Button("Refresh Actors##tp_refresh", ImVec2(160, 0)))
                doRefresh = true;

            ImGui::SameLine();

            // фильтр по имени класса
            ImGui::SetNextItemWidth(220.f);
            ImGui::InputTextWithHint("##tp_filter", "filter class...",
                s_tpFilter, sizeof(s_tpFilter));

            ImGui::SameLine();

            // инфо о кеше
            if (s_lastRefreshTime < 0.0) {
                ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f),
                    "not refreshed yet");
            }
            else {
                double age = ImGui::GetTime() - s_lastRefreshTime;
                ImGui::TextDisabled("%zu items  (%.0fs ago)",
                    s_items.size(), age);
            }

            // ---- сам refresh ----
            if (doRefresh)
            {
                // === БЕЗ ЛОКОВ: атомарный снапшот, ноль копий ===
                auto snap = Entity::Snapshot();

                std::vector<TeleportActorItem> fresh;
                if (snap) {
                    const auto& hero = snap->hero;
                    const auto& actors = snap->actors;

                    // читаем позицию игрока
                    Vector3 myPos{};
                    if (hero.APawn) {
                        for (const auto& a : actors)
                            if (a.AActor == hero.APawn) { myPos = a.position; break; }
                    }

                    // собираем список
                    fresh.reserve(actors.size());
                    for (const auto& a : actors) {
                        if (!a.AActor || a.AActor == (uintptr_t)-1) continue;
                        if (a.UEClassName.empty()) continue;
                        if (a.position.x == 0.0 &&
                            a.position.y == 0.0 &&
                            a.position.z == 0.0) continue;

                        TeleportActorItem it;
                        it.actor = a.AActor;
                        it.cls = a.UEClassName;
                        it.pos = a.position;

                        double dx = a.position.x - myPos.x;
                        double dy = a.position.y - myPos.y;
                        double dz = a.position.z - myPos.z;
                        it.dist = (float)(sqrt(dx * dx + dy * dy + dz * dz) / 100.0);

                        fresh.push_back(std::move(it));
                    }
                }

                // сортируем по дистанции
                std::sort(fresh.begin(), fresh.end(),
                    [](const TeleportActorItem& a, const TeleportActorItem& b) {
                        return a.dist < b.dist;
                    });

                s_items = std::move(fresh);
                s_lastRefreshTime = ImGui::GetTime();
                s_lastRefreshCount = (int)s_items.size();

                printf("[teleport] refreshed %zu actors\n", s_items.size());
            }

            // ---- список (только из кеша) ----
            ImGui::BeginChild("teleport_actor_list", ImVec2(0, 320), true);

            if (s_items.empty()) {
                ImGui::TextDisabled("(empty - click Refresh Actors)");
            }
            else {
                int shown = 0;
                for (const auto& it : s_items) {
                    if (s_tpFilter[0] != '\0' &&
                        it.cls.find(s_tpFilter) == std::string::npos)
                        continue;

                    bool selected = (it.actor == g_Teleport.targetActor);

                    char label[320];
                    snprintf(label, sizeof(label), "%-40s [%6.1fm]  %p",
                        it.cls.c_str(), it.dist, (void*)it.actor);

                    ImGui::PushID((int)it.actor);   // уникальный ID на актора
                    if (ImGui::Selectable(label, selected)) {
                        g_Teleport.targetActor = it.actor;
                        g_Teleport.hasFrozen = false;
                        g_Teleport.enabled = false;
                        g_Teleport.pointMode = false;   // выбран актор — это не свободная точка
                        TeleportCapture();
                    }
                    ImGui::PopID();
                    ++shown;
                }

                if (shown == 0)
                    ImGui::TextDisabled("(nothing matches filter)");
            }

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        // ============================================================
        // MISC
        // ============================================================
        if (ImGui::BeginTabItem("Misc"))
        {
            auto& cfg = AppSetting::get();

            ImGui::SeparatorText("Screen");
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputInt("Width##misc_w", &cfg.screenWidth, 1, 10);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputInt("Height##misc_h", &cfg.screenHeight, 1, 10);
            if (cfg.screenWidth < 1) cfg.screenWidth = 1;
            if (cfg.screenHeight < 1) cfg.screenHeight = 1;

            ImGui::SeparatorText("KMBox");
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputText("IP##misc_ip", cfg.kmboxIP, IM_COUNTOF(cfg.kmboxIP));
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputText("Port##misc_port", cfg.kmboxPort, IM_COUNTOF(cfg.kmboxPort),
                ImGuiInputTextFlags_CharsDecimal);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputText("UUID##misc_uuid", cfg.kmboxUUID, IM_COUNTOF(cfg.kmboxUUID));

            ImGui::SeparatorText("Client & Gui");
            {
                float limitMs = AppSetting::get().limit_update_ms.load();
                if (ImGui::SliderFloat("DMA##misc_dma", &limitMs, 4.17f, 33.33f, "%.2f ms"))
                    AppSetting::get().limit_update_ms.store(limitMs);
            }

            ImGui::Checkbox("Fuser Mode##misc_fuser", &cfg.fuserMode);

            ImGui::Dummy(ImVec2(0, 20));
            if (ImGui::Button("Save##misc_save", ImVec2(120, 0))) cfg.save();
            ImGui::SameLine();
            if (ImGui::Button("Load##misc_load", ImVec2(120, 0))) cfg.load();
            ImGui::SameLine();
            if (ImGui::Button("Reset##misc_reset", ImVec2(120, 0))) {
                std::remove("config.bin");
                abort();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset ESP##misc_resesp", ImVec2(120, 0)))
                g_Esp = EspSettings();

            // ============================================================
 // CREATE CHARACTER — настройка внешности при создании персонажа.
 // Enable -> покадровая запись. Read -> считать значения. Diagnose -> цепочка.
 // ============================================================
            ImGui::Dummy(ImVec2(0, 24));
            ImGui::SeparatorText("Create Character##misc_cc");

            ImGui::Checkbox("Enable (write every frame)##misc_cc_en", &g_CreateChar.enabled);
            ImGui::SameLine();
            ImGui::TextDisabled("<- ON: applies live each frame");

            static char ccStatus[96] = "";
            if (ImGui::Button("Read##misc_cc_read", ImVec2(120, 0))) {
                if (CreateCharRead()) snprintf(ccStatus, sizeof(ccStatus), "Read OK");
                else snprintf(ccStatus, sizeof(ccStatus), "Read FAILED - open creation screen");
            }
            ImGui::SameLine();
            if (ImGui::Button("Diagnose##misc_cc_diag", ImVec2(120, 0))) {
                CreateCharDiagnose();
                snprintf(ccStatus, sizeof(ccStatus), "Diagnose done");
            }
            ImGui::TextDisabled("%s", ccStatus);
            if (g_CreateCharDiag.controller)
                ImGui::TextDisabled("controller=%llX  bloodlinePanel=%llX",
                    (unsigned long long)g_CreateCharDiag.controller, (unsigned long long)g_CreateCharDiag.panel);

            ImGui::TextDisabled("Set values below, keep Enable ON. Game's Create button - press yourself.");
            ImGui::Spacing();

            // lo/hi — только UI-кламп. Реальные поля в CreateCharSettings и в памяти игры — int32_t.
#define CC_IN(label, field, lo, hi) do {                       \
    ImGui::SetNextItemWidth(180.f);                            \
    ImGui::InputInt(label, &g_CreateChar.field);               \
    if (g_CreateChar.field < lo) g_CreateChar.field = lo;      \
    if (g_CreateChar.field > hi) g_CreateChar.field = hi;      \
} while (0)

            // EMainRace -> читаемое имя (для пояснений в Ancestry)
            auto MainRaceName = [](int v) -> const char* {
                switch (v) {
                    case 0: return "Tindremene"; case 1: return "Sidoian"; case 2: return "Khurite";
                    case 3: return "Sarducaan"; case 4: return "Kallard"; case 5: return "Thursar";
                    case 6: return "Risar"; case 10: return "Sheevra"; case 11: return "Veela";
                    case 20: return "Blainn"; case 21: return "Huergar";
                    case 30: return "Thursar02"; case 35: return "Risar02";
                    case 40: return "Thursar03"; case 45: return "Risar03";
                    default: return "?";
                }
            };

            if (ImGui::BeginTabBar("misc_cc_tabs")) {
                if (ImGui::BeginTabItem("Body##mcc")) {
                    ImGui::TextWrapped("Species = BIOLOGICAL вид персонажа (Human/Alvarin/Oghmir/Thursar/Risar).");
                    ImGui::TextDisabled("НЕ путай: Race (нация) и Ancestry (предки) - в других табах.");
                    ImGui::SetNextItemWidth(200.f); ImGui::InputFloat("Length##cc_len", &g_CreateChar.length, 0.1f, 1.f);
                    ImGui::SetNextItemWidth(200.f); ImGui::InputFloat("Body Size##cc_size", &g_CreateChar.bodySize, 0.1f, 1.f);
                    ImGui::SetNextItemWidth(200.f); ImGui::InputFloat("Body Strength##cc_str", &g_CreateChar.bodyStrength, 0.1f, 1.f);
                    ImGui::Separator();
                    ImGui::TextDisabled("Species / Sex. -1 (Leave) = don't touch.");
                    const char* speciesNames[] = { "Human", "Alvarin", "Oghmir", "Thursar", "Risar" };
                    int speciesSel = g_CreateChar.bodySpecies;
                    if (ImGui::Combo("Species##cc_sp", &speciesSel, speciesNames, IM_ARRAYSIZE(speciesNames)))
                        g_CreateChar.bodySpecies = speciesSel;
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Leave##cc_sp_leave")) g_CreateChar.bodySpecies = -1;
                    const char* sexNames[] = { "None", "Male", "Female" };
                    int sexSel = g_CreateChar.bodySex;
                    if (ImGui::Combo("Sex##cc_sex", &sexSel, sexNames, IM_ARRAYSIZE(sexNames)))
                        g_CreateChar.bodySex = sexSel;
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Leave##cc_sex_leave")) g_CreateChar.bodySex = -1;
                    CC_IN("Age (years, BodyData)##cc_age", bodyAge, -1, 1000);
                    ImGui::TextDisabled("Реальный возраст тела. Возраст как характеристика (attr carousel) - в табе Race/Attrs.");
                    CC_IN("Body color##cc_color", bodyColor, -1, 1000);
                    ImGui::TextDisabled("Основной цвет тела. Цвета волос/бороды/кожи/глаз - в табе Look (DecorationData).");
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Ancestry##mcc")) {
                    ImGui::TextWrapped("Ancestry = 4 ПРЕДКА. Пишутся ФИНАЛЬНЫЕ значения EMainRace (BodyData vAncestry1..4). -1 = leave.");
                    ImGui::TextDisabled("ВАЖНО: это НЕ индекс ComboBox крови! Числа = EMainRace напрямую (см. значения ниже).");
                    ImGui::TextDisabled("EMainRace: 0 Tindremene,1 Sidoian,2 Khurite,3 Sarducaan,4 Kallard,5 Thursar,6 Risar,10 Sheevra,11 Veela,20 Blainn,21 Huergar");
                    CC_IN("Ancestry 1##cc_a1", ancestry1, -1, 1000000);
                    CC_IN("Ancestry 2##cc_a2", ancestry2, -1, 1000000);
                    CC_IN("Ancestry 3##cc_a3", ancestry3, -1, 1000000);
                    CC_IN("Ancestry 4##cc_a4", ancestry4, -1, 1000000);
                    ImGui::SeparatorText("Current vAncestry (from Read)##cc_cur");
                    ImGui::BulletText("Ancestry1 = %d  ->  %s", g_CreateChar.ancestry1, MainRaceName(g_CreateChar.ancestry1));
                    ImGui::BulletText("Ancestry2 = %d  ->  %s", g_CreateChar.ancestry2, MainRaceName(g_CreateChar.ancestry2));
                    ImGui::BulletText("Ancestry3 = %d  ->  %s", g_CreateChar.ancestry3, MainRaceName(g_CreateChar.ancestry3));
                    ImGui::BulletText("Ancestry4 = %d  ->  %s", g_CreateChar.ancestry4, MainRaceName(g_CreateChar.ancestry4));
                    if (!g_BloodlineNames.empty()) {
                        ImGui::SeparatorText("Bloodlines (from game) - СПРАВКА, не настройка##cc_names");
                        ImGui::TextDisabled("Виден, только когда игра загрузила список для текущего вида (напр. Alvarin).");
                        ImGui::TextDisabled("Индекс [0],[1] тут = позиция в списке вида, НЕ EMainRace!");
                        for (int i = 0; i < (int)g_BloodlineNames.size() && i < 64; i++) {
                            char nm[128] = {};
                            WideCharToMultiByte(CP_UTF8, 0, g_BloodlineNames[i].c_str(), (int)g_BloodlineNames[i].size(), nm, sizeof(nm) - 1, nullptr, nullptr);
                            ImGui::BulletText("[%d] %s", i, nm);
                        }
                    }
                    else {
                        ImGui::TextDisabled("(bloodline names load on Read, while on Bloodline step)");
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Look##mcc")) {
                    ImGui::TextDisabled("DecorationData. -1 = leave as-is.");
                    ImGui::TextDisabled("Tattoo Type = КАКОЙ УЗОР (тип/категория), Tattoo = индекс самого узора.");
                    CC_IN("Hair type##cc_ht", hairType, -1, 1000);
                    CC_IN("Hair color##cc_hc", hairColor, -1, 1000);
                    CC_IN("Beard type##cc_bt", beardType, -1, 1000);
                    CC_IN("Beard color##cc_bc", beardColor, -1, 1000);
                    CC_IN("Skin color##cc_sk", skinColor, -1, 1000);
                    CC_IN("Eye color##cc_ey", eyeColor, -1, 1000);
                    CC_IN("Skin deco A##cc_sa", skinDecoA, -1, 1000);
                    CC_IN("Skin deco B##cc_sb", skinDecoB, -1, 1000);
                    CC_IN("Skin deco C##cc_sc", skinDecoC, -1, 1000);
                    CC_IN("Face tattoo type##cc_ftt", faceTattooType, -1, 1000);
                    CC_IN("Face tattoo##cc_ft", faceTattoo, -1, 1000);
                    CC_IN("Body tattoo type##cc_btt", bodyTattooType, -1, 1000);
                    CC_IN("Body tattoo##cc_bt", bodyTattoo, -1, 1000);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Race/Attrs##mcc")) {
                    ImGui::TextDisabled("Carousels. -1 = leave as-is.");
                    ImGui::TextDisabled("Race = НАЦИЯ (народность). Clade = КЛАН внутри нации (старт. умения/бонусы).");
                    ImGui::TextDisabled("Species (биологический вид) - в табе Body; Ancestry (4 предка) - в табе Ancestry.");
                    CC_IN("Race##cc_race", race, -1, 1000);
                    CC_IN("Gender##cc_gen", gender, -1, 1000);
                    CC_IN("Clade##cc_clade", clade, -1, 1000);
                    ImGui::Separator();
                    CC_IN("Str##cc_aStr", attrStr, -1, 1000);
                    CC_IN("Dex##cc_aDex", attrDex, -1, 1000);
                    CC_IN("Con##cc_aCon", attrCon, -1, 1000);
                    CC_IN("Int##cc_aInt", attrInt, -1, 1000);
                    CC_IN("Psy##cc_aPsy", attrPsy, -1, 1000);
                    CC_IN("Age (attr carousel)##cc_aAge", attrAge, -1, 1000);
                    CC_IN("Size##cc_aSize", attrSize, -1, 1000);
                    CC_IN("Attr Points##cc_aPts", attrPoints, -1, 100000);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            #undef CC_IN

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}