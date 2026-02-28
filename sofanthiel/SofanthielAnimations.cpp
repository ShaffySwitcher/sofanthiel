#include "Sofanthiel.h"
#include "IconsFontAwesome6.h"
#include "InputManager.h"

void Sofanthiel::handleAnimCels()
{
    ImGui::Begin("Animation Cels", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button(ICON_FA_PLUS " New Cel")) {
        showNewCelPopup = true;
        memset(newCelNameBuffer, 0, sizeof(newCelNameBuffer));
    }

    if (!animationCels.empty()) {
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.60f, 1.0f), "%d cels", static_cast<int>(animationCels.size()));
    }

    ImGui::Separator();

    if (showNewCelPopup) {
        ImGui::OpenPopup("New Animation Cel");
    }

    if (ImGui::BeginPopupModal("New Animation Cel", &showNewCelPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter a name for the new animation cel:");
        ImGui::InputText("##celname", newCelNameBuffer, sizeof(newCelNameBuffer));

        bool nameValid = strlen(newCelNameBuffer) > 0;
        bool nameExists = nameValid && !isCelNameUnique(newCelNameBuffer);

        if (nameExists) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "A cel with this name already exists!");
            ImGui::BeginDisabled(true);
        }
        else if (!nameValid) {
            ImGui::BeginDisabled(true);
        }

        if (ImGui::Button("Create", getScaledButtonSize(120, 0)) || (nameValid && !nameExists && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            AnimationCel newCel;
            newCel.name = newCelNameBuffer;
            animationCels.push_back(newCel);

            showNewCelPopup = false;
            ImGui::CloseCurrentPopup();
        }

        if (nameExists || !nameValid) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", getScaledButtonSize(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            showNewCelPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (showRenameCelPopup) {
        ImGui::OpenPopup("Rename Animation Cel");
    }

    if (ImGui::BeginPopupModal("Rename Animation Cel", &showRenameCelPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (renamingCelIndex >= 0 && renamingCelIndex < animationCels.size()) {
            ImGui::Text("Enter new name for '%s':", animationCels[renamingCelIndex].name.c_str());
            ImGui::InputText("##renamecel", renameCelNameBuffer, sizeof(renameCelNameBuffer));

            bool nameValid = strlen(renameCelNameBuffer) > 0;
            bool nameExists = nameValid && !isCelNameUnique(renameCelNameBuffer, renamingCelIndex);

            if (nameExists) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "A cel with this name already exists!");
                ImGui::BeginDisabled(true);
            }
            else if (!nameValid) {
                ImGui::BeginDisabled(true);
            }

            if (ImGui::Button("Rename", getScaledButtonSize(120, 0)) || (nameValid && !nameExists && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
                std::string oldName = animationCels[renamingCelIndex].name;
                std::string newName = renameCelNameBuffer;

                animationCels[renamingCelIndex].name = newName;

                for (auto& anim : animations) {
                    for (auto& entry : anim.entries) {
                        if (entry.celName == oldName) {
                            entry.celName = newName;
                        }
                    }
                }

                showRenameCelPopup = false;
                renamingCelIndex = -1;
                ImGui::CloseCurrentPopup();
            }

            if (nameExists || !nameValid) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", getScaledButtonSize(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showRenameCelPopup = false;
                renamingCelIndex = -1;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    ImGui::BeginChild("AnimationCelsList", ImVec2(0, 0), ImGuiChildFlags_None);

    if (animationCels.empty()) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float textHeight = ImGui::GetTextLineHeight() * 2 + ImGui::GetStyle().ItemSpacing.y;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - textHeight) * 0.5f);

        const char* msg = ICON_FA_LAYER_GROUP " No cels";
        ImGui::SetCursorPosX((avail.x - ImGui::CalcTextSize(msg).x) * 0.5f);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), "%s", msg);

        const char* sub = "Double-click a cel to edit it";
        ImGui::SetCursorPosX((avail.x - ImGui::CalcTextSize(sub).x) * 0.5f);
        ImGui::TextColored(ImVec4(0.40f, 0.40f, 0.45f, 1.0f), "%s", sub);
    }

    float availWidth = ImGui::GetContentRegionAvail().x;

    for (int i = 0; i < static_cast<int>(animationCels.size()); i++) {
        const AnimationCel& cel = animationCels[i];

        ImGui::PushID(i);

        bool isEditing = (editingCelIndex == i && celEditingMode);
        char label[256];
        snprintf(label, sizeof(label), ICON_FA_IMAGE " %s  (%d OAMs)", cel.name.c_str(), static_cast<int>(cel.oams.size()));

        if (ImGui::Selectable(label, isEditing, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(availWidth, 0))) {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                this->celEditingMode = true;
                this->editingCelIndex = i;
                this->selectedOAMIndices.clear();
            }
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Double-click to edit '%s'", cel.name.c_str());
        }

        if (ImGui::BeginPopupContextItem("##cel_context")) {
            if (ImGui::MenuItem(ICON_FA_PEN " Rename")) {
                showRenameCelPopup = true;
                renamingCelIndex = i;
                strncpy(renameCelNameBuffer, animationCels[i].name.c_str(), sizeof(renameCelNameBuffer) - 1);
                renameCelNameBuffer[sizeof(renameCelNameBuffer) - 1] = '\0';
            }
            if (ImGui::MenuItem(ICON_FA_COPY " Copy")) {
                celClipboard = cel;
                hasCelClipboard = true;
            }

            if (ImGui::MenuItem(ICON_FA_PASTE " Paste", nullptr, false, hasCelClipboard)) {
                AnimationCel pastedCel = celClipboard;
                std::string baseName = pastedCel.name;
                std::string newName = baseName;
                int counter = 1;

                bool nameExists;
                do {
                    nameExists = false;
                    for (const auto& existingCel : animationCels) {
                        if (existingCel.name == newName) {
                            nameExists = true;
                            newName = baseName + "_" + std::to_string(counter++);
                            break;
                        }
                    }
                } while (nameExists);

                pastedCel.name = newName;
                animationCels.push_back(pastedCel);
            }

            ImGui::Separator();

            if (ImGui::MenuItem(ICON_FA_TRASH " Remove")) {
                animationCels.erase(animationCels.begin() + i);
                if (editingCelIndex >= static_cast<int>(animationCels.size())) {
                    editingCelIndex = -1;
                    celEditingMode = false;
                }
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            int payload_n = i;
            ImGui::SetDragDropPayload("DND_ANIM_CELL", &payload_n, sizeof(int));
            ImGui::Text("%s", cel.name.c_str());
            ImGui::EndDragDropSource();
        }

        ImGui::PopID();
    }

    if (ImGui::IsWindowFocused() && editingCelIndex >= 0 && editingCelIndex < animationCels.size()) {
        if (InputManager::isPressed(InputManager::Copy)) {
            celClipboard = animationCels[editingCelIndex];
            hasCelClipboard = true;
        }

        if (InputManager::isPressed(InputManager::Paste) && hasCelClipboard) {
            AnimationCel pastedCel = celClipboard;

            std::string baseName = pastedCel.name;
            std::string newName = baseName;
            int counter = 1;

            bool nameExists;
            do {
                nameExists = false;
                for (const auto& existingCel : animationCels) {
                    if (existingCel.name == newName) {
                        nameExists = true;
                        newName = baseName + "_" + std::to_string(counter++);
                        break;
                    }
                }
            } while (nameExists);

            pastedCel.name = newName;
            animationCels.push_back(pastedCel);
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

void Sofanthiel::handleAnims()
{
    ImGui::Begin("Animations", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button(ICON_FA_PLUS " New Animation")) {
        showNewAnimationPopup = true;
        memset(newAnimationNameBuffer, 0, sizeof(newAnimationNameBuffer));
    }

    if (!animations.empty() && currentAnimation >= 0 && currentAnimation < static_cast<int>(animations.size())) {
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        Animation& curAnim = animations[currentAnimation];
        ImGui::TextColored(ImVec4(0.65f, 0.75f, 0.90f, 1.0f), ICON_FA_FILM " %d entries  " ICON_FA_CLOCK " %d frames",
            static_cast<int>(curAnim.entries.size()), totalFrames);
    }

    if (showNewAnimationPopup) {
        ImGui::OpenPopup("New Animation");
    }

    if (ImGui::BeginPopupModal("New Animation", &showNewAnimationPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter a name for the new animation:");
        ImGui::InputText("##animname", newAnimationNameBuffer, sizeof(newAnimationNameBuffer));

        bool nameValid = strlen(newAnimationNameBuffer) > 0;
        bool nameExists = nameValid && !isAnimationNameUnique(newAnimationNameBuffer);

        if (nameExists) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "An animation with this name already exists!");
            ImGui::BeginDisabled(true);
        }
        else if (!nameValid) {
            ImGui::BeginDisabled(true);
        }

        if (ImGui::Button("Create", getScaledButtonSize(120, 0)) || (nameValid && !nameExists && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            Animation newAnimation;
            newAnimation.name = newAnimationNameBuffer;
            animations.push_back(newAnimation);
            currentAnimation = static_cast<int>(animations.size()) - 1;
            recalculateTotalFrames();

            showNewAnimationPopup = false;
            ImGui::CloseCurrentPopup();
        }

        if (nameExists || !nameValid) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", getScaledButtonSize(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            showNewAnimationPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (showRenameAnimationPopup) {
        ImGui::OpenPopup("Rename Animation");
    }

    if (ImGui::BeginPopupModal("Rename Animation", &showRenameAnimationPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (renamingAnimationIndex >= 0 && renamingAnimationIndex < static_cast<int>(animations.size())) {
            ImGui::Text("Enter new name for '%s':", animations[renamingAnimationIndex].name.c_str());
            ImGui::InputText("##renameanim", renameAnimationNameBuffer, sizeof(renameAnimationNameBuffer));

            bool nameValid = strlen(renameAnimationNameBuffer) > 0;
            bool nameExists = nameValid && !isAnimationNameUnique(renameAnimationNameBuffer, renamingAnimationIndex);

            if (nameExists) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "An animation with this name already exists!");
                ImGui::BeginDisabled(true);
            }
            else if (!nameValid) {
                ImGui::BeginDisabled(true);
            }

            if (ImGui::Button("Rename", getScaledButtonSize(120, 0)) || (nameValid && !nameExists && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
                animations[renamingAnimationIndex].name = renameAnimationNameBuffer;

                showRenameAnimationPopup = false;
                renamingAnimationIndex = -1;
                ImGui::CloseCurrentPopup();
            }

            if (nameExists || !nameValid) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", getScaledButtonSize(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showRenameAnimationPopup = false;
                renamingAnimationIndex = -1;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    if (!animations.empty()) {
        ImGuiTabBarFlags tabFlags = ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton;
        if (ImGui::BeginTabBar("AnimationsTabBar", tabFlags)) {
            for (int i = 0; i < static_cast<int>(animations.size()); i++) {
                ImGui::PushID(i);
                Animation& anim = animations[i];

                if (ImGui::BeginTabItem(anim.name.c_str())) {
                    if (currentAnimation != i) {
                        currentAnimation = i;
                        recalculateTotalFrames();
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginPopupContextItem("##anim_tab_ctx")) {
                    if (ImGui::MenuItem(ICON_FA_PEN " Rename")) {
                        showRenameAnimationPopup = true;
                        renamingAnimationIndex = i;
                        strncpy(renameAnimationNameBuffer, anim.name.c_str(), sizeof(renameAnimationNameBuffer) - 1);
                        renameAnimationNameBuffer[sizeof(renameAnimationNameBuffer) - 1] = '\0';
                    }
                    if (ImGui::MenuItem(ICON_FA_COPY " Copy")) {
                        animationClipboard = anim;
                        hasAnimationClipboard = true;
                    }
                    if (ImGui::MenuItem(ICON_FA_PASTE " Paste", nullptr, false, hasAnimationClipboard)) {
                        Animation pastedAnimation = animationClipboard;

                        std::string baseName = pastedAnimation.name;
                        std::string newName = baseName;
                        int counter = 1;

                        bool nameExists;
                        do {
                            nameExists = false;
                            for (const auto& existingAnim : animations) {
                                if (existingAnim.name == newName) {
                                    nameExists = true;
                                    newName = baseName + "_" + std::to_string(counter++);
                                    break;
                                }
                            }
                        } while (nameExists);

                        pastedAnimation.name = newName;
                        animations.push_back(pastedAnimation);
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem(ICON_FA_TRASH " Remove")) {
                        animations.erase(animations.begin() + i);
                        if (currentAnimation >= static_cast<int>(animations.size())) {
                            currentAnimation = animations.empty() ? -1 : static_cast<int>(animations.size()) - 1;
                        }
                        recalculateTotalFrames();
                        ImGui::EndPopup();
                        ImGui::PopID();
                        ImGui::EndTabBar();
                        ImGui::End();
                        return;
                    }

                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
            ImGui::EndTabBar();
        }
    } else {
        ImGui::Separator();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float textHeight = ImGui::GetTextLineHeight() * 2 + ImGui::GetStyle().ItemSpacing.y;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - textHeight) * 0.5f);

        const char* msg = ICON_FA_FILM " No animations";
        ImGui::SetCursorPosX((avail.x - ImGui::CalcTextSize(msg).x) * 0.5f);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), "%s", msg);

        const char* sub = "Click 'New Animation' to create one";
        ImGui::SetCursorPosX((avail.x - ImGui::CalcTextSize(sub).x) * 0.5f);
        ImGui::TextColored(ImVec4(0.40f, 0.40f, 0.45f, 1.0f), "%s", sub);
    }

    if (currentAnimation >= 0 && currentAnimation < static_cast<int>(animations.size())) {
        if (InputManager::isPressed(InputManager::Copy)) {
            animationClipboard = animations[currentAnimation];
            hasAnimationClipboard = true;
        }

        if (InputManager::isPressed(InputManager::Paste) && hasAnimationClipboard) {
            Animation pastedAnimation = animationClipboard;

            std::string baseName = pastedAnimation.name;
            std::string newName = baseName;
            int counter = 1;

            bool nameExists;
            do {
                nameExists = false;
                for (const auto& existingAnim : animations) {
                    if (existingAnim.name == newName) {
                        nameExists = true;
                        newName = baseName + "_" + std::to_string(counter++);
                        break;
                    }
                }
            } while (nameExists);

            pastedAnimation.name = newName;
            animations.push_back(pastedAnimation);
        }
    }

    ImGui::End();
}
