#include "jb_warden_features.h"
#include <random>
#include <cstdio>
#include <algorithm>

jb_warden_features g_jb_warden_features;
PLUGIN_EXPOSE(jb_warden_features, g_jb_warden_features);
#define MAX_PLAYERS 64

// SYSTEM API`s
IVEngineServer2* engine = nullptr;
CGlobalVars* gpGlobals = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

// API
IUtilsApi* utils;
IPlayersApi* players_api;
IJailbreakApi* jailbreak_api;


bool b_debug = true;

int iHealth;
int iArmor;
int iRed;
int iGreen;
int iBlue;
float flSpeed;
std::string sSoundAll;
std::string sSoundLocal;
std::string sClantag;



void PlaySlotSound(int iSlot, const char* path){
    auto pController = CCSPlayerController::FromSlot(iSlot);
    if (!pController) return;
    players_api->EmitSound(iSlot,pController->entindex(),path,1,3.0);
}

void PlaySoundAll(const char* path){
    for(int i = 0; i < MAX_PLAYERS;i++){
        auto pController = CCSPlayerController::FromSlot(i);
        if (!pController) continue;
        players_api->EmitSound(i,pController->entindex(),path,1,3.0);
    }
}

void LoadConfig() {
    KeyValues* config = new KeyValues("Config");
    const char* path = "addons/configs/Jailbreak/warden_features.ini";
    if (!config->LoadFromFile(g_pFullFileSystem, path)) {
        utils->ErrorLog("%s Failed to load: %s", g_PLAPI->GetLogTag(), path);
        delete config;
        return;
    }

    iHealth = config->GetInt("Health", 100);
    iArmor = config->GetInt("Armor", 100);

    iRed = config->GetInt("Red", 0);
    iGreen = config->GetInt("Green", 0);
    iBlue = config->GetInt("Blue", 255);

    flSpeed = config->GetFloat("Speed", 1.0f);
    sSoundAll = config->GetString("SoundAll", "");
    sSoundLocal = config->GetString("SoundLocal", "");

    sClantag = config->GetString("Clantag","[КМД]");

    delete config;
}

CGameEntitySystem* GameEntitySystem() {
    return utils ? utils->GetCGameEntitySystem() : nullptr;
}

void UpdateScoreboardUI() {
    if (!utils) return;
    
    IGameEventManager2* pEventMgr = utils->GetGameEventManager();
    if (!pEventMgr) return;

    IGameEvent* pEvent = pEventMgr->CreateEvent("nextlevel_changed", false);
    if (pEvent) {
        pEvent->SetString("nextlevel", "unknown");
        pEvent->SetString("skirmishmode", "default");
        
        pEventMgr->FireEvent(pEvent, false);
    }
}

void ClearClangtag(CCSPlayerController* pc) {
    pc->m_szClan() = "\0";
    utils->SetStateChanged(pc, "CCSPlayerController", "m_szClan");
    UpdateScoreboardUI();
}

void ClearAllTags(){
    for (int i = 0; i < MAX_PLAYERS;i++) {
        auto pc = CCSPlayerController::FromSlot(i);
        if (!pc) return;
        pc->m_szClan() = "\0";
        utils->SetStateChanged(pc, "CCSPlayerController", "m_szClan");
    }
    UpdateScoreboardUI();
}

void StartupServer() {
    g_pGameEntitySystem = GameEntitySystem();
    g_pEntitySystem = utils->GetCEntitySystem();
    gpGlobals = utils->GetCGlobalVars();

    LoadConfig();

    jailbreak_api->OnNewWardenListener(g_PLID,[](int iSlot){
        auto pController = CCSPlayerController::FromSlot(iSlot);
        if (!pController) return;
        auto pPawn = pController->GetPlayerPawn();
        if (!pPawn || !pPawn->IsAlive()) return;

        pPawn->m_iHealth = iHealth;
        pPawn->m_iMaxHealth = iHealth;
        pPawn->m_ArmorValue = iArmor;
        pPawn->m_flVelocityModifier = flSpeed;

        pController->m_szClan() = CUtlSymbolLarge(sClantag.c_str());
        utils->SetStateChanged(pController, "CCSPlayerController", "m_szClan");
        UpdateScoreboardUI();

        Color clr(iRed, iGreen, iBlue, 255);
        pPawn->m_clrRender.Set(clr);

        utils->SetStateChanged(pPawn, "CBaseEntity", "m_iHealth");
        utils->SetStateChanged(pPawn, "CBaseEntity", "m_iMaxHealth");
        utils->SetStateChanged(pPawn, "CCSPlayerPawn", "m_ArmorValue");
        utils->SetStateChanged(pPawn, "CCSPlayerPawn", "m_flVelocityModifier");
        utils->SetStateChanged(pPawn, "CBaseModelEntity", "m_clrRender");

        auto itemservice = pPawn->m_pItemServices.Get();
        if (!itemservice) return;
        itemservice->m_bHasHelmet = true;
        utils->SetStateChanged(pPawn,"CCSPlayer_ItemServices","m_bHasHelmet");

        if (!sSoundAll.empty()) {
            PlaySoundAll(sSoundAll.c_str());
        }
        if (!sSoundLocal.empty()) {
            PlaySlotSound(iSlot, sSoundLocal.c_str());
        }
    });

    jailbreak_api->OnWardenDieListener(g_PLID,[](int iSlot){
        auto pController = CCSPlayerController::FromSlot(iSlot);
        if (!pController) return;
        pController->m_szClan() = CUtlSymbolLarge("\0");
        utils->SetStateChanged(pController, "CCSPlayerController", "m_szClan");
        UpdateScoreboardUI();
    });

    utils->HookEvent(g_PLID,"player_death",[](const char* szName, IGameEvent* pEvent, bool bDontBroadcast){
        int iSlot = pEvent->GetInt("userid");
        auto pc = CCSPlayerController::FromSlot(iSlot);
        if (!pc) return;
        ClearClangtag(pc);
    });
    utils->HookEvent(g_PLID,"round_end",[](const char* szName, IGameEvent* pEvent, bool bDontBroadcast){
        ClearAllTags();
    });
}

bool jb_warden_features::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late) {
    PLUGIN_SAVEVARS();

    GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkSystem, INetworkSystem, NETWORKSYSTEM_INTERFACE_VERSION);

    ConVar_Register(FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);
    g_SMAPI->AddListener(this, this);

    return true;
}

void jb_warden_features::AllPluginsLoaded() {
    int ret;
    utils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing UTILS plugin.\n", g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    jailbreak_api = (IJailbreakApi*)g_SMAPI->MetaFactory(JAILBREAK_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing Jailbreak Core plugin.\n", g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    players_api = (IPlayersApi*)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing UTILS plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    utils->StartupServer(g_PLID, StartupServer);
}

bool jb_warden_features::Unload(char* error, size_t maxlen) {
    jailbreak_api->ClearAllPluginHooks(g_PLID);
    utils->ClearAllHooks(g_PLID);
    ConVar_Unregister();
    return true;
}

const char* jb_warden_features::GetAuthor() { return "niffox"; }
const char* jb_warden_features::GetDate() { return __DATE__; }
const char* jb_warden_features::GetDescription() { return "[JB] Warden Features"; }
const char* jb_warden_features::GetLicense() { return "Private"; }
const char* jb_warden_features::GetLogTag() { return "[JB] Warden Features"; }
const char* jb_warden_features::GetName() { return "[JB] Warden Features"; }
const char* jb_warden_features::GetURL() { return "https://t.me/niffox_2q"; }
const char* jb_warden_features::GetVersion() { return "1.0.2"; }