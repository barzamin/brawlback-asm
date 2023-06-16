#include "Rollback_Hooks.h"
#include "sy_core.h"
#include <EXI/EXIBios.h>
#include <gf/gf_heap_manager.h>
#include <modules.h>
#define P1_CHAR_ID_IDX 0x98
#define P2_CHAR_ID_IDX P1_CHAR_ID_IDX + 0x5C
#define P3_CHAR_ID_IDX P2_CHAR_ID_IDX + 0x5C
#define P4_CHAR_ID_IDX P3_CHAR_ID_IDX + 0x5C
bu32 frameCounter = 0;
bool shouldTrackAllocs = false;
bool doDumpList = false;
bool isRollback = false;

bu32 getCurrentFrame() {
    return frameCounter;
}

bool gameHasStarted() {
    scMelee* melee = dynamic_cast<scMelee*>(gfSceneManager::getInstance()->searchScene("scMelee"));
    if(melee)
    {
        return melee->m_operatorReadyGo->isEnd() != 0;
    }
    else 
    {
        return false;
    }

}

void fillOutGameSettings(GameSettings& settings) {
    settings.randomSeed = g_mtRandDefault.seed;
    settings.stageID = g_GameGlobal->m_modeMelee->m_meleeInitData.m_stageID;

    bu8 p1_id = g_GameGlobal->m_modeMelee->m_playersInitData[0].m_slotID;
    //OSReport("P1 pre-override char id: %d\n", p1_id);
    
    bu8 p2_id = g_GameGlobal->m_modeMelee->m_playersInitData[1].m_slotID;
    //OSReport("P2 pre-override char id: %d\n", p2_id);

    // brawl loads all players into the earliest slots.
    // I.E. if players choose P1 and P3, they will get loaded into P1 and P2
    // this means we can use the number of players in a match to iterate over
    // players since we know they'll always be next to each other

    // TODO: replace this with some way to get the actual number of players in a match.
    // unfortunately FIGHTER_MANAGER->getEntryCount() isn't filled out at this point in the game loading
    // sequence. Gotta find another way to get it, or some better spot to grab the number of players
    settings.numPlayers = 2;
    //OSReport("Num Players: %u\n", (unsigned int)settings.numPlayers);
    PlayerSettings playerSettings[2];
    playerSettings[0].charID = p1_id;
    playerSettings[1].charID = p2_id;
    
    for (int i = 0; i < settings.numPlayers; i++) {
        settings.playerSettings[i] = playerSettings[i];
    }
}


// take gamesettings and apply it to our game
void MergeGameSettingsIntoGame(GameSettings& settings) {
    //DEFAULT_MT_RAND->seed = settings->randomSeed;
    g_mtRandDefault.seed = 0x496ffd00; // hardcoded for testing (arbitrary number)
    g_mtRandOther.seed = 0x496ffd00;

    //GM_GLOBAL_MODE_MELEE->stageID = settings->stageID;
    //GM_GLOBAL_MODE_MELEE->stageID = 2;

    Netplay::localPlayerIdx = settings.localPlayerIdx;
    //OSReport("Local player index is %d\n", Netplay::localPlayerIdx);

    bu8 p1_char = settings.playerSettings[0].charID;
    bu8 p2_char = settings.playerSettings[1].charID;
    //OSReport("P1 char: %d  P2 char: %d\n", p1_char, p2_char);
    //OSReport("Stage id: %d\n", settings.stageID);

    int chars[MAX_NUM_PLAYERS] = {p1_char, p2_char, -1, -1};
    GMMelee::PopulateMatchSettings(chars, settings.stageID );
}

namespace Util {

    void printInputs(const BrawlbackPad& pad) {
        OSReport(" -- Pad --\n");
        OSReport("StickX: %hhu ", pad.stickX);
        OSReport("StickY: %hhu ", pad.stickY);
        OSReport("CStickX: %hhu ", pad.cStickX);
        OSReport("CStickY: %hhu\n", pad.cStickY);
        OSReport("Buttons: ");
        OSReport("Buttons: 0x%x", pad.buttons);
        OSReport("holdButtons: 0x%x", pad.holdButtons);
        //OSReport(" ---------\n"); 
    }

    void printGameInputs(const gfPadStatus& pad) {
        OSReport(" -- Pad --\n");
        OSReport(" LAnalogue: %u    RAnalogue %u\n", pad.LAnalogue, pad.RAnalogue);
        OSReport("StickX: %hhu ", pad.stickX);
        OSReport("StickY: %hhu ", pad.stickY);
        OSReport("CStickX: %hhu ", pad.cStickX);
        OSReport("CStickY: %hhu\n", pad.cStickY);
        OSReport("Buttons: ");
        OSReport("B1: 0x%x ", pad.buttons);
        OSReport("B2: 0x%x ", pad._buttons);
        OSReport("B3: 0x%x \n", pad.newPressedButtons);
        //OSReport(" ---------\n"); 
    }

    void printFrameData(const FrameData& fd) {
        for (int i = 1; i < MAX_NUM_PLAYERS; i++) {
            OSReport("Frame %u pIdx %u\n", fd.playerFrameDatas[i].frame, (unsigned int)fd.playerFrameDatas[i].playerIdx);
            printInputs(fd.playerFrameDatas[i].pad);
        }
    }

