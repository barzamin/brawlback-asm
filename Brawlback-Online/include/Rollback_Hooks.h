#pragma once
#include <types.h>
#include <OS/OSInterrupt.h>
#include <OS/OSError.h>
#include <gf/gf_frame.h>
#include <gf/gf_pad_system.h>
#include <ft/ft_manager.h>
#include <gm/gm_global.h>
#include <gm/gm_global_mode_melee.h>
#include <gf/gf_game_application.h>
#include <gf/gf_scene.h>
#include <sc/sc_melee.h>
#include <mt/mt_rand.h>
#include <OS/OSTime.h>
#include <ExiStructures.h>
#include <ft/ft_manager.h>

#include "EXI_hooks.h"
#include "utils.h"
#include "exi_packet.h"

#if 1
#define NETPLAY_IMPL
#define ROLLBACK_IMPL
#endif

bu32 getCurrentFrame();

void fillOutGameSettings(GameSettings& settings);
void MergeGameSettingsIntoGame(GameSettings& settings);
extern bool canRollback;
extern bu32 frameCounter;
extern bool shouldTrackAllocs;
extern bool doDumpList;
extern bool isRollback;

namespace FrameLogic {
    // Functions
    void WriteInputsForFrame(bu32 currentFrame);
    void FrameDataLogic(bu32 currentFrame);
    void SendFrameCounterPointerLoc();
    bool ShouldSkipGfTaskProcess(bu32* gfTask, bu32 task_type);

    // Hooks
    void initFrameCounter();
    void updateFrameCounter();
    void beginningOfMainGameLoop();
    void beginFrame();
    void endFrame();
    void endMainLoop();
    void gfTaskProcessHook();
}
namespace FrameAdvance {
    // Variables
    extern bu32 framesToAdvance;
    extern FrameData currentFrameData;
    
    // Functions
    bu32 getFramesToAdvance();
    void TriggerFastForwardState(bu8 numFramesToFF);
    void StallOneFrame();
    void ResetFrameAdvance();
    void GetInputsForFrame(bu32 frame, FrameData* inputs);
    void ProcessGameSimulationFrame(FrameData* inputs);
    void setFrameAdvanceFromEmu();
    void getGamePadStatusInjection(gfPadSystem* padSystem, bu32 padStatus);

    // Hooks
    void updateLowHook();
    void handleFrameAdvanceHook();
    void updateIpSwitchPreProcess();
}

// TODO: put this in the submodule and pack it
struct GameReport {
    f64 damage[MAX_NUM_PLAYERS];
    s32 stocks[MAX_NUM_PLAYERS];
    s32 frame_duration;
};

namespace Util {
    void printInputs(const BrawlbackPad& pad);
    void printGameInputs(const gfPadStatus& pad);
    void printFrameData(const FrameData& fd);
    void SyncLog(const BrawlbackPad& pad, bu8 playerIdx);
    void FixFrameDataEndianness(FrameData* fd);
    BrawlbackPad GamePadToBrawlbackPad(const gfPadStatus& pad);
    void PopulatePlayerFrameData(PlayerFrameData& pfd, bu8 port, bu8 pIdx);
    void InjectBrawlbackPadToPadStatus(gfPadStatus& gamePad, const BrawlbackPad& pad, int port);
    void SaveState(bu32 currentFrame);
}
namespace Match {
    extern bu32 allocSizeTracker;
    extern char allocHeapName[30];

    void PopulateGameReport(GameReport& report);
    void SendGameReport(GameReport& report);
    void StopGameScMeleeHook();
    void StartSceneMelee();
    void ExitSceneMelee();
    void setRandSeed();
    void dump_gfMemoryPool_hook();
    void DumpGfMemoryPoolHook(char** r30_reg_val, bu32 addr_start, bu32 addr_end, bu32 mem_size, u8 id);
    void ProcessGameAllocation(u8* allocated_addr, bu32 size, char* heap_name);
    void ProcessGameFree(u8* address, char* heap_name);
    void alloc_gfMemoryPool_hook();
    void AllocGfMemoryPoolBeginHook(char** internal_heap_data, bu32 size, bu32 alignment);
    void allocGfMemoryPoolEndHook();
    void free_gfMemoryPool_hook();
    void FreeGfMemoryPoolHook(char** internal_heap_data, u8* address);
}

namespace Netplay {
    // Variables
    extern GameSettings gameSettings;
    extern const bu8 localPlayerIdxInvalid;
    extern bu8 localPlayerIdx;
    extern bool isInMatch;

    // Functions
    void StartMatching();
    bool CheckIsMatched();
    void EndMatch();

    bool IsInMatch();
    void SetIsInMatch(bool isMatch);

    int getLocalPlayerIdx();
    GameSettings& getGameSettings();
}

namespace NetMatchMakingStatus {
    const bu32 LinkDown = 0x0;
    const bu32 LinkUp = 0x1;
    const bu32 DWCReady = 0x2;
    const bu32 WaitLogin = 0x3;
    const bu32 Download = 0x4;
    const bu32 Online = 0x5;
    const bu32 MatchingStart = 0x6;
    const bu32 Matching = 0x7;
    const bu32 MatchingWaitShare = 0x8;
    const bu32 P2P = 0x9;
    const bu32 DisconnectAll = 0xa;
    const bu32 WaitShutdown = 0xb;
    const bu32 Error = 0xc;
}

namespace Scene {
    const u32 None = 0x0;
    const u32 MemoryChange = 0x1;
    const u32 Idle = 0x2;
    const u32 InitialChange = 0x3;
    const u32 SelectCharacter = 0x4;
    const u32 Unk5 = 0x5;
    const u32 SelStage = 0x6;
    const u32 Unk7 = 0x7;
    const u32 Unk8 = 0x8;
    const u32 Melee = 0x9;
    const u32 Unk10 = 0xa;
    const u32 Unk11 = 0xb;
    const u32 VsResult = 0xc;
    const u32 Unk13 = 0xd;
    const u32 PrizeCheck = 0xe;
    const u32 Unk15 =	0xf;
    const u32 MenuMain = 0x10;
}

extern u8 defaultGmGlobalModeMelee[0x320];
namespace GMMelee {
    // Variables
    extern bool isMatchChoicesPopulated;
    extern int charChoices[MAX_NUM_PLAYERS];
    extern int stageChoice;
    #define STAGE_ID_IDX 27

    // Functions
    void PopulateMatchSettings(int chars[4], int stageID);
    void ResetMatchChoicesPopulated();

    // Hooks
    void postSetupMelee();
}

namespace RollbackHooks {
    void InstallHooks();
}