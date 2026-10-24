#define _ITERATOR_DEBUG_LEVEL 0
#define _HAS_ITERATOR_DEBUGGING 0

#include "mods/service.hpp"
#include "mods/svc/log.h"
#include "mods/svc/config.h"
#include "mods/svc/ui.h"
#include "d/d_kankyo.h"
#include "d/d_kankyo_wether.h"

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(UiService, svc_ui);

enum WeatherType {
    WEATHER_CLEAR = 0,
    WEATHER_RAINY = 1,
    WEATHER_FOGGY = 2
};

enum FogMode {
    FOG_NONE = 0,
    FOG_DEFAULT = 1,
    FOG_NEAR = 2,
    FOG_FAR  = 3
};

static ConfigVarHandle weatherVar       = 0;
static ConfigVarHandle ignoreFogVar     = 0;
static ConfigVarHandle rainIntensityVar = 0;
static ConfigVarHandle fogModeVar       = 0;
static ConfigVarHandle fogNearVar       = 0;
static ConfigVarHandle fogFarVar        = 0;
static ConfigVarHandle forceClearFogVar = 0;

static const char* weatherOptions[] = { "Clear", "Rainy", "Foggy" };
static const char* fogModeOptions[] = { "None", "Default", "Near", "Far" };

static void clear_all_fog() {
    dKy_fog_startendz_set(0.0f, 0.0f, 0.0f);
}

static bool is_not_rainy(ModContext* ctx, void*) {
    int64_t w = 0;
    svc_config->get_int(ctx, weatherVar, &w);
    return w != WEATHER_RAINY;
}

static bool is_not_foggy(ModContext* ctx, void*) {
    int64_t w = 0;
    svc_config->get_int(ctx, weatherVar, &w);
    return w != WEATHER_FOGGY;
}

static bool hide_ignore_fog(ModContext* ctx, void*) {
    int64_t w = 0;
    svc_config->get_int(ctx, weatherVar, &w);
    return w == WEATHER_FOGGY;
}

static ModResult build_panel(ModContext* ctx, UiElementHandle panel, void*, ModError*) {
    svc_ui->pane_add_section(ctx, panel, "Weather");

    UiControlDesc dropdown = UI_CONTROL_DESC_INIT;
    dropdown.kind = UI_CONTROL_DROPDOWN;
    dropdown.label = "Weather";
    dropdown.binding = UI_BINDING_CONFIG_VAR;
    dropdown.config_var = weatherVar;
    dropdown.options = weatherOptions;
    dropdown.option_count = 3;
    svc_ui->pane_add_control(ctx, panel, &dropdown, nullptr);

    UiControlDesc forceClear = UI_CONTROL_DESC_INIT;
    forceClear.kind = UI_CONTROL_TOGGLE;
    forceClear.label = "Force Clear Fog";
    forceClear.tooltip = "When enabled, bypasses Foggy settings and keeps all fog cleared.";
    forceClear.binding = UI_BINDING_CONFIG_VAR;
    forceClear.config_var = forceClearFogVar;
    svc_ui->pane_add_control(ctx, panel, &forceClear, nullptr);

    UiControlDesc ignoreFog = UI_CONTROL_DESC_INIT;
    ignoreFog.kind = UI_CONTROL_TOGGLE;
    ignoreFog.label = "Ignore Fog";
    ignoreFog.tooltip = "When enabled, will not change current fog settings.";
    ignoreFog.binding = UI_BINDING_CONFIG_VAR;
    ignoreFog.config_var = ignoreFogVar;
    ignoreFog.is_disabled = hide_ignore_fog;
    svc_ui->pane_add_control(ctx, panel, &ignoreFog, nullptr);

    UiControlDesc rain = UI_CONTROL_DESC_INIT;
    rain.kind = UI_CONTROL_NUMBER;
    rain.label = "Rain Intensity";
    rain.binding = UI_BINDING_CONFIG_VAR;
    rain.config_var = rainIntensityVar;
    rain.min = 0;
    rain.max = 500;
    rain.step = 1;
    rain.is_disabled = is_not_rainy;
    svc_ui->pane_add_control(ctx, panel, &rain, nullptr);

    UiControlDesc fogMode = UI_CONTROL_DESC_INIT;
    fogMode.kind = UI_CONTROL_DROPDOWN;
    fogMode.label = "Fog Mode";
    fogMode.binding = UI_BINDING_CONFIG_VAR;
    fogMode.config_var = fogModeVar;
    fogMode.options = fogModeOptions;
    fogMode.option_count = 4;
    fogMode.is_disabled = is_not_foggy;
    svc_ui->pane_add_control(ctx, panel, &fogMode, nullptr);

    UiControlDesc fogNear = UI_CONTROL_DESC_INIT;
    fogNear.kind = UI_CONTROL_NUMBER;
    fogNear.label = "Fog Near";
    fogNear.binding = UI_BINDING_CONFIG_VAR;
    fogNear.config_var = fogNearVar;
    fogNear.min = 0;
    fogNear.max = 10000;
    fogNear.step = 50;
    fogNear.is_disabled = is_not_foggy;
    svc_ui->pane_add_control(ctx, panel, &fogNear, nullptr);

    UiControlDesc fogFar = UI_CONTROL_DESC_INIT;
    fogFar.kind = UI_CONTROL_NUMBER;
    fogFar.label = "Fog Far";
    fogFar.binding = UI_BINDING_CONFIG_VAR;
    fogFar.config_var = fogFarVar;
    fogFar.min = 0;
    fogFar.max = 20000;
    fogFar.step = 50;
    fogFar.is_disabled = is_not_foggy;
    svc_ui->pane_add_control(ctx, panel, &fogFar, nullptr);

    return MOD_OK;
}

