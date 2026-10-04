#pragma once
#include <string>
#include <json.hpp>

// --- Phase 1 Attacks ---
#include "AttackRadial.h"
#include "AttackFan.h"
#include "AttackPhalanx.h"
#include "AttackRain.h"
#include "AttackUltimate.h"
#include "AttackWave.h"
#include "AttackMeteor.h"
#include "AttackDirect.h"

// --- Phase 2 Attacks ---
#include "AttackBouncing.h"
#include "AttackBoomerangs.h"
#include "AttackBlasters.h"
#include "AttackSpears.h"

using json = nlohmann::json;

// Every tunable attack parameter block, grouped so a load can replace all of them at once.
struct AttackParamSet {
    // Phase 01 (bullet hell)
    RadialParams   radialNormal{};
    RadialParams   radialContinuous{};
    FanParams      fanNormal{};
    FanParams      fanContinuous{};
    PhalanxParams  phalanx{};
    RainParams     rain{};
    RainParams     rainTargeted{};
    UltimateParams ultimate{};
    WaveParams     wave{};
    MeteorParams   meteor{};
    DirectParams   direct{};

    // Phase 02 (Windowkill)
    BouncingBulletParams bouncing{};
    BoomerangParams      boomerang{};
    BlasterParams        blaster{};
    UndyneSpearParams    spear{};
};

class AttackParamManager {
public:
    static AttackParamManager& Instance() {
        static AttackParamManager instance;
        return instance;
    }

    // Replaces all params with struct defaults overridden by the file.
    // On any failure (open, syntax, wrong value type) the current params are left untouched.
    bool Load(const std::string& filepath);

    // Loads again from the path given to the last Load() call.
    bool Reload() { return Load(m_filepath); }

    // ========================================================
    // GETTER: PHASE 01
    // ========================================================
    RadialParams& GetRadialNormalParams() { return m_params.radialNormal; }
    RadialParams& GetRadialContinuousParams() { return m_params.radialContinuous; }
    FanParams& GetFanNormalParams() { return m_params.fanNormal; }
    FanParams& GetFanContinuousParams() { return m_params.fanContinuous; }
    PhalanxParams& GetPhalanxParams() { return m_params.phalanx; }
    RainParams& GetRainParams() { return m_params.rain; }
    RainParams& GetRainTargetedParams() { return m_params.rainTargeted; }
    UltimateParams& GetUltimateParams() { return m_params.ultimate; }
    WaveParams& GetWaveParams() { return m_params.wave; }
    MeteorParams& GetMeteorParams() { return m_params.meteor; }
    DirectParams& GetDirectParams() { return m_params.direct; }

    // ========================================================
    // GETTER: PHASE 02 (WINDOWKILL)
    // ========================================================
    BouncingBulletParams& GetBouncingParams() { return m_params.bouncing; }
    BoomerangParams& GetBoomerangParams() { return m_params.boomerang; }
    BlasterParams& GetBlasterParams() { return m_params.blaster; }
    UndyneSpearParams& GetUndyneParams() { return m_params.spear; }

private:
    AttackParamManager() = default;

    // Fungsi Internal Parser
    void ParseRadialParams(const json& j, RadialParams& outParams);
    void ParseFanParams(const json& j, FanParams& outParams);
    void ParsePhalanxParams(const json& j, PhalanxParams& outParams);
    void ParseRainParams(const json& j, RainParams& outParams);
    void ParseUltimateParams(const json& j, UltimateParams& outParams);
	void ParseWaveParams(const json& j, WaveParams& outParams);
	void ParseMeteorParams(const json& j, MeteorParams& outParams);
    void ParseDirectParams(const json& j, DirectParams& outParams);

    void ParseBouncingParams(const json& j, BouncingBulletParams& outParams);
    void ParseBoomerangParams(const json& j, BoomerangParams& outParams);
    void ParseBlasterParams(const json& j, BlasterParams& outParams);
    void ParseSpearParams(const json& j, UndyneSpearParams& outParams);

    AttackParamSet m_params{};   // live values, edited by the debug panel
    std::string    m_filepath{}; // path of the last Load(), used by Reload()
};