    void SyncLog(const BrawlbackPad& pad, bu8 playerIdx) {
        OSReport("[Sync] Injecting inputs for player %u on frame %u\n", (unsigned int)playerIdx, getCurrentFrame());
        printInputs(pad);
        OSReport("[/Sync]\n");
    }

    void FixFrameDataEndianness(FrameData* fd) {
        utils::swapByteOrder(fd->randomSeed);
        for (int i = 0; i < MAX_NUM_PLAYERS; i++) {
            utils::swapByteOrder(fd->playerFrameDatas[i].frame);
            utils::swapByteOrder(fd->playerFrameDatas[i].syncData.anim);
            utils::swapByteOrder(fd->playerFrameDatas[i].syncData.locX);
            utils::swapByteOrder(fd->playerFrameDatas[i].syncData.locY);
            utils::swapByteOrder(fd->playerFrameDatas[i].syncData.percent);
            utils::swapByteOrder(fd->playerFrameDatas[i].pad._buttons);
            utils::swapByteOrder(fd->playerFrameDatas[i].pad.buttons);
            utils::swapByteOrder(fd->playerFrameDatas[i].pad.holdButtons);
            utils::swapByteOrder(fd->playerFrameDatas[i].pad.releasedButtons);
            utils::swapByteOrder(fd->playerFrameDatas[i].pad.rapidFireButtons);
            utils::swapByteOrder(fd->playerFrameDatas[i].pad.newPressedButtons);
            utils::swapByteOrder(fd->playerFrameDatas[i].sysPad._buttons);
            utils::swapByteOrder(fd->playerFrameDatas[i].sysPad.buttons);
            utils::swapByteOrder(fd->playerFrameDatas[i].sysPad.holdButtons);
            utils::swapByteOrder(fd->playerFrameDatas[i].sysPad.releasedButtons);
            utils::swapByteOrder(fd->playerFrameDatas[i].sysPad.rapidFireButtons);
            utils::swapByteOrder(fd->playerFrameDatas[i].sysPad.newPressedButtons);
        }
    }
    

    // TODO: fix pause by making sure that the sys data thingy is also checking for one of the other button bits

    BrawlbackPad GamePadToBrawlbackPad(const gfPadStatus& pad) {
        BrawlbackPad ret = BrawlbackPad();
        ret._buttons = pad._buttons;
        ret.buttons = pad.buttons;
        // *(ret.newPressedButtons-0x2) = (int)*(pad+0x14);
        ret.holdButtons = pad.holdButtons;
        ret.rapidFireButtons = pad.rapidFireButtons;
        ret.releasedButtons = pad.releasedButtons;
        ret.newPressedButtons = pad.newPressedButtons;
        ret.LAnalogue = pad.LAnalogue;
        ret.RAnalogue = pad.RAnalogue;
        ret.cStickX = pad.cStickX;
        ret.cStickY = pad.cStickY;
        ret.stickX = pad.stickX;
        ret.stickY = pad.stickY;

        // OSReport("BUTTONS======\n");
        // OSReport("Buttons: 0x%x\n", pad._buttons);
        // OSReport("Buttons: 0x%x\n", pad.buttons);
        // OSReport("Buttons holdButtons: 0x%x\n", pad.holdButtons);
        // OSReport("Buttons rapidFireButtons: 0x%x\n", pad.rapidFireButtons);
        // OSReport("Buttons releasedButtons: 0x%x\n", pad.releasedButtons);
        // OSReport("Buttons newPressedButtons: 0x%x\n", pad.newPressedButtons);

        return ret;
    }

    void PopulatePlayerFrameData(PlayerFrameData& pfd, bu8 port, bu8 pIdx) {
        if(gameHasStarted())
        {
            ftManager* fighterManager = g_ftManager;
            Fighter* fighter = fighterManager->getFighter(fighterManager->getEntryIdFromIndex(pIdx));
            ftOwner* ftowner = fighterManager->getOwner(fighterManager->getEntryIdFromIndex(pIdx));
            
            pfd.syncData.facingDir = fighter->m_moduleAccesser->getPostureModule()->getLr() < 0.0 ? -1 : 1;
            pfd.syncData.locX = fighter->m_moduleAccesser->getPostureModule()->getPos().m_x;
            pfd.syncData.locY = fighter->m_moduleAccesser->getPostureModule()->getPos().m_y;
            pfd.syncData.anim = fighter->m_moduleAccesser->getStatusModule()->getStatusKind();
            
            pfd.syncData.percent = (float)ftowner->getDamage();
            pfd.syncData.stocks = (bu8)ftowner->getStockCount();
        }
        
        pfd.pad = Util::GamePadToBrawlbackPad(g_PadSystem.gcPads[port]);
        pfd.sysPad = Util::GamePadToBrawlbackPad(g_PadSystem.gcSysPads[port]);
    }
    void InjectBrawlbackPadToPadStatus(gfPadStatus& gamePad, const BrawlbackPad& pad, int port) {
        
        // TODO: do this once on match start or whatever, so we don't need to access this so often and lose cpu cycles
        //bool isNotConnected = Netplay::getGameSettings().playerSettings[port].playerType == PlayerType::PLAYERTYPE_NONE;
        // get current char selection and if none, the set as not connected
        bu8 charId = g_globalMelee.m_playersInitData[port].m_slotID;
        // bu8 charId = GM_GLOBAL_MODE_MELEE->playerData[port].charId;
        bool isNotConnected = charId == -1;
        // GM_GLOBAL_MODE_MELEE->playerData[port].playerType = isNotConnected ? 03 : 0 ; // Set to Human
        

        gamePad.isNotConnected = isNotConnected;
        gamePad._buttons = pad._buttons;
        gamePad.buttons = pad.buttons;
        // int* addr  = (int*) &gamePad;
        // *(addr+0x14+0x2) = pad.buttons;
        gamePad.holdButtons = pad.holdButtons;
        gamePad.rapidFireButtons = pad.rapidFireButtons;
        gamePad.releasedButtons = pad.releasedButtons;
        gamePad.newPressedButtons = pad.newPressedButtons;
        gamePad.LAnalogue = pad.LAnalogue;
        gamePad.RAnalogue = pad.RAnalogue;
        gamePad.cStickX = pad.cStickX;
        gamePad.cStickY = pad.cStickY;
        gamePad.stickX = pad.stickX;
        gamePad.stickY = pad.stickY;
        // OSReport("Port 0x%x Inputs\n", port);
        // OSReport("Buttons: 0x%x\n", pad.buttons);
        // OSReport("Buttons: 0x%x\n", pad.newPressedButtons);

    }

    void SaveState(bu32 currentFrame) {
        EXIPacket::CreateAndSend(EXICommand::CMD_CAPTURE_SAVESTATE, &currentFrame, sizeof(currentFrame));
    }
}

namespace Match {
    const char* relevantHeaps = "System WiiPad IteamResource InfoResource CommonResource CopyFB Physics ItemInstance Fighter1Resoruce Fighter2Resoruce Fighter1Resoruce2 Fighter2Resoruce2 Fighter1Instance Fighter2Instance FighterTechqniq InfoInstance InfoExtraResource GameGlobal FighterKirbyResource1 GlobalMode OverlayCommon Tmp OverlayStage ItemExtraResource FighterKirbyResource2 FighterKirbyResource3";
    void PopulateGameReport(GameReport& report)
    {
        ftManager* fighterManager = g_ftManager;

        for (int i = 0; i < Netplay::getGameSettings().numPlayers; i++) {
            Fighter* const fighter = fighterManager->getFighter(fighterManager->getEntryIdFromIndex(i));
            s32 stocks = fighter->getOwner()->getStockCount();
            OSReport("Stock count player idx %i = %i\n", i, stocks);
            report.stocks[i] = stocks;
            f64 damage = fighter->getOwner()->getDamage();
            OSReport("Damage for player idx %i = %f\n", i, damage);
            report.damage[i] = damage;
        }
        report.frame_duration = getCurrentFrame();
    }
    void SendGameReport(GameReport& report)
    {
        EXIPacket::CreateAndSend(EXICommand::CMD_MATCH_END, &report, sizeof(report));
    }
    void StopGameScMeleeHook()
    {
        utils::SaveRegs();
        OSReport("Game report in stopGameScMeleeBeginningHook hook\n");
        if (Netplay::getGameSettings().numPlayers > 1) {
            #if 0  // toggle for sending end match game stats
            GameReport report;
            PopulateGameReport(report);
            SendGameReport(report);
            #endif
        }
        utils::RestoreRegs();
    }
    void StartSceneMelee()
    {
        utils::SaveRegs();
        OSDisableInterrupts();
        //OSReport("  ~~~~~~~~~~~~~~~~  Start Scene Melee  ~~~~~~~~~~~~~~~~  \n");
        #ifdef NETPLAY_IMPL
        //Netplay::StartMatching(); // now moved to m_modeMelee.cpp
        Netplay::SetIsInMatch(true);
        g_mtRandDefault.seed = 0x496ffd00;
        g_mtRandOther.seed = 0x496ffd00;
        #else
        // 'debug' values
        Netplay::getGameSettings().localPlayerIdx = 0;
        Netplay::localPlayerIdx = 0;
        Netplay::getGameSettings().numPlayers = 2;
        #endif
        OSEnableInterrupts();
        utils::RestoreRegs();
    }
    void ExitSceneMelee()
    {
        utils::SaveRegs();
        OSDisableInterrupts();
        OSReport("  ~~~~~~~~~~~~~~~~  Exit Scene Melee  ~~~~~~~~~~~~~~~~  \n");
        frameCounter = 0;
        #ifdef NETPLAY_IMPL
        Netplay::EndMatch();
        Netplay::SetIsInMatch(false);
        #endif
        OSEnableInterrupts();
        utils::RestoreRegs();
    }
    void setRandSeed()
    {
        utils::SaveRegs();
        if(Netplay::IsInMatch())
        {
            g_mtRandDefault.seed = 0x496ffd00;
            g_mtRandOther.seed = 0x496ffd00;
        }
        utils::RestoreRegs();
    }
    void dump_gfMemoryPool_hook()
    {
        utils::SaveRegs();
        char** r30_reg_val;
        bu32 addr_start;
        bu32 addr_end;
        bu32 mem_size;
        bu8 id;
        asm volatile(
            "mr %0, r30\n\t"
            "mr %1, r4\n\t"
            "mr %2, r5\n\t"
            "mr %3, r6\n\t"
            "mr %4, r7\n\t"
            : "=r"(r30_reg_val), "=r"(addr_start), "=r"(addr_end), "=r"(mem_size), "=r"(id)
        );
        DumpGfMemoryPoolHook(r30_reg_val, addr_start, addr_end, mem_size, id);
        utils::RestoreRegs();
    }
    void DumpGfMemoryPoolHook(char** r30_reg_val, bu32 addr_start, bu32 addr_end, bu32 mem_size, u8 id)
    {
        char* heap_name = *r30_reg_val;
        if(strstr(relevantHeaps, heap_name) != (char*)0x0) {
            SavestateMemRegionInfo memRegion;
            memRegion.address = addr_start; // might be bad cast... 64 bit ptr to 32 bit int
            memRegion.size = mem_size;
            memmove(memRegion.nameBuffer, heap_name, strlen(heap_name));
            memRegion.nameBuffer[strlen(heap_name)] = '\0';
            memRegion.nameSize = strlen(heap_name);
            EXIPacket::CreateAndSend(EXICommand::CMD_SEND_DUMPALL, &memRegion, sizeof(SavestateMemRegionInfo));
        }
    }
    void ProcessGameAllocation(u8* allocated_addr, bu32 size, char* heap_name)
    {
        if (shouldTrackAllocs && strstr(relevantHeaps, heap_name) != (char*)0x0 && !isRollback) {
            //OSReport("ALLOC: size = 0x%08x  allocated addr = 0x%08x\n", size, allocated_addr);
            SavestateMemRegionInfo memRegion;
            memRegion.address = reinterpret_cast<bu32>(allocated_addr); // might be bad cast... 64 bit ptr to 32 bit int
            memRegion.size = size;
            memmove(memRegion.nameBuffer, heap_name, strlen(heap_name));
            memRegion.nameBuffer[strlen(heap_name)] = '\0';
            memRegion.nameSize = strlen(heap_name);
            EXIPacket::CreateAndSend(EXICommand::CMD_SEND_ALLOCS, &memRegion, sizeof(memRegion));
        }
    }
    void ProcessGameFree(u8* address, char* heap_name)
    {
        if (shouldTrackAllocs && strstr(relevantHeaps, heap_name) != (char*)0x0 && !isRollback) {
            //OSReport("FREE: addr = 0x%08x\n", address);
            SavestateMemRegionInfo memRegion;
            memmove(memRegion.nameBuffer, heap_name, strlen(heap_name));
            memRegion.nameBuffer[strlen(heap_name)] = '\0';
            memRegion.nameSize = strlen(heap_name);
            memRegion.address = reinterpret_cast<bu32>(address);
            EXIPacket::CreateAndSend(EXICommand::CMD_SEND_DEALLOCS, &memRegion, sizeof(memRegion));
        }
    }
    bu32 allocSizeTracker = 0;
    char allocHeapName[30];
    void alloc_gfMemoryPool_hook()
    {
        utils::SaveRegs();
        char** internal_heap_data;
        bu32 size;
        bu32 alignment;
        asm(
            "mr %0, r3\n\t"
            "mr %1, r4\n\t"
            "mr %2, r5\n\t" 
            : "=r"(internal_heap_data), "=r"(size), "=r"(alignment)
        );
        AllocGfMemoryPoolBeginHook(internal_heap_data, size, alignment);
        utils::RestoreRegs();
    }
    void AllocGfMemoryPoolBeginHook(char** internal_heap_data, bu32 size, bu32 alignment)
    {
        char* heap_name = *internal_heap_data;
        memmove(allocHeapName, heap_name, strlen(heap_name));
        allocHeapName[strlen(heap_name)] = '\0';
        allocSizeTracker = size;
    }
    void allocGfMemoryPoolEndHook()
    {
        utils::SaveRegs();
        u8* alloc_addr;
        asm volatile(
            "mr %0, r30\n\t"
            : "=r"(alloc_addr)
        );
        ProcessGameAllocation(alloc_addr, allocSizeTracker, allocHeapName);
        utils::RestoreRegs();
    }
    void free_gfMemoryPool_hook()
    {
        utils::SaveRegs();
        char** internal_heap_data;
        u8* address;
        asm volatile(
            "mr %0, r3\n\t"
            "mr %1, r4\n\t"
            : "=r"(internal_heap_data), "=r"(address)
        );
        FreeGfMemoryPoolHook(internal_heap_data, address);
        utils::RestoreRegs();
    }
    void FreeGfMemoryPoolHook(char** internal_heap_data, bu8* address)
    {
        char* heap_name = *internal_heap_data;
        ProcessGameFree(address, heap_name);
    }
}

namespace FrameAdvance {
    bu32 framesToAdvance = 1;
    FrameData currentFrameData = FrameData();
    bu32 getFramesToAdvance() 
    { 
        return framesToAdvance; 
    }

    void TriggerFastForwardState(bu8 numFramesToFF) 
    {
        if (framesToAdvance == 1 && numFramesToFF > 0) {
            framesToAdvance = numFramesToFF;
        }
    }
    void StallOneFrame() 
    { 
        if (framesToAdvance == 1) {
            framesToAdvance = 0; 
        }
    }

    void ResetFrameAdvance() 
    { 
        //OSReport("Resetting frameadvance to normal\n");
        framesToAdvance = 1;
        isRollback = false;
    }


    // requests FrameData for specified frame from emulator and assigns it to inputs
    void GetInputsForFrame(bu32 frame, FrameData* inputs) 
    {
        EXIPacket::CreateAndSend(EXICommand::CMD_FRAMEDATA, &frame, sizeof(frame));
        EXIHooks::readEXI(inputs, sizeof(FrameData), EXI_CHAN_1, 0, EXI_FREQ_32HZ);
        Util::FixFrameDataEndianness(inputs);
    }

    // should be called on every simulation frame
    void ProcessGameSimulationFrame(FrameData* inputs) 
    {
        bu32 gameLogicFrame = getCurrentFrame();
        //OSReport("ProcessGameSimulationFrame %u \n", gameLogicFrame);
        
        // save state on each simulated frame (this includes resim frames)
        if(shouldTrackAllocs)
        {
            Util::SaveState(gameLogicFrame);

            GetInputsForFrame(gameLogicFrame, inputs);
        }

        //OSReport("Using inputs %u %u  game frame: %u\n", inputs->playerFrameDatas[0].frame, inputs->playerFrameDatas[1].frame, gameLogicFrame);
        
        //Util::printFrameData(*inputs);
    }