static void apply_fog(int64_t mode, int64_t nearVal, int64_t farVal) {

    float nearF = static_cast<float>(nearVal);
    float farF  = static_cast<float>(farVal);

    switch(mode)
    {
    case FOG_NONE:
        dKy_fog_startendz_set(farF, farF, 1.0f);
        break;
    case FOG_NEAR:
        dKy_fog_startendz_set(nearF, nearF + 500.0f, 1.0f);
        break;
    case FOG_FAR:
        dKy_fog_startendz_set(farF, farF + 2000.0f, 1.0f);
        break;
    default:
        return;
    }
}

MOD_EXPORT ModResult mod_initialize(ModError* error) {
    ConfigVarDesc weatherDesc = CONFIG_VAR_DESC_INIT;
    weatherDesc.name = "weather";
    weatherDesc.type = CONFIG_VAR_INT;
    weatherDesc.default_int = WEATHER_CLEAR;
    svc_config->register_var(mod_ctx, &weatherDesc, &weatherVar);

    ConfigVarDesc ignoreDesc = CONFIG_VAR_DESC_INIT;
    ignoreDesc.name = "ignore_fog";
    ignoreDesc.type = CONFIG_VAR_BOOL;
    ignoreDesc.default_bool = true;
    svc_config->register_var(mod_ctx, &ignoreDesc, &ignoreFogVar);

    ConfigVarDesc rainDesc = CONFIG_VAR_DESC_INIT;
    rainDesc.name = "rain_intensity";
    rainDesc.type = CONFIG_VAR_INT;
    rainDesc.default_int = 50;
    svc_config->register_var(mod_ctx, &rainDesc, &rainIntensityVar);

    ConfigVarDesc fogModeDesc = CONFIG_VAR_DESC_INIT;
    fogModeDesc.name = "fog_mode";
    fogModeDesc.type = CONFIG_VAR_INT;
    fogModeDesc.default_int = FOG_FAR;
    svc_config->register_var(mod_ctx, &fogModeDesc, &fogModeVar);

    ConfigVarDesc fogNearDesc = CONFIG_VAR_DESC_INIT;
    fogNearDesc.name = "fog_near";
    fogNearDesc.type = CONFIG_VAR_INT;
    fogNearDesc.default_int = 200;
    svc_config->register_var(mod_ctx, &fogNearDesc, &fogNearVar);

    ConfigVarDesc fogFarDesc = CONFIG_VAR_DESC_INIT;
    fogFarDesc.name = "fog_far";
    fogFarDesc.type = CONFIG_VAR_INT;
    fogFarDesc.default_int = 2000;
    svc_config->register_var(mod_ctx, &fogFarDesc, &fogFarVar);

    UiModsPanelDesc panelDesc = UI_MODS_PANEL_DESC_INIT;
    panelDesc.build = build_panel;
    svc_ui->register_mods_panel(mod_ctx, &panelDesc);

    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError* error) {
    int64_t weather = 0;
    bool ignoreFog = true;
    int64_t rain = 0, fogMode = 0, fogNear = 0, fogFar = 0;

    svc_config->get_int(mod_ctx, weatherVar, &weather);
    svc_config->get_bool(mod_ctx, ignoreFogVar, &ignoreFog);
    svc_config->get_int(mod_ctx, rainIntensityVar, &rain);
    svc_config->get_int(mod_ctx, fogModeVar, &fogMode);
    svc_config->get_int(mod_ctx, fogNearVar, &fogNear);
    svc_config->get_int(mod_ctx, fogFarVar, &fogFar);

    switch (weather) {
    case WEATHER_CLEAR:
        dKyw_rain_set(0);
        if (!ignoreFog) {
            clear_all_fog();
        }
        break;

    case WEATHER_RAINY:
        dKyw_rain_set(static_cast<int>(rain));
        if (!ignoreFog) {
            clear_all_fog();
        }
        break;

    case WEATHER_FOGGY:
        dKyw_rain_set(0);
        if (fogMode == FOG_DEFAULT) {
            clear_all_fog();
        } else if (fogMode == FOG_NONE)
            apply_fog(fogMode, 999999.0f, 999999.0f);
        else
            apply_fog(fogMode, fogNear, fogFar);
        break;
    }

    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError* error) {
    return MOD_OK;
}
