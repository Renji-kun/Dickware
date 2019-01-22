
#include "Misc.h"
#include "ConfigSystem.h"
#include "Settings.h"

void Misc::OnCreateMove(CUserCmd* cmd)
{
	pCmd = cmd;
    if (!g_LocalPlayer || !g_LocalPlayer->IsAlive())
    {
        return;
    }
    NoCrouchCooldown(cmd);
}

void Misc::OnFrameStageNotify(ClientFrameStage_t stage)
{
    switch (stage)
    {
        case FRAME_UNDEFINED:
            break;
        case FRAME_START:
            break;
        case FRAME_NET_UPDATE_START:
            break;
        case FRAME_NET_UPDATE_POSTDATAUPDATE_START:
            break;
        case FRAME_NET_UPDATE_POSTDATAUPDATE_END:
            break;
        case FRAME_NET_UPDATE_END:
            break;
        case FRAME_RENDER_START:
            NoFlash();
            break;
        case FRAME_RENDER_END:
            break;
    }
}

void Misc::ExecuteGameConfig(const std::string & config)
{
	g_EngineClient->ExecuteClientCmd(config.c_str());
}

void Misc::SetLocalPlayerReady()
{
	static auto SetLocalPlayerReadyFn = reinterpret_cast<bool(__stdcall*)(const char*)>(Utils::PatternScan(L"client_panorama.dll", "55 8B EC 83 E4 F8 8B 4D 08 BA ? ? ? ? E8 ? ? ? ? 85 C0 75 12"));
	if (SetLocalPlayerReadyFn)
		SetLocalPlayerReadyFn("deferred");
}

void Misc::NoCrouchCooldown(CUserCmd* cmd)
{
    //if (!g_Config.GetBool("misc_no_crouch_cooldown"))
	if(!Settings::Misc::NoCrouchCooldown)
    {
        return;
    }

    cmd->buttons |= IN_BULLRUSH;
}

void Misc::NoFlash()
{
    if (!g_LocalPlayer || !g_LocalPlayer->IsAlive() || !g_Config.GetBool("vis_misc_noflash"))
    {
        return;
    }

    //g_LocalPlayer->m_flFlashMaxAlpha() = 0.f;
    g_LocalPlayer->m_flFlashDuration() = 0.f;
}
