#pragma once

#include <Arduino.h>

struct CultivationProfile
{
    bool used = false;
    String name;
    float targetPh = 5.80f;
    float phTolerance = 0.10f;
    uint32_t doseDurationMs = 500;
    uint32_t doseIntervalMinutes = 4;
    // Se aplica solo a la regulación automática; no incluye dosis manuales.
    uint8_t maxDailyDoses = 3;
    bool automaticMode = true;

    float targetEc = 1.40f;
    uint8_t lightOnHour = 6;
    uint8_t lightOnMinute = 0;
    uint8_t lightOffHour = 18;
    uint8_t lightOffMinute = 0;
    bool lightScheduleEnabled = false;
    bool lightManualOn = false;
};

constexpr uint8_t MAX_CULTIVATION_PROFILES = 10;

int8_t getActiveProfileSlot();
void setActiveProfileSlot(int8_t slot);
bool loadProfile(uint8_t slot, CultivationProfile& profile);
void saveProfile(uint8_t slot, const CultivationProfile& profile);
void deleteProfile(uint8_t slot);
int8_t findFreeProfileSlot();
void applyProfileToConfig(const CultivationProfile& profile);
bool profileMatchesConfig(const CultivationProfile& profile);
void clearActiveProfileIfConfigChanged();
void handleGetProfiles();
void handleSaveProfile();
void handleApplyProfile();
void handleDeleteProfile();