    void updateIpSwitchPreProcess() 
    {
        utils::SaveRegs();
        if (Netplay::IsInMatch()) {
            ProcessGameSimulationFrame(&currentFrameData);
        }
        utils::RestoreRegs();
    }
    void updateLowHook() 
    {
        utils::SaveRegs();
        gfPadSystem* padSystem;
        bu32 padStatus;
        asm volatile(
            "mr %0, r25\n\t"
            "mr %1, r26\n\t"
            : "=r"(padSystem), "=r"(padStatus)
        );
        getGamePadStatusInjection(padSystem, padStatus);
        utils::RestoreRegs();
        asm volatile {
            lwz	r0, 0x0014 (sp)
            mtlr r0
            addi sp, sp, 16
            lwz	r4, -0x4390 (r13)
            lis r12, 0x8002
            ori r12, r12, 0x946C
            mtctr r12
            bctr
        }
    }
    void getGamePadStatusInjection(gfPadSystem* padSystem, bu32 padStatus) 
    {
        // OSReport("PAD %i 0x%x\n", 0, &PAD_SYSTEM->sysPads[0]);
        // OSReport("PAD %i 0x%x\n", 1, &PAD_SYSTEM->sysPads[1]);
        // OSReport("PAD %i 0x%x\n", 2, &PAD_SYSTEM->sysPads[2]);
        // OSReport("PAD %i 0x%x\n", 3, &PAD_SYSTEM->sysPads[3]);
        //Util::printGameInputs(PAD_SYSTEM->sysPads[0]);
        //  if((ddst->releasedButtons + ddst->newPressedButtons) != 0){
        //         OSReport("LOCAL BUTTONS(P%i)===GP(%i)===\n", port, isGamePad);
        //         OSReport("releasedButtons 0x%x\n", ddst->releasedButtons);
        //         OSReport("newPressedButtons 0x%x\n", ddst->newPressedButtons);
        // }

        // if((ddst->buttons) != 0){
        //     // OSReport("LOCAL BUTTONS(P%i)===GP(%i)===\n", port, isGamePad);
        //     // OSReport("buttons 0x%x\n", ddst->buttons);
        //     OSReport("Melee Info=====\n");
        //     OSReport("p1 charId=0x%x ptype=0x%x unk1=0x%x unk2=0x%x\n", GM_GLOBAL_MODE_MELEE->playerData[0].charId, GM_GLOBAL_MODE_MELEE->playerData[0].playerType, GM_GLOBAL_MODE_MELEE->playerData[0].unk1, GM_GLOBAL_MODE_MELEE->playerData[0].unk2);
        //     OSReport("p2 charId=0x%x ptype=0x%x unk1=0x%x unk2=0x%x\n", GM_GLOBAL_MODE_MELEE->playerData[1].charId, GM_GLOBAL_MODE_MELEE->playerData[1].playerType, GM_GLOBAL_MODE_MELEE->playerData[1].unk1, GM_GLOBAL_MODE_MELEE->playerData[1].unk2);
        //     OSReport("p3 charId=0x%x ptype=0x%x unk1=0x%x unk2=0x%x\n", GM_GLOBAL_MODE_MELEE->playerData[2].charId, GM_GLOBAL_MODE_MELEE->playerData[2].playerType, GM_GLOBAL_MODE_MELEE->playerData[2].unk1, GM_GLOBAL_MODE_MELEE->playerData[2].unk2);
        //     OSReport("p4 charId=0x%x ptype=0x%x unk1=0x%x unk2=0x%x\n", GM_GLOBAL_MODE_MELEE->playerData[3].charId, GM_GLOBAL_MODE_MELEE->playerData[3].playerType, GM_GLOBAL_MODE_MELEE->playerData[3].unk1, GM_GLOBAL_MODE_MELEE->playerData[3].unk2);

        // }
        if (Netplay::IsInMatch()) {
            //OSReport("Injecting pad for- frame %u port %i\n", currentFrameData.playerFrameDatas[port].frame, port);
            
            FrameLogic::FrameDataLogic(getCurrentFrame());
            for(int i  = 0; i < MAX_NUM_PLAYERS; i++)
            {
                gfPadStatus* gamePad = reinterpret_cast<gfPadStatus*>(padStatus + 0x40 * i);
                PlayerFrameData frameData = currentFrameData.playerFrameDatas[i];
                BrawlbackPad pad = frameData.pad;
                Util::InjectBrawlbackPadToPadStatus(*gamePad, pad, i);
                // if(ddst->newPressedButtons == 0x1000){
                //     bp();
                // }

                // TODO: make whole game struct be filled in from dolphin based off a known good match between two characters on a stage like battlefield
                if((gamePad->releasedButtons + gamePad->newPressedButtons) != 0){
                    //OSReport("BUTTONS(P%i)===GP(%i)===\n", i, true);
                    //OSReport("releasedButtons 0x%x\n", gamePad->releasedButtons);
                    //OSReport("newPressedButtons 0x%x\n", gamePad->newPressedButtons);
                }
            }

            // if((ddst->buttons) != 0){
                // OSReport("BUTTONS(P%i)===GP(%i)===\n", port, isGamePad);
                // OSReport("buttons 0x%x\n", ddst->buttons);
            // }

        }
    }
    void handleFrameAdvanceHook() {
        utils::SaveRegs();
        setFrameAdvanceFromEmu();
        asm volatile("cmplw r19, %0\n\t"
            :
            : "r" (framesToAdvance)
        );
        utils::RestoreRegs();
    }
    void setFrameAdvanceFromEmu() {
        EXIPacket::CreateAndSend(EXICommand::CMD_FRAMEADVANCE);
        EXIHooks::readEXI(&framesToAdvance, sizeof(bu32), EXI_CHAN_1, 0, EXI_FREQ_32HZ);
        utils::swapByteOrder(framesToAdvance);
        if(framesToAdvance > 1)
        {
            isRollback = true;
        }
    }
}

namespace FrameLogic {
    void WriteInputsForFrame(bu32 currentFrame)
    {
        bu8 localPlayerIdx = Netplay::localPlayerIdx;
        if (localPlayerIdx != Netplay::localPlayerIdxInvalid) {
            PlayerFrameData playerFrame = PlayerFrameData();
            playerFrame.frame = currentFrame;
            playerFrame.playerIdx = localPlayerIdx;
            Util::PopulatePlayerFrameData(playerFrame, Netplay::getGameSettings().localPlayerPort, localPlayerIdx);
            // sending inputs + current game frame
            EXIPacket::CreateAndSend(EXICommand::CMD_ONLINE_INPUTS, &playerFrame, sizeof(PlayerFrameData));
        }
        else {
            OSReport("Invalid player index! Can't send inputs to emulator!\n");
        }
    }
    void FrameDataLogic(bu32 currentFrame)
    {
        WriteInputsForFrame(currentFrame);
    }
    void SendFrameCounterPointerLoc()
    {
        bu32 frameCounterLocation = reinterpret_cast<bu32>(&frameCounter);
        EXIPacket::CreateAndSend(EXICommand::CMD_SEND_FRAMECOUNTERLOC, &frameCounterLocation, sizeof(bu32));
    }
    const char* nonResimTasks = "ecMgr EffectManager";
    bool ShouldSkipGfTaskProcess(bu32* gfTask, bu32 task_type)
    {
        if (isRollback) { // if we're resimulating, disable certain tasks that don't need to run on resim frames.
            char* taskName = (char*)(*gfTask); // 0x0 offset of gfTask* is the task name
            //OSReport("Processing task %s\n", taskName);
            return strstr(nonResimTasks, taskName) != (char*)0x0;
        }
        return false;
    }
    void initFrameCounter()
    {
        utils::SaveRegs();
        frameCounter = 0;
        utils::RestoreRegs();
    }
    void updateFrameCounter()
    {
        utils::SaveRegs();
        frameCounter++;
        utils::RestoreRegs();
    }
    void beginningOfMainGameLoop()
    {
        utils::SaveRegs();
        if (Netplay::IsInMatch()) {
            EXIPacket::CreateAndSend(EXICommand::CMD_TIMER_START);
        }
        utils::RestoreRegs();
    }
    void beginFrame()
    {
        utils::SaveRegs();
        bu32 currentFrame = getCurrentFrame();
        //Util::printGameInputs(PAD_SYSTEM->pads[0]);
        //Util::printGameInputs(PAD_SYSTEM->sysPads[0]);

        // this is the start of all our logic for each frame. Because EXI writes/reads are synchronous,
        // you can think of the control flow going like this
        // this function -> write data to emulator through exi -> emulator processes data and possibly queues up data
        // to send back to the game -> send data to the game if there is any -> game processes that data -> repeat
        if (Netplay::IsInMatch()) {
            // reset flag to be used later
            // just resimulated/stalled/skipped/whatever, reset to normal
            FrameAdvance::ResetFrameAdvance();
            // lol
            g_mtRandDefault.seed = 0x496ffd00;
            g_mtRandOther.seed = 0x496ffd00;
            OSReport("~~~~~~~~~~~~~~~~ FRAME %d ~~~~~~~~~~~~~~~~\n", currentFrame);
            #ifdef NETPLAY_IMPL
                FrameDataLogic(currentFrame);
                if(!shouldTrackAllocs)
                {
                    gfHeapManager::dumpAll();
                    shouldTrackAllocs = true;
                }
            #endif
        }
        
        utils::RestoreRegs();
    }
    void endFrame()
    {
        utils::SaveRegs();
        if (Netplay::IsInMatch()) {
            EXIPacket::CreateAndSend(EXICommand::CMD_TIMER_END);
        }
        utils::RestoreRegs();
    }
    void endMainLoop()
    {
        utils::SaveRegs();
        utils::RestoreRegs();
    }
    void gfTaskProcessHook()
    {
        utils::SaveRegs();
        register bu32* gfTask; 
        register bu32 task_type;
        asm (
            "mr gfTask, r3\n\t"
            "mr task_type, r4\n\t"
        );
        if(ShouldSkipGfTaskProcess(gfTask, task_type))
        {
            utils::RestoreRegs();
            asm volatile {
                lwz	r0, 0x0014 (sp)
                mtlr r0
                addi sp, sp, 16
                blr
            }
        }
        else {
            utils::RestoreRegs();
            asm volatile {
                lwz	r0, 0x0014 (sp)
                mtlr r0
                addi sp, sp, 16
                cmpwi r4, 0x8
                lis r12, 0x8002
                ori r12, r12, 0xdc78
                mtctr r12
                bctr
            }
        }
    }
}
u8 defaultGmGlobalModeMelee[0x320] = {0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2a, 0x81, 0x8, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0x78, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x70, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0xff, 0xff, 0xff, 0x7, 0x57, 0xff, 0xf1, 0xff, 0x87, 0xf1, 0xff, 0x0, 0x3, 0xf0, 0x2f, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x1, 0x0, 0x28, 0x1d, 0x2, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x15, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x78, 0x0, 0x0, 0x0, 0x10, 0x0, 0x0, 0x64, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x29, 0x0, 0x1, 0x0, 0x4, 0x0, 0x0, 0x2, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x78, 0x0, 0x0, 0x0, 0x10, 0x0, 0x0, 0x64, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3e, 0x3, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x78, 0x0, 0x0, 0x0, 0x10, 0x0, 0x0, 0x64, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3e, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x78, 0x0, 0x0, 0x0, 0x10, 0x0, 0x0, 0x64, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3e, 0x3, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x78, 0x0, 0x0, 0x0, 0x10, 0x0, 0x0, 0x15, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3e, 0x3, 0x5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x78, 0x0, 0x0, 0x0, 0x10, 0x0, 0x0, 0x15, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x5, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3e, 0x3, 0x6, 0x0, 0x0, 0x0, 0x0, 0x0, 0x6, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x78, 0x0, 0x0, 0x0, 0x10, 0x0, 0x0, 0x15, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x6, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
namespace GMMelee {
    bool isMatchChoicesPopulated = false;
    int charChoices[MAX_NUM_PLAYERS] = {-1, -1, -1, -1};
    int stageChoice = -1;
    void PopulateMatchSettings(int chars[MAX_NUM_PLAYERS], int stageID) 
    {
        for (int i = 0; i < MAX_NUM_PLAYERS; i++) {
            charChoices[i] = chars[i];
        }
        stageChoice = stageID;
        isMatchChoicesPopulated = true;
    }
    // called on match end
    void ResetMatchChoicesPopulated() 
    { 
        isMatchChoicesPopulated = false; 
    }

    void postSetupMelee() {
        utils::SaveRegs();
        OSDisableInterrupts();
        OSReport("postSetupMelee\n");

        #ifdef NETPLAY_IMPL
        Netplay::StartMatching(); // start netplay logic
        #endif

        // falco, wolf, battlefield
        //PopulateMatchSettings( {0x15, 0x29, -1, -1}, 0x1 );

        if (isMatchChoicesPopulated) {
            OSReport("postSetupMelee stage: 0x%x p1: 0x%x p2: 0x%x\n", stageChoice, charChoices[0], charChoices[1]);

            memmove(g_GameGlobal->m_modeMelee, defaultGmGlobalModeMelee, 0x320);
        
            g_GameGlobal->m_modeMelee->m_playersInitData[0].m_slotID = charChoices[0];
            g_GameGlobal->m_modeMelee->m_playersInitData[1].m_slotID = charChoices[1];
            
            g_GameGlobal->m_modeMelee->m_playersInitData[0].m_initState = 0;
            g_GameGlobal->m_modeMelee->m_playersInitData[1].m_initState = 0;

            g_GameGlobal->m_modeMelee->m_playersInitData[0].unk1 = 0x80;
            g_GameGlobal->m_modeMelee->m_playersInitData[1].unk1 = 0x80;

            g_GameGlobal->m_modeMelee->m_playersInitData[0].m_startPointIdx = 0;
            g_GameGlobal->m_modeMelee->m_playersInitData[1].m_startPointIdx = 1;

            // melee[P1_CHAR_ID_IDX+1] = 0; // Set player type to human
            // melee[P2_CHAR_ID_IDX+1] = 0;
            // melee[STAGE_ID_IDX] = stageChoice;
            g_GameGlobal->m_modeMelee->m_meleeInitData.m_stageID = 0x01; // TODO uncomment and use above line, just testing with battlefield
        }
        OSEnableInterrupts();
        utils::RestoreRegs();
    }
}

namespace Netplay {
    GameSettings gameSettings;
    const bu8 localPlayerIdxInvalid = 200;
    bu8 localPlayerIdx = localPlayerIdxInvalid;
    bool isInMatch = false;
    bool IsInMatch() 
    { 
        return isInMatch; 
    }
    void SetIsInMatch(bool isMatch) 
    {
        isInMatch = isMatch; 
    }

    GameSettings& getGameSettings() 
    { 
        return gameSettings;
    }

    void FixGameSettingsEndianness(GameSettings& settings) 
    {
        utils::swapByteOrder(settings.stageID);
        utils::swapByteOrder(settings.randomSeed);
        for(int i = 0; i < MAX_NUM_PLAYERS; i++)
        {
            for(int f = 0; f < NAMETAG_SIZE; f++)
            {
                utils::swapByteOrder(settings.playerSettings[i].nametag[f]);
            }
        }
    }

    void StartMatching() 
    {
        OSReport("Filling in game settings from game\n");
        // populate game settings
        fillOutGameSettings(gameSettings);

        // send our populated game settings to the emu
        EXIPacket::CreateAndSend(EXICommand::CMD_START_MATCH, &gameSettings, sizeof(GameSettings));

        // start emu netplay thread so it can start trying to find an opponent
        EXIPacket::CreateAndSend(EXICommand::CMD_FIND_OPPONENT);

        // Temporary. Atm, this just stalls main thread while we do our mm/connecting
        // in the future, when netmenu stuff is implemented, the organization of StartMatching and CheckIsMatched
        // will make more sense
        while (!CheckIsMatched()) {}

    }

    
    bool CheckIsMatched() {
        bool matched = false;
        bu8 cmd_byte = EXICommand::CMD_UNKNOWN;

        // cmd byte + game settings
        const size_t read_size = sizeof(GameSettings) + 1;
        bu8 read_data[read_size];

        // stall until we get game settings from opponent, then load those in and continue to boot up the match
        //while (cmd_byte != EXICommand::CMD_SETUP_PLAYERS) {
            EXIHooks::readEXI(read_data, read_size, EXI_CHAN_1, 0, EXI_FREQ_32HZ);
            cmd_byte = read_data[0];

            if (cmd_byte == EXICommand::CMD_SETUP_PLAYERS) {
                GameSettings gameSettingsFromOpponent;
                memmove(&gameSettingsFromOpponent, read_data + 1, sizeof(GameSettings));
                FixGameSettingsEndianness(gameSettingsFromOpponent);
                MergeGameSettingsIntoGame(gameSettingsFromOpponent);
                // TODO: we shoud assign the gameSettings var to the gameSettings from opponent since its merged with ours now.
                gameSettings.localPlayerPort = gameSettingsFromOpponent.localPlayerPort;
                matched = true;
            }
            else {
                //OSReport("Reading for setupplayers, didn't get it...\n");
            }
        //}
        return matched;
    }

    void EndMatch() {
        localPlayerIdx = localPlayerIdxInvalid;
        gameSettings = GameSettings();
        GMMelee::ResetMatchChoicesPopulated();
    }

}

namespace RollbackHooks {
    void InstallHooks() 
    {
        // Match Namespace
        SyringeCore::syInlineHook(0x806d4c10, reinterpret_cast<void*>(Match::StopGameScMeleeHook));
        SyringeCore::syInlineHook(0x806d176c, reinterpret_cast<void*>(Match::StartSceneMelee));
        SyringeCore::syInlineHook(0x806d4844, reinterpret_cast<void*>(Match::ExitSceneMelee));
        SyringeCore::syInlineHook(0x8003fac4, reinterpret_cast<void*>(Match::setRandSeed));
        SyringeCore::syInlineHook(0x80026258, reinterpret_cast<void*>(Match::dump_gfMemoryPool_hook));
        SyringeCore::syInlineHook(0x80025c6c, reinterpret_cast<void*>(Match::alloc_gfMemoryPool_hook));
        SyringeCore::syInlineHook(0x80025ec4, reinterpret_cast<void*>(Match::allocGfMemoryPoolEndHook));
        SyringeCore::syInlineHook(0x80025f40, reinterpret_cast<void*>(Match::free_gfMemoryPool_hook));

        // FrameAdvance Namespace
        SyringeCore::syInlineHook(0x8004aa2c, reinterpret_cast<void*>(FrameAdvance::updateIpSwitchPreProcess));
        SyringeCore::sySimpleHook(0x80029468, reinterpret_cast<void*>(FrameAdvance::updateLowHook));
        SyringeCore::syInlineHook(0x800173a4, reinterpret_cast<void*>(FrameAdvance::handleFrameAdvanceHook));

        // FrameLogic Namespace
        SyringeCore::sySimpleHook(0x8002dc74, reinterpret_cast<void*>(FrameLogic::gfTaskProcessHook));
        //SyringeCore::syHookFunction(0x800174fc, reinterpret_cast<void*>(FrameLogic::endMainLoop));
        SyringeCore::syInlineHook(0x801473a0, reinterpret_cast<void*>(FrameLogic::endFrame));
        SyringeCore::syInlineHook(0x800171b4, reinterpret_cast<void*>(FrameLogic::beginningOfMainGameLoop));
        SyringeCore::syInlineHook(0x80017760, reinterpret_cast<void*>(FrameLogic::updateFrameCounter));
        SyringeCore::syInlineHook(0x8004e884, reinterpret_cast<void*>(FrameLogic::initFrameCounter));
        SyringeCore::syInlineHook(0x80147394, reinterpret_cast<void*>(FrameLogic::beginFrame));

        // GMMelee Namespace
        SyringeCore::syInlineHook(0x806dd03c, reinterpret_cast<void*>(GMMelee::postSetupMelee));
    }
}