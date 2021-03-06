#include "hooks.hpp"
#include <intrin.h>
#include "options.hpp"
#include "render.hpp"
#include "menu_helpers.hpp"
#include "ConfigSystem.h"
#include "helpers/input.hpp"
#include "helpers/utils.hpp"
#include "features/bhop.hpp"
#include "features/visuals.hpp"
#include "features/glow.hpp"
#include "EnginePrediction.h"
#include "Chams.h"
#include "MovementFix.h"
#include "Rbot.h"
#include "KeyLoop.h"
#include "ThirdpersonAngleHelper.h"
#include "AntiAim.h"
#include "Fakelag.h"
#include "Resolver.h"
#include "BuyBot.h"
#include "HitPossitionHelper.h"
#include "RuntimeSaver.h"
#include "Logger.h"
#include "ClantagChanger.h"
#include "Backtrack.h"
#include "features/MaterialManager.hpp"
#include "Lbot.h"
#include "Misc.h"
#include "ConsoleHelper.h"
#include "Utils\ConvarSpoofer.h"
#include "features\TriggerBot.h"
#include "Settings.h"
#include "features\NightMode.h"
#include "features\Skinchanger.h"
#include "features\GrenadeTrajectory.h"
#include "features\Radar.h"
//#include "Asuswalls.h"
#include "NoSmoke.h"
#include "features\LagCompensation.h"
#include "features\EventLogger.h"
#include "ShadowVMTHook.h"
#pragma intrinsic(_ReturnAddress)

namespace Hooks
{
    vfunc_hook hlclient_hook;
    vfunc_hook direct3d_hook;
    vfunc_hook vguipanel_hook;
    vfunc_hook vguisurf_hook;
    vfunc_hook sound_hook;
    vfunc_hook mdlrender_hook;
    vfunc_hook clientmode_hook;
    vfunc_hook sv_cheats;
	recv_prop_hook* sequence_hook;
    vfunc_hook RenderView_hook;
    vfunc_hook ViewRender_hook;
    vfunc_hook gameevents_hook;
    vfunc_hook clientstate_hook;
	vfunc_hook firebullets_hook;
	vfunc_hook partition_hook;
	vfunc_hook engine_hook;

	TempEntities o_TempEntities = nullptr;
	IsHLTV o_IsHLTV = nullptr;
	float flAngle = 0.f;

	CUtlVector<SndInfo_t> sndList;

	// Fix this ghetto
	void* retAddr;

    void Initialize()
    {
        g_Logger.Clear();
        g_Logger.Info ( "cheat", "initializing cheat" );

        hlclient_hook.setup ( g_CHLClient, "client_panorama.dll" );
        direct3d_hook.setup ( g_D3DDevice9, "shaderapidx9.dll" );
        vguipanel_hook.setup ( g_VGuiPanel, "vgui2.dll" );
        vguisurf_hook.setup ( g_VGuiSurface, "vguimatsurface.dll" );
        sound_hook.setup ( g_EngineSound, "engine.dll" );
		engine_hook.setup(g_EngineClient, "engine.dll");
        //mdlrender_hook.setup ( g_MdlRender, "client_panorama.dll" );
		mdlrender_hook.setup(g_StudioRender);
        clientmode_hook.setup ( g_ClientMode, "client_panorama.dll" );
		partition_hook.setup(g_SpatialPartition);
        ConVar* sv_cheats_con = g_CVar->FindVar ( "sv_cheats" );
        sv_cheats.setup ( sv_cheats_con );
		auto dwFireBullets = *(DWORD**)(Utils::PatternScan(GetModuleHandleW(L"client_panorama.dll"), "55 8B EC 51 53 56 8B F1 BB ? ? ? ? B8") + 0x131);
        RenderView_hook.setup ( g_RenderView, "engine.dll" );
        gameevents_hook.setup ( g_GameEvents, "engine.dll" );
        ViewRender_hook.setup ( g_ViewRender, "client_panorama.dll" );
		firebullets_hook.setup(dwFireBullets, "client_panorama.dll");
		sequence_hook = new recv_prop_hook(C_BaseViewModel::m_nSequence(), hkRecvProxy);

        direct3d_hook.hook_index ( index::EndScene, hkEndScene );
        direct3d_hook.hook_index ( index::Reset, hkReset );

        hlclient_hook.hook_index ( index::FrameStageNotify, hkFrameStageNotify );
        hlclient_hook.hook_index ( index::CreateMove, hkCreateMove_Proxy );
		//partition_hook.hook_index(index::SuppressLists, hkSuppressLists);

        vguipanel_hook.hook_index ( index::PaintTraverse, hkPaintTraverse );

        sound_hook.hook_index ( index::EmitSound1, hkEmitSound1 );
        vguisurf_hook.hook_index ( index::LockCursor, hkLockCursor );

        mdlrender_hook.hook_index ( index::DrawModelExecute, hkDrawModelExecute );

        clientmode_hook.hook_index ( index::DoPostScreenSpaceEffects, hkDoPostScreenEffects );
        clientmode_hook.hook_index ( index::OverrideView, hkOverrideView );
		clientmode_hook.hook_index(index::ShouldDrawFog, hkShouldDrawFog);
		engine_hook.hook_index(index::IsHltv, hkIsHLTV);

        sv_cheats.hook_index ( index::SvCheatsGetBool, hkSvCheatsGetBool );
		firebullets_hook.hook_index(index::FireBullets, hkTEFireBulletsPostDataUpdate);
        RenderView_hook.hook_index ( index::SceneEnd, hkSceneEnd );
        ViewRender_hook.hook_index ( index::SmokeOverlay, Hooked_RenderSmokeOverlay );
        gameevents_hook.hook_index ( index::FireEvent, hkFireEvent );
		//hlclient_hook.hook_index(index::WriteUsercmdDeltaToBuffer, hkWriteUsercmdDeltaToBuffer);

		o_IsHLTV = engine_hook.get_original<IsHLTV>(index::IsHltv);

		retAddr = Utils::PatternScan(GetModuleHandle(L"client_panorama.dll"), "84 C0 75 38 8B 0D ? ? ? ? 8B 01 8B 80 ? ? ? ? FF D0");

        g_Logger.Success ( "cheat", "cheat initialized" );

        #ifdef _DEBUG
        NetvarSys::Get().Dump();
        #endif // _DEBUG

        g_CVar->FindVar ( "cl_interpolate" )->SetValue ( 1 ); //0
        g_CVar->FindVar ( "sv_showanimstate" )->SetValue ( 1 );
        g_CVar->FindVar ( "developer" )->SetValue ( 0 );
        g_CVar->FindVar ( "cl_interp_ratio" )->SetValue ( 2 );
        //g_CVar->FindVar("con_filter_text_out")->SetValue("");
        //g_CVar->FindVar("con_filter_enable")->SetValue(2);
        //g_CVar->FindVar("con_filter_text")->SetValue(".     ");
        //g_CVar->FindVar("con_notifytime")->SetValue(3);

        //g_CVar->FindVar("cl_interp_ratio")->SetValue(1.0f);
        //g_CVar->FindVar("cl_smooth")->SetValue(0.0f);
        //g_CVar->FindVar("cl_smoothtime")->SetValue(0.01f);
        //g_CVar->FindVar("cl_lagcompensation")->SetValue(1);//cl_sv_lagcompensateself->SetValue(1);
    }
    //--------------------------------------------------------------------------------
    void Shutdown()
    {
        //clientstate_hook.unhook_all();

        Glow::Get().Shutdown();
		//g_Saver.MediaPlayer.Release();
        hlclient_hook.unhook_all();
        direct3d_hook.unhook_all();
        vguipanel_hook.unhook_all();
        vguisurf_hook.unhook_all();
        sound_hook.unhook_all();
        mdlrender_hook.unhook_all();
        clientmode_hook.unhook_all();
        //sv_cheats.unhook_all();
        RenderView_hook.unhook_all();
        ViewRender_hook.unhook_all();
        gameevents_hook.unhook_all();
		clientstate_hook.unhook_all();
		sequence_hook->~recv_prop_hook();

    }

	void UpdateSpoofer()
	{
		if (!g_EngineClient->IsConnected() || !g_EngineClient->IsInGame())
		{
			if (g_CVarSpoofer->Ready())
				g_CVarSpoofer->Release();

			return;
		}

		if (!g_CVarSpoofer->Ready()) 
		{
			g_CVarSpoofer->Init();

			g_CVarSpoofer->Add("sv_cheats");
			g_CVarSpoofer->Add("r_drawspecificstaticprop");

			g_CVarSpoofer->Spoof();
		}

		if (g_CVarSpoofer->Ready())
			g_CVarSpoofer->Get("sv_cheats")->SetInt(1);
	}
    //--------------------------------------------------------------------------------
    bool HookedNetchan = false;
    long __stdcall hkEndScene ( IDirect3DDevice9* pDevice )
    {
        auto oEndScene = direct3d_hook.get_original<EndScene> ( index::EndScene );

        //if (g_Unload) clientstate_hook.unhook_all();

        if ( g_Unload )
            oEndScene ( pDevice );

        /*
        DWORD NetChannel = *(DWORD*)(*(DWORD*)g_ClientState + 0x9C);
        if (!g_EngineClient->IsInGame() || !g_EngineClient->IsConnected() || !NetChannel)
        {
        	if(HookedNetchan) clientstate_hook.unhook_all();
        	HookedNetchan = false;
        }
        */

        static auto viewmodel_fov = g_CVar->FindVar ( "viewmodel_fov" );
        static auto mat_ambient_light_r = g_CVar->FindVar ( "mat_ambient_light_r" );
        static auto mat_ambient_light_g = g_CVar->FindVar ( "mat_ambient_light_g" );
        static auto mat_ambient_light_b = g_CVar->FindVar ( "mat_ambient_light_b" );
        static auto crosshair_cvar = g_CVar->FindVar ( "crosshair" );
        static auto phys_pushscale_cvar = g_CVar->FindVar ( "phys_pushscale" );
        static auto phys_pushscale_org = phys_pushscale_cvar->Get<int>();
        static auto engine_no_focus_sleep_cvar = g_CVar->FindVar ( "engine_no_focus_sleep" );
		static auto cl_csm_enabled = g_CVar->FindVar("cl_csm_enabled");
		static auto weapon_debug_spread_show = g_CVar->FindVar("weapon_debug_spread_show");
        engine_no_focus_sleep_cvar->SetValue ( 0 );
        //static auto cl_extrapolate_cvar = g_CVar->FindVar("cl_extrapolate");//->SetValue(0);
        //cl_extrapolate_cvar->m_fnChangeCallbacks.m_Size = 0;
        //cl_extrapolate_cvar->SetValue(0);
        //static auto cl_interp_ratio_cvar = g_CVar->FindVar("cl_interp_ratio");
        //cl_interp_ratio
        //cl_interp
        //static auto cl_lagcompensation_cvar = g_CVar->FindVar("cl_lagcompensation");cl_interpolation
        //static auto cl_interp_cvar = g_CVar->FindVar("cl_interp");
        //static auto cl_sv_lagcompensateself = g_CVar->FindVar("sv_lagcompensateself");

        //cl_interp_ratio_cvar->SetValue(0);

        //AntiAim::Get().ResetLbyPrediction();

        //test
        //cl_sv_lagcompensateself->SetValue(1);

        /*
        static bool setup = false;
        if (!setup)
        {
        	setup = true;
        	g_CVar->FindVar("cl_interp")->SetValue(0.01f);
        	g_CVar->FindVar("cl_cmdrate")->SetValue(66);
        	g_CVar->FindVar("cl_updaterate")->SetValue(66);
        	g_CVar->FindVar("cl_interp_all")->SetValue(0.0f);
        	g_CVar->FindVar("cl_interp_ratio")->SetValue(1.0f);
        	g_CVar->FindVar("cl_smooth")->SetValue(0.0f);
        	g_CVar->FindVar("cl_smoothtime")->SetValue(0.01f);
        }
        */
		
		
		// FPS Optimization
		static bool activeLow = false;
		
		if (Settings::Visual::DisablePP && !activeLow)
		{
			cl_csm_enabled->m_nFlags &= ~FCVAR_CHEAT;
			cl_csm_enabled->SetValue(0);
			activeLow = true;
		}
		if (!Settings::Visual::DisablePP && activeLow)
		{
			cl_csm_enabled->m_nFlags &= ~FCVAR_CHEAT;
			cl_csm_enabled->SetValue(1);
			activeLow = false;
		}


		static bool activeSniper = false;
		if (Settings::Visual::SniperCrosshair && !activeSniper)
		{
			weapon_debug_spread_show->m_nFlags &= ~FCVAR_CHEAT;
			weapon_debug_spread_show->SetValue(2);
			activeSniper = true;
		}
		if (!Settings::Visual::SniperCrosshair && activeSniper)
		{
			weapon_debug_spread_show->m_nFlags &= ~FCVAR_CHEAT;
			weapon_debug_spread_show->SetValue(0);
			activeSniper = false;
		}
			

        #ifdef _DEBUG
        phys_pushscale_cvar->m_fnChangeCallbacks.m_Size = 0;
        phys_pushscale_cvar->SetValue ( phys_pushscale_org + ( 250 * g_Config.GetInt ( "vis_misc_addforce" ) ) );
        #endif // _DEBUG
        viewmodel_fov->m_fnChangeCallbacks.m_Size = 0;
        //viewmodel_fov->SetValue ( g_Config.GetInt ( "viewmodel_fov" ) ); //phys_pushscale
		viewmodel_fov->SetValue(Settings::Visual::ViewModelFOV); //phys_pushscale
        mat_ambient_light_r->SetValue ( g_Config.GetFloat ( "mat_ambient_light_r" ) );
        mat_ambient_light_g->SetValue ( g_Config.GetFloat ( "mat_ambient_light_g" ) );
        mat_ambient_light_b->SetValue ( g_Config.GetFloat ( "mat_ambient_light_b" ) );
        crosshair_cvar->SetValue ( !g_Config.GetBool ( "esp_crosshair" ) );
        //cl_interpolate_cvar->SetValue(1);
        //cl_lagcompensation_cvar->SetValue(1);
        //cl_interp_cvar->SetValue(1);

        DWORD colorwrite, srgbwrite;
        pDevice->GetRenderState ( D3DRS_COLORWRITEENABLE, &colorwrite );
        pDevice->GetRenderState ( D3DRS_SRGBWRITEENABLE, &srgbwrite );

        pDevice->SetRenderState ( D3DRS_COLORWRITEENABLE, 0xffffffff );
        //removes the source engine color correction
        pDevice->SetRenderState ( D3DRS_SRGBWRITEENABLE, false );

        pDevice->SetSamplerState ( NULL, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
        pDevice->SetSamplerState ( NULL, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
        pDevice->SetSamplerState ( NULL, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP );
        pDevice->SetSamplerState ( NULL, D3DSAMP_SRGBTEXTURE, NULL );

        ImGui_ImplDX9_NewFrame();

        auto esp_drawlist = Render::Get().RenderScene();

        MenuHelper::Get().Render();
		// Add radar render
		Radar::Get().Render();

        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData ( ImGui::GetDrawData(), esp_drawlist );

        pDevice->SetRenderState ( D3DRS_COLORWRITEENABLE, colorwrite );
        pDevice->SetRenderState ( D3DRS_SRGBWRITEENABLE, srgbwrite );

        return oEndScene ( pDevice );
    }
    //--------------------------------------------------------------------------------
    long __stdcall hkReset ( IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pPresentationParameters )
    {
        auto oReset = direct3d_hook.get_original<Reset> ( index::Reset );

        MenuHelper::Get().OnDeviceLost();

        auto hr = oReset ( device, pPresentationParameters );

        if ( hr >= 0 )
            MenuHelper::Get().OnDeviceReset();

        return hr;
    }
    //--------------------------------------------------------------------------------
    void __stdcall hkCreateMove ( int sequence_number, float input_sample_frametime, bool active, bool& bSendPacket )
    {
        auto oCreateMove = hlclient_hook.get_original<CreateMove> ( index::CreateMove );

        oCreateMove ( g_CHLClient, sequence_number, input_sample_frametime, active );

        auto cmd = g_Input->GetUserCmd ( sequence_number );
        auto verified = g_Input->GetVerifiedCmd ( sequence_number );

        //g_LocalPlayer fix
        //g_LocalPlayer = static_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(g_EngineClient->GetLocalPlayer()));

        if ( !cmd || !cmd->command_number || !bSendPacket || g_Unload || !g_EngineClient || !g_EngineClient->IsConnected() || !g_EngineClient->IsInGame() )
            return;

        //if (g_ClientState->m_nDeltaTick != -1) return;

        //Console.WriteLine(cmd->viewangles);
        /*
        DWORD NetChannel = *(DWORD*)(*(DWORD*)g_ClientState + 0x9C);
        if (NetChannel && g_EngineClient && g_ClientState && !HookedNetchan && g_LocalPlayer && g_EngineClient->IsInGame() && g_EngineClient->IsConnected())
        {
        	auto netchan = *reinterpret_cast<INetChannel**>(reinterpret_cast<std::uintptr_t>(g_ClientState) + 0x9C);
        	clientstate_hook.setup(netchan, "engine.dll");
        	clientstate_hook.hook_index(46, SendDatagram_h);
        	HookedNetchan = true;
        }
        */

		g_Saver.CurrentWeaponRef = g_LocalPlayer->m_hActiveWeapon().Get();
		g_Saver.TickCount = cmd->tick_count;
		g_Saver.CommandNumber = cmd->command_number;

        QAngle OldViewangles = cmd->viewangles;
        float OldForwardmove = cmd->forwardmove;
        float OldSidemove = cmd->sidemove;
		flAngle = cmd->viewangles.yaw;
		QAngle org_angle = cmd->viewangles;

		Rbot::Get().GetTickbase(cmd);
        KeyLoop::Get().OnCreateMove();

		if(MenuHelper::Get().IsVisible())
			cmd->buttons &= ~(IN_ATTACK | IN_ATTACK2);

		g_Saver.PredictionData.reset();
		if (Settings::Misc::BHop)
			BunnyHop::Get().OnCreateMove(cmd);
		
		if (Settings::Misc::AutoStrafe)
			BunnyHop::Get().AutoStrafe(cmd);

		QAngle wish_angle = cmd->viewangles;
		cmd->viewangles = org_angle;

		prediction->Setup(g_Saver.PredictionData);

		prediction->RunPrediction(g_Saver.PredictionData, cmd);
		{
#ifdef _DEBUG
		//Backtrack::Get().OnCreateMove();
#endif // _DEBUG

			Misc::Get().OnCreateMove(cmd);

			Fakelag::Get().OnCreateMove(cmd, bSendPacket);

			if (Settings::RageBot::EnabledAA)
				AntiAim::Get().OnCreateMove(cmd, bSendPacket);

			Lbot::Get().OnCreateMove(cmd);

			if (Settings::Aimbot::LegitAA > 0)
				Lbot::Get().LegitAA(cmd, bSendPacket);

			if (Settings::RageBot::Enabled)
			{
				Rbot::Get().PrecacheShit();
				Rbot::Get().CreateMove(cmd, bSendPacket);
				Rbot::Get().AccuracyBoost(cmd);
			}

			GrenadeHint::Get().Tick(cmd->buttons);

			if (!Settings::RageBot::Enabled)
			{
				Math::FixAngles(cmd->viewangles);
				cmd->viewangles.yaw = std::remainderf(cmd->viewangles.yaw, 360.0f);
			}

			if ((Settings::Aimbot::LegitAA > 0 || Settings::RageBot::DesyncType > 0) && g_ClientState->chokedcommands >= 14)
			{
				bSendPacket = true;
				cmd->viewangles = g_ClientState->viewangles;
			}


			if (!Settings::RageBot::Enabled && Settings::Aimbot::Enabled)
			{
				// from aimware dmp
				static ConVar* m_yaw = m_yaw = g_CVar->FindVar("m_yaw");
				static ConVar* m_pitch = m_pitch = g_CVar->FindVar("m_pitch");
				static ConVar* sensitivity = sensitivity = g_CVar->FindVar("sensitivity");

				static QAngle m_angOldViewangles = g_ClientState->viewangles;

				float delta_x = std::remainderf(cmd->viewangles.pitch - m_angOldViewangles.pitch, 360.0f);
				float delta_y = std::remainderf(cmd->viewangles.yaw - m_angOldViewangles.yaw, 360.0f);

				if (delta_x != 0.0f) {
					float mouse_y = -((delta_x / m_pitch->GetFloat()) / sensitivity->GetFloat());
					short mousedy;
					if (mouse_y <= 32767.0f) {
						if (mouse_y >= -32768.0f) {
							if (mouse_y >= 1.0f || mouse_y < 0.0f) {
								if (mouse_y <= -1.0f || mouse_y > 0.0f)
									mousedy = static_cast<short>(mouse_y);
								else
									mousedy = -1;
							}
							else {
								mousedy = 1;
							}
						}
						else {
							mousedy = 0x8000u;
						}
					}
					else {
						mousedy = 0x7FFF;
					}

					cmd->mousedy = mousedy;
				}

				if (delta_y != 0.0f) {
					float mouse_x = -((delta_y / m_yaw->GetFloat()) / sensitivity->GetFloat());
					short mousedx;
					if (mouse_x <= 32767.0f) {
						if (mouse_x >= -32768.0f) {
							if (mouse_x >= 1.0f || mouse_x < 0.0f) {
								if (mouse_x <= -1.0f || mouse_x > 0.0f)
									mousedx = static_cast<short>(mouse_x);
								else
									mousedx = -1;
							}
							else {
								mousedx = 1;
							}
						}
						else {
							mousedx = 0x8000u;
						}
					}
					else {
						mousedx = 0x7FFF;
					}

					cmd->mousedx = mousedx;
				}

				

				if (Settings::Aimbot::Backtrack)
					Backtrack::Get().FinishLegitBacktrack(cmd);
			}

			if (!Settings::RageBot::Enabled && Settings::TriggerBot::Enabled)
				TriggerBot::Get().OnCreateMove(cmd);

			AntiAim::Get().LbyBreakerPrediction(cmd, bSendPacket);

			if (Settings::RageBot::Enabled && Settings::RageBot::Resolver)
				Resolver::Get().OnCreateMove(OldViewangles);

			if (Settings::Misc::BuyBot)
				BuyBot::Get().OnCreateMove();

			if (Settings::Misc::ClantagType > 0)
				ClantagChanger::Get().OnCreateMove();

			MovementFix::Get().FixMovement(cmd, wish_angle);
		}
		prediction->EndPrediction(g_Saver.PredictionData);
		
        if ( g_LocalPlayer && g_LocalPlayer->IsAlive() && ( cmd->buttons & IN_ATTACK || cmd->buttons & IN_ATTACK2 ) )
            g_Saver.LastShotEyePos = g_LocalPlayer->GetEyePos();

        if ( g_Saver.RbotDidLastShot )
        {
            g_Saver.RbotDidLastShot = false;

            if ( bSendPacket )
                bSendPacket = false;
        }

		
		/*if (Settings::RageBot::Enabled && Settings::RageBot::LagComp)
		{
			for (int i = 1; i <= g_GlobalVars->maxClients; i++)
			{
				C_BasePlayer* player = C_BasePlayer::GetPlayerByIndex(i);
				if(player && player != g_LocalPlayer)
					LagCompensation::Get().UpdateAnimations(player);
			}
		}*/

        Math::NormalizeAngles ( cmd->viewangles );


		if ( Settings::RageBot::SlideWalk )
            AntiAim::Get().SlideWalk ( cmd );

        Math::ClampAngles ( cmd->viewangles );

        verified->m_cmd = *cmd;
        verified->m_crc = cmd->GetChecksum();

		/* Force updating in game thread */
		static float updateTime = g_GlobalVars->curtime + .1f;
		static bool shouldForceUpdate = false;

		if (g_Saver.RequestForceUpdate)
		{
			g_Saver.RequestForceUpdate = false;
			updateTime = g_GlobalVars->curtime + .1f;
			shouldForceUpdate = true;
		}

		if (shouldForceUpdate)
		{
			if (g_GlobalVars->curtime > updateTime)
			{
				shouldForceUpdate = false;
				g_ClientState->ForceFullUpdate();
			}
		}

		if (!o_TempEntities)
		{
			clientstate_hook.setup((uintptr_t*)((uintptr_t)g_ClientState + 0x8));
			clientstate_hook.hook_index(index::TempEntities, hkTempEntities);
			o_TempEntities = clientstate_hook.get_original<TempEntities>(index::TempEntities);
		}
    }
    //--------------------------------------------------------------------------------
    __declspec ( naked ) void __stdcall hkCreateMove_Proxy ( int sequence_number, float input_sample_frametime, bool active )
    {
        __asm
        {
            push ebp
            mov  ebp, esp
            push ebx
            lea  ecx, [esp]
            push ecx
            push dword ptr[active]
            push dword ptr[input_sample_frametime]
            push dword ptr[sequence_number]
            call Hooks::hkCreateMove
            pop  ebx
            pop  ebp
            retn 0Ch
        }
    }
    //--------------------------------------------------------------------------------
    void __stdcall hkPaintTraverse ( vgui::VPANEL panel, bool forceRepaint, bool allowForce )
    {
        static auto panelId = vgui::VPANEL{ 0 };
        static auto oPaintTraverse = vguipanel_hook.get_original<PaintTraverse> ( index::PaintTraverse );

		//static ConVar* cl_csm_enabled = g_CVar->FindVar("cl_csm_enabled ");

		if( Settings::Visual::NoScopeOverlay && !strcmp("HudZoom", g_VGuiPanel->GetName(panel)) )
            return;

        oPaintTraverse ( g_VGuiPanel, panel, forceRepaint, allowForce );

        if ( g_Unload )
            return;

		static bool* s_bOverridePostProcessingDisable = *(bool**)(Utils::PatternScan(GetModuleHandleW(L"client_panorama.dll"), "80 3D ? ? ? ? ? 53 56 57 0F 85") + 0x2);
		*s_bOverridePostProcessingDisable = Settings::Visual::DisablePP;

        if ( !panelId )
        {
            const auto panelName = g_VGuiPanel->GetName ( panel );

            if ( !strcmp ( panelName, "FocusOverlayPanel" ) )
                panelId = panel;
        }
        else if ( panelId == panel )
        {
            //Ignore 50% cuz it called very often
            static bool bSkip = false;
            bSkip = !bSkip;

            if ( bSkip )
                return;
			if ( g_LocalPlayer && InputSys::Get().IsKeyDown(VK_TAB) && Settings::Misc::RankReveal )
                Utils::RankRevealAll();

			if(Settings::Misc::EventLogEnabled)
				EventLogger::Get().PaintTraverse();

            Render::Get().BeginScene();
			GrenadeHint::Get().Paint();
        }
    }
    //--------------------------------------------------------------------------------
    void __stdcall hkEmitSound1 ( IRecipientFilter& filter, int iEntIndex, int iChannel, const char* pSoundEntry, unsigned int nSoundEntryHash, const char* pSample, float flVolume, int nSeed, float flAttenuation, int iFlags, int iPitch, const Vector* pOrigin, const Vector* pDirection, void* pUtlVecOrigins, bool bUpdatePositions, float soundtime, int speakerentity, int unk )
    {
        static auto ofunc = sound_hook.get_original<EmitSound1> ( index::EmitSound1 );

		if (!strcmp(pSoundEntry, "UIPanorama.popup_accept_match_beep"))
			Misc::Get().SetLocalPlayerReady();

        ofunc ( g_EngineSound, filter, iEntIndex, iChannel, pSoundEntry, nSoundEntryHash, pSample, flVolume, nSeed, flAttenuation, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity, unk );
    }
    //--------------------------------------------------------------------------------
    int __stdcall hkDoPostScreenEffects ( int a1 )
    {
        auto oDoPostScreenEffects = clientmode_hook.get_original<DoPostScreenEffects> ( index::DoPostScreenSpaceEffects );

        //if ( g_LocalPlayer && g_Config.GetBool ( "glow_enabled" ) && !g_Unload && g_EngineClient->IsConnected() && g_EngineClient->IsInGame() )
		if (g_LocalPlayer && !g_Unload && g_EngineClient->IsConnected() && g_EngineClient->IsInGame())
            Glow::Get().Run();

        return oDoPostScreenEffects ( g_ClientMode, a1 );
    }
	//--------------------------------------------------------------------------------
	void __fastcall hkDrawModelExecute(void* pEcx, void* pEdx, void* pResults, DrawModelInfo_t* pInfo, matrix3x4_t* pBoneToWorld, float* flpFlexWeights, float* flpFlexDelayedWeights, Vector& vrModelOrigin, int32_t iFlags)
	{
		static auto ofunc = mdlrender_hook.get_original<DrawModelExecute>(index::DrawModelExecute);
		bool forced_mat = !g_MdlRender->IsForcedMaterialOverride();
		if (forced_mat)
			Chams::Get().OnDrawModelExecute(pResults, pInfo, pBoneToWorld, flpFlexWeights, flpFlexDelayedWeights, vrModelOrigin, iFlags);

		ofunc(pEcx, pResults, pInfo, pBoneToWorld, flpFlexWeights, flpFlexDelayedWeights, vrModelOrigin, iFlags);

		if (forced_mat)
			g_MdlRender->ForcedMaterialOverride(nullptr);
	}
	//--------------------------------------------------------------------------------
    void __stdcall hkFrameStageNotify ( ClientFrameStage_t stage )
    {
        static auto ofunc = hlclient_hook.get_original<FrameStageNotify> ( index::FrameStageNotify );

		Skinchanger::Get().OnFrameStageNotify(stage);

		QAngle aim_punch_old;
		QAngle view_punch_old;

		QAngle* aim_punch = nullptr;
		QAngle* view_punch = nullptr;

        //if (g_ClientState->m_nDeltaTick != -1) return  ofunc(g_CHLClient, stage);
        if ( !g_EngineClient->IsConnected() || !g_EngineClient->IsInGame() )
            return ofunc ( g_CHLClient, stage );

        if ( !g_Unload )
            Misc::Get().OnFrameStageNotify ( stage );

		QAngle vecAimPunch;
		QAngle vecViewPunch;

        switch ( stage )
        {
            case FRAME_UNDEFINED:
                break;

            case FRAME_START:
                break;

            case FRAME_NET_UPDATE_START:
                break;

            case FRAME_NET_UPDATE_POSTDATAUPDATE_START:
            {
				//Misc::Get().PunchAngleFix_FSN();
				break;
            }

            case FRAME_NET_UPDATE_POSTDATAUPDATE_END:
            {
                if ( g_Unload )
                    return;

				
				if ( Settings::RageBot::Enabled && Settings::RageBot::Resolver )
                    Resolver::Get().OnFramestageNotify();

                NoSmoke::Get().OnFrameStageNotify();

                for ( int i = 1; i < g_EngineClient->GetMaxClients(); i++ )
                {
                    auto entity = static_cast<C_BasePlayer*> ( g_EntityList->GetClientEntity ( i ) );

                    if ( !entity || !g_LocalPlayer || !entity->IsPlayer() || entity->IsDormant()
                            || !entity->IsAlive() )
                        continue;

                    VarMapping_t* map = entity->VarMapping();

                    if ( map )
                    {
                        for ( int j = 0; j < map->m_nInterpolatedEntries; j++ )
                            map->m_Entries[j].m_bNeedsToInterpolate = false;
                    }
                }

                break;
            }

            case FRAME_NET_UPDATE_END:

				if (Settings::RageBot::LagComp)
					LagCompensation::Get().FrameUpdatePostEntityThink();

                break;

            case FRAME_RENDER_START:
            {
                if ( !g_Unload )
                {
					//UpdateSpoofer();
					bool rbot = Settings::RageBot::Enabled;

					if ( Settings::RageBot::Enabled && Settings::RageBot::EnabledAA && Settings::Visual::ThirdPersonEnabled )
                    {
                        //ThirdpersonAngleHelper::Get().SetThirdpersonAngle();
                        //ThirdpersonAngleHelper::Get().AnimFix();

						g_Prediction->SetLocalViewAngles(g_LocalPlayer->m_angEyeAngles());
						if(Settings::RageBot::DesyncType > 0 || Settings::Aimbot::LegitAA > 0)
							g_LocalPlayer->SetAbsAngles(QAngle(0.f, g_Saver.DesyncYaw, 0.f));
						else
							g_LocalPlayer->SetAbsAngles(QAngle(0.f, g_LocalPlayer->GetPlayerAnimState()->m_flGoalFeetYaw, 0.f));
						g_LocalPlayer->UpdateClientSideAnimation();

						if (!g_LocalPlayer->IsAlive())
							g_LocalPlayer->m_iObserverMode() = 5;
						
                        //bool Moving = g_LocalPlayer->m_vecVelocity().Length2D() > 0.1f || ( cmd->sidemove != 0.f || cmd->forwardmove != 0.f );
                        //bool InAir = ! ( g_LocalPlayer->m_fFlags() & FL_ONGROUND );

                        // if ( !Moving && !InAir )
                        //    g_LocalPlayer->m_fFlags = ACT_FLY;

						
                        //ThirdpersonAngleHelper::Get().SetThirdpersonAngle();
                    }
                    else
                    {
                        if ( g_LocalPlayer && g_LocalPlayer->IsAlive() )
                            g_LocalPlayer->m_bClientSideAnimation() = true;
                    }

					if (Settings::Misc::NoVisualRecoil)
					{
						if (g_LocalPlayer && g_LocalPlayer->IsAlive())
						{
							// Store their current values..
							vecAimPunch = g_LocalPlayer->m_aimPunchAngle();
							vecViewPunch = g_LocalPlayer->m_viewPunchAngle();

							// ..then replace them with zero.
							g_LocalPlayer->m_aimPunchAngle() = QAngle(0, 0, 0);
							g_LocalPlayer->m_viewPunchAngle() = QAngle(0, 0, 0);
						}
					}

                    auto old_curtime = g_GlobalVars->curtime;
                    auto old_frametime = g_GlobalVars->frametime;

                    /*for ( int i = 1; i < g_EngineClient->GetMaxClients(); i++ )
                    {
                        auto entity = static_cast<C_BasePlayer*> ( g_EntityList->GetClientEntity ( i ) );

                        if ( !entity || !g_LocalPlayer || !entity->IsPlayer() || entity->IsDormant()
                                || !entity->IsAlive() )
                            continue;

                        if ( rbot )
                        {
                            entity->InvalidateBoneCache();
                            ThirdpersonAngleHelper::Get().EnemyAnimationFix ( entity );
                        }
                        else
                            entity->m_bClientSideAnimation() = true;
                    }*/

                    g_GlobalVars->curtime = old_curtime;
                    g_GlobalVars->frametime = old_frametime;
                }

                break;
            }

            case FRAME_RENDER_END:
				if (Settings::Visual::NightMode)
					NightMode::Get().Apply(false);
				else
					NightMode::Get().Revert();
                break;
        }
		

        ofunc ( g_CHLClient, stage );
		if (Settings::Misc::NoVisualRecoil)
		{
			g_LocalPlayer->m_aimPunchAngle() = vecAimPunch;
			g_LocalPlayer->m_viewPunchAngle() = vecViewPunch;
		}
    }
    //--------------------------------------------------------------------------------
    void __stdcall hkOverrideView ( CViewSetup* vsView )
    {
        static auto ofunc = clientmode_hook.get_original<OverrideView> ( index::OverrideView );

        if ( !g_EngineClient->IsConnected() || !g_EngineClient->IsInGame() )
            return ofunc ( g_ClientMode, vsView );

		GrenadeHint::Get().View();

        if ( g_EngineClient->IsInGame() && vsView )
            Visuals::Get().ThirdPerson();

		if ( g_LocalPlayer && g_LocalPlayer->m_bIsScoped() && !Settings::Visual::DisableScopeZoom )
            return ofunc ( g_ClientMode, vsView );

		vsView->fov = Settings::Visual::FOV;

        ofunc ( g_ClientMode, vsView );
    }
    //--------------------------------------------------------------------------------
    void __stdcall hkLockCursor()
    {
        static auto ofunc = vguisurf_hook.get_original<LockCursor_t> ( index::LockCursor );

        if ( MenuHelper::Get().IsVisible() )
        {
            g_VGuiSurface->UnlockCursor();
            return;
        }

        ofunc ( g_VGuiSurface );

    }
    //--------------------------------------------------------------------------------
    /*void __stdcall hkDrawModelExecute ( IMatRenderContext* ctx, const DrawModelState_t& state, const ModelRenderInfo_t& pInfo, matrix3x4_t* pCustomBoneToWorld )
    {
        static auto ofunc = mdlrender_hook.get_original<DrawModelExecute> ( index::DrawModelExecute );
        ofunc ( g_MdlRender, ctx, state, pInfo, pCustomBoneToWorld );
    }*/

    auto dwCAM_Think = Utils::PatternScan ( GetModuleHandleW ( L"client_panorama.dll" ), "85 C0 75 30 38 86" );
    typedef bool ( __thiscall* svc_get_bool_t ) ( PVOID );
    bool __fastcall hkSvCheatsGetBool ( PVOID pConVar, void* edx )
    {
        static auto ofunc = sv_cheats.get_original<svc_get_bool_t> ( 13 );

        if ( !ofunc )
            return false;

        if ( reinterpret_cast<DWORD> ( _ReturnAddress() ) == reinterpret_cast<DWORD> ( dwCAM_Think ) )
            return true;

        return ofunc ( pConVar );
    }

	bool __fastcall hkIsHLTV(void* ECX, void* EDX)
	{
		if ( reinterpret_cast<DWORD>(_ReturnAddress()) == reinterpret_cast<DWORD>(retAddr) )
			return true;
		return o_IsHLTV(ECX);
	}

	bool __fastcall hkShouldDrawFog(void* ecx, void* edx)
	{
		return !Settings::Visual::DisablePP;
	}

	void __stdcall FireBullets_PostDataUpdate(C_TEFireBullets* thisptr, DataUpdateType_t updateType)
	{
		static auto ofunc = firebullets_hook.get_original<FireBullets>(index::FireBullets);

		if (!g_LocalPlayer || !g_LocalPlayer->IsAlive())
			return ofunc(thisptr, updateType);

//		if (Settings::RageBot::LagComp && thisptr)
//		{
//			int iPlayer = thisptr->m_iPlayer + 1;
//			if (iPlayer < 64)
//			{
//				auto player = C_BasePlayer::GetPlayerByIndex(iPlayer);
//
//				if (player && player != g_LocalPlayer && !player->IsDormant() && player->m_iTeamNum() != g_LocalPlayer->m_iTeamNum())
//				{
//					QAngle eyeAngles = QAngle(thisptr->m_vecAngles.pitch, thisptr->m_vecAngles.yaw, thisptr->m_vecAngles.roll);
//					QAngle calcedAngle = Math::CalcAngle(player->GetEyePos(), g_LocalPlayer->GetEyePos());
//
//					thisptr->m_vecAngles.pitch = calcedAngle.pitch;
//					thisptr->m_vecAngles.yaw = calcedAngle.yaw;
//					thisptr->m_vecAngles.roll = 0.f;
//
//					float
//						event_time = g_GlobalVars->tickcount,
//						player_time = player->m_flSimulationTime();
//
//					// Extrapolate tick to hit scouters etc
//					auto lag_records = LagCompensation::Get().m_LagRecord[iPlayer];
//
//					float shot_time = TICKS_TO_TIME(event_time);
//					for (auto& record : lag_records)
//					{
//						if (record.m_iTickCount <= event_time)
//						{
//							shot_time = record.m_flSimulationTime + TICKS_TO_TIME(event_time - record.m_iTickCount); // also get choked from this
//#ifdef _DEBUG
//							g_CVar->ConsoleColorPrintf(Color(0, 255, 0, 255), "Found <<exact>> shot time: %f, ticks choked to get here: %d\n", shot_time, event_time - record.m_iTickCount);
//#endif
//							break;
//						}
//#ifdef _DEBUG
//						else
//							g_CVar->ConsolePrintf("Bad curtime difference, EVENT: %f, RECORD: %f\n", event_time, record.m_iTickCount);
//#endif
//					}
//#ifdef _DEBUG
//					g_CVar->ConsolePrintf("Calced angs: %f %f, Event angs: %f %f, CURTIME_TICKOUNT: %f, SIMTIME: %f, CALCED_TIME: %f\n", calcedAngle.pitch, calcedAngle.yaw, eyeAngles.pitch, eyeAngles.yaw, event_time, player_time, shot_time);
//#endif
//					if (!lag_records.empty())
//					{
//						int choked = floorf((event_time - player_time) / g_GlobalVars->interval_per_tick) + 0.5;
//						choked = (choked > 14 ? 14 : choked < 1 ? 0 : choked);
//						player->m_vecOrigin() = (lag_records.begin()->m_vecOrigin + (g_GlobalVars->interval_per_tick * lag_records.begin()->m_vecVelocity * choked));
//					}
//
//					LagCompensation::Get().SetOverwriteTick(player, calcedAngle, shot_time, 1);
//				}
//			}
//		}

		ofunc(thisptr, updateType);
	}


	__declspec (naked) void __stdcall hkTEFireBulletsPostDataUpdate(DataUpdateType_t updateType)
	{
		__asm
		{
			push[esp + 4]
			push ecx
			call FireBullets_PostDataUpdate
			retn 4
		}
	}

	void __stdcall hkSuppressLists(int a2, bool a3)
	{
		static auto ofunc = partition_hook.get_original< SuppressLists >(index::SuppressLists);

		static auto OnRenderStart_Return = Utils::PatternScan(GetModuleHandleA("client_panorama.dll"), "FF 50 40 8B 1D ? ? ? ?") + 0x3;
		static auto FrameNetUpdateEnd_Return = Utils::PatternScan(GetModuleHandleA("client_panorama.dll"), "5F 5E 5D C2 04 00 83 3D ? ? ? ? ?");

		if (g_LocalPlayer && g_LocalPlayer->IsAlive()) 
		{
			if (_ReturnAddress() == OnRenderStart_Return) 
			{
				//static auto set_abs_angles = Utils::PatternScan(GetModuleHandleA("client_panorama.dll"), "55 8B EC 83 E4 F8 83 EC 64 53 56 57 8B F1 E8");
				//reinterpret_cast<void(__thiscall*)(void*, const QAngle&)>(set_abs_angles)(g_LocalPlayer, QAngle(0.0f, g_Saver.AnimState.m_flGoalFeetYaw, 0.0f));
			}
			else if (_ReturnAddress() == FrameNetUpdateEnd_Return) 
			{
				//Skinchanger::Get().OnFrameStageNotify(true);
			}
		}

		ofunc(g_SpatialPartition, a2, a3);
	}

    //--------------------------------------------------------------------------------
    void __fastcall hkSceneEnd ( void* pEcx, void* pEdx )
    {
        static auto ofunc = RenderView_hook.get_original<SceneEnd> ( index::SceneEnd );
        ofunc ( pEcx, pEdx );

        if ( !g_EngineClient->IsConnected() || !g_EngineClient->IsInGame() )
            return;

        if ( g_Unload )
            return;

        //if (g_ClientState->m_nDeltaTick != -1) return;
        // |
        // v
        // code here
    }


    //--------------------------------------------------------------------------------
	bool __stdcall hkFireEvent(IGameEvent* pEvent)
	{
		static auto ofunc = gameevents_hook.get_original<FireEvent>(index::FireEvent);

		if (!g_EngineClient->IsConnected() || !g_EngineClient->IsInGame())
			return ofunc(g_GameEvents, pEvent);

		// -->
		if (Settings::RageBot::Enabled)
		{
			Rbot::Get().OnFireEvent(pEvent);
			Resolver::Get().OnFireEvent(pEvent);
		}
			

		if (!strcmp(pEvent->GetName(), "round_start"))
			BuyBot::Get().OnRoundStart();

		/*if (!strcmp(pEvent->GetName(), "player_footstep"))
		{
			g_Logger.Info("VISUAL", "Sound ESP call");
			Visuals::Get().RenderSoundESP(pEvent);
		}*/
			
		EventLogger::Get().OnFireEvent(pEvent);
        HitPossitionHelper::Get().OnFireEvent ( pEvent );

        return ofunc( g_GameEvents, pEvent );
    }

	static auto random_sequence(const int low, const int high) -> int
	{
		return rand() % (high - low + 1) + low;
	}

	static auto fix_animation(const char* model, const int sequence) -> int
	{
		enum ESequence
		{
			SEQUENCE_DEFAULT_DRAW = 0,
			SEQUENCE_DEFAULT_IDLE1 = 1,
			SEQUENCE_DEFAULT_IDLE2 = 2,
			SEQUENCE_DEFAULT_LIGHT_MISS1 = 3,
			SEQUENCE_DEFAULT_LIGHT_MISS2 = 4,
			SEQUENCE_DEFAULT_HEAVY_MISS1 = 9,
			SEQUENCE_DEFAULT_HEAVY_HIT1 = 10,
			SEQUENCE_DEFAULT_HEAVY_BACKSTAB = 11,
			SEQUENCE_DEFAULT_LOOKAT01 = 12,
			SEQUENCE_BUTTERFLY_DRAW = 0,
			SEQUENCE_BUTTERFLY_DRAW2 = 1,
			SEQUENCE_BUTTERFLY_LOOKAT01 = 13,
			SEQUENCE_BUTTERFLY_LOOKAT03 = 15,
			SEQUENCE_FALCHION_IDLE1 = 1,
			SEQUENCE_FALCHION_HEAVY_MISS1 = 8,
			SEQUENCE_FALCHION_HEAVY_MISS1_NOFLIP = 9,
			SEQUENCE_FALCHION_LOOKAT01 = 12,
			SEQUENCE_FALCHION_LOOKAT02 = 13,
			SEQUENCE_DAGGERS_IDLE1 = 1,
			SEQUENCE_DAGGERS_LIGHT_MISS1 = 2,
			SEQUENCE_DAGGERS_LIGHT_MISS5 = 6,
			SEQUENCE_DAGGERS_HEAVY_MISS2 = 11,
			SEQUENCE_DAGGERS_HEAVY_MISS1 = 12,
			SEQUENCE_BOWIE_IDLE1 = 1,
		};
		if (strstr(model, "models/weapons/v_knife_butterfly.mdl")) {
			switch (sequence) {
			case SEQUENCE_DEFAULT_DRAW:
				return random_sequence(SEQUENCE_BUTTERFLY_DRAW, SEQUENCE_BUTTERFLY_DRAW2);
			case SEQUENCE_DEFAULT_LOOKAT01:
				return random_sequence(SEQUENCE_BUTTERFLY_LOOKAT01, SEQUENCE_BUTTERFLY_LOOKAT03);
			default:
				return sequence + 1;
			}
		}
		else if (strstr(model, "models/weapons/v_knife_falchion_advanced.mdl")) {
			switch (sequence) {
			case SEQUENCE_DEFAULT_IDLE2:
				return SEQUENCE_FALCHION_IDLE1;
			case SEQUENCE_DEFAULT_HEAVY_MISS1:
				return random_sequence(SEQUENCE_FALCHION_HEAVY_MISS1, SEQUENCE_FALCHION_HEAVY_MISS1_NOFLIP);
			case SEQUENCE_DEFAULT_LOOKAT01:
				return random_sequence(SEQUENCE_FALCHION_LOOKAT01, SEQUENCE_FALCHION_LOOKAT02);
			case SEQUENCE_DEFAULT_DRAW:
			case SEQUENCE_DEFAULT_IDLE1:
				return sequence;
			default:
				return sequence - 1;
			}
		}
		else if (strstr(model, "models/weapons/v_knife_push.mdl")) {
			switch (sequence) {
			case SEQUENCE_DEFAULT_IDLE2:
				return SEQUENCE_DAGGERS_IDLE1;
			case SEQUENCE_DEFAULT_LIGHT_MISS1:
			case SEQUENCE_DEFAULT_LIGHT_MISS2:
				return random_sequence(SEQUENCE_DAGGERS_LIGHT_MISS1, SEQUENCE_DAGGERS_LIGHT_MISS5);
			case SEQUENCE_DEFAULT_HEAVY_MISS1:
				return random_sequence(SEQUENCE_DAGGERS_HEAVY_MISS2, SEQUENCE_DAGGERS_HEAVY_MISS1);
			case SEQUENCE_DEFAULT_HEAVY_HIT1:
			case SEQUENCE_DEFAULT_HEAVY_BACKSTAB:
			case SEQUENCE_DEFAULT_LOOKAT01:
				return sequence + 3;
			case SEQUENCE_DEFAULT_DRAW:
			case SEQUENCE_DEFAULT_IDLE1:
				return sequence;
			default:
				return sequence + 2;
			}
		}
		else if (strstr(model, "models/weapons/v_knife_survival_bowie.mdl")) {
			switch (sequence) {
			case SEQUENCE_DEFAULT_DRAW:
			case SEQUENCE_DEFAULT_IDLE1:
				return sequence;
			case SEQUENCE_DEFAULT_IDLE2:
				return SEQUENCE_BOWIE_IDLE1;
			default:
				return sequence - 1;
			}
		}
		else {
			return sequence;
		}
	}

	void hkRecvProxy(const CRecvProxyData* pData, void* entity, void* output)
	{
		static auto ofunc = sequence_hook->get_original_function();
		const auto local = static_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(g_EngineClient->GetLocalPlayer()));
		if (local && local->IsAlive())
		{
			const auto proxy_data = const_cast<CRecvProxyData*>(pData);
			const auto view_model = static_cast<C_BaseViewModel*>(entity);
			if (view_model && view_model->m_hOwner() && view_model->m_hOwner().IsValid())
			{
				const auto owner = static_cast<C_BasePlayer*>(g_EntityList->GetClientEntityFromHandle(view_model->m_hOwner()));
				if (owner == g_EntityList->GetClientEntity(g_EngineClient->GetLocalPlayer()))
				{
					const auto view_model_weapon_handle = view_model->m_hWeapon();
					if (view_model_weapon_handle.IsValid())
					{
						const auto view_model_weapon = static_cast<C_BaseAttributableItem*>(g_EntityList->GetClientEntityFromHandle(view_model_weapon_handle));
						if (view_model_weapon)
						{
							if (k_weapon_info.count(view_model_weapon->m_Item().m_iItemDefinitionIndex()))
							{
								auto original_sequence = proxy_data->m_Value.m_Int;
								const auto override_model = k_weapon_info.at(view_model_weapon->m_Item().m_iItemDefinitionIndex()).model;
								proxy_data->m_Value.m_Int = fix_animation(override_model, proxy_data->m_Value.m_Int);
							}
						}
					}
				}
			}
		}
		ofunc(pData, entity, output);
	}

    //--------------------------------------------------------------------------------
    void __stdcall Hooked_RenderSmokeOverlay ( bool unk ) { /* no need to call :) we want to remove the smoke overlay */ }


	void CL_ParseEventDelta(void* RawData, void* pToData, RecvTable* pRecvTable)
	{
		// "RecvTable_DecodeZeros: table '%s' missing a decoder.", look at the function that calls it.
		static uintptr_t CL_ParseEventDeltaF = (uintptr_t)Utils::PatternScan(GetModuleHandle(L"engine.dll"), ("55 8B EC 83 E4 F8 53 57"));
		__asm
		{
			mov     ecx, RawData
			mov     edx, pToData
			push	pRecvTable
			call    CL_ParseEventDeltaF
			add     esp, 4
		}
	}

	bool __fastcall hkTempEntities(void* ECX, void* EDX, void* msg)
	{
		if (!g_LocalPlayer || !g_EngineClient->IsInGame() || !g_EngineClient->IsConnected())
			return o_TempEntities(ECX, msg);

		bool ret = o_TempEntities(ECX, msg);

		/*never used, so we fix those peoples compiling problems :)*/

		if (!Settings::RageBot::LagComp || !g_LocalPlayer->IsAlive())
			return ret;

		CEventInfo* ei = g_ClientState->events;
		CEventInfo* next = NULL;

		if (!ei)
			return ret;

		// Filtering events
		do
		{
			next = *(CEventInfo * *)((uintptr_t)ei + 0x38);

			uint16_t classID = ei->classID - 1;

			auto m_pCreateEventFn = ei->pClientClass->m_pCreateEventFn; // ei->pClientClass->m_pCreateEventFn ptr
			if (!m_pCreateEventFn)
				continue;

			IClientNetworkable* pCE = m_pCreateEventFn();
			if (!pCE)
				continue;

			if (classID == (int)ClassId::CTEFireBullets)
			{
				// set fire_delay to zero to send out event so its not here later.
				ei->fire_delay = 0.0f;

				auto pRecvTable = ei->pClientClass->m_pRecvTable;
				void *BasePtr = pCE->GetDataTableBasePtr();
				
				// Decode data into client event object and use the DTBasePtr to get the netvars
				CL_ParseEventDelta(ei->pData, BasePtr, pRecvTable);
				
				if (!BasePtr)
					continue;
				
				// This nigga right HERE just fired a BULLET MANE
				int EntityIndex = *(int*)((uintptr_t)BasePtr + 0x10) + 1;
				
				auto pEntity = (C_BasePlayer*)g_EntityList->GetClientEntity(EntityIndex);
				if (pEntity && pEntity->GetClientClass() &&  pEntity->GetClientClass()->m_ClassID == ClassId::CCSPlayer && !(pEntity->m_iTeamNum() == g_LocalPlayer->m_iTeamNum()))  //!pEntity->IsTeamMate())
				{
					QAngle EyeAngles = QAngle(*(float*)((uintptr_t)BasePtr + 0x24), *(float*)((uintptr_t)BasePtr + 0x28), 0.0f),
						CalcedAngle = Math::CalcAngle(pEntity->GetEyePos(), g_LocalPlayer->GetEyePos());
				
					*(float*)((uintptr_t)BasePtr + 0x24) = CalcedAngle.pitch;
					*(float*)((uintptr_t)BasePtr + 0x28) = CalcedAngle.yaw;
					*(float*)((uintptr_t)BasePtr + 0x2C) = 0;
				
					float
						event_time = TICKS_TO_TIME(g_GlobalVars->tickcount),
						player_time = pEntity->m_flSimulationTime();
				
					// Extrapolate tick to hit scouters etc
					auto lag_records = LagCompensation::Get().m_LagRecord[pEntity->EntIndex()];
				
					float shot_time = event_time;
					for (auto& record : lag_records)
					{
						if (TICKS_TO_TIME(record.m_iTickCount) <= event_time)
						{
							shot_time = record.m_flSimulationTime + (event_time - TICKS_TO_TIME(record.m_iTickCount)); // also get choked from this
	#ifdef _DEBUG
							g_CVar->ConsoleColorPrintf(Color(0, 255, 0, 255), "Found exact shot time: %f, ticks choked to get here: %d\n", shot_time, TIME_TO_TICKS(event_time - TICKS_TO_TIME(record.m_iTickCount)));
	#endif
							break;
						}
	#ifdef _DEBUG
						else
							g_CVar->ConsolePrintf("Bad curtime difference, EVENT: %f, RECORD: %f\n", event_time, TICKS_TO_TIME(record.m_iTickCount));
	#endif
					}
	#ifdef _DEBUG
					g_CVar->ConsolePrintf("Calced angs: %f %f, Event angs: %f %f, CURTIME_TICKOUNT: %f, SIMTIME: %f, CALCED_TIME: %f\n", CalcedAngle.pitch, CalcedAngle.yaw, EyeAngles.pitch, EyeAngles.yaw, event_time, player_time, shot_time);
	#endif
					if (!lag_records.empty())
					{
						int choked = floorf((event_time - player_time) / g_GlobalVars->interval_per_tick) + 0.5;
						choked = (choked > 14 ? 14 : choked < 1 ? 0 : choked);
						pEntity->m_vecOrigin() = (lag_records.begin()->m_vecOrigin + (g_GlobalVars->interval_per_tick * lag_records.begin()->m_vecVelocity * choked));
					}
				
					LagCompensation::Get().SetOverwriteTick(pEntity, CalcedAngle, shot_time, 1);
				}
			}
			ei = next;
		} while (next != NULL);

		return ret;
	}

	int32_t nTickBaseShift = 0;
	int32_t nSinceUse = 0;
	bool bInSendMove = false, bFirstSendMovePack = false;

	bool __fastcall hkWriteUsercmdDeltaToBuffer(IBaseClientDLL* ECX, void* EDX, int nSlot, bf_write* buf, int from, int to, bool isNewCmd)
	{
		static auto ofunc = hlclient_hook.get_original<WriteUsercmdDeltaToBuffer_t>(index::WriteUsercmdDeltaToBuffer);
		return true;
	}

		//static DWORD WriteUsercmdDeltaToBufferReturn = (DWORD)Utils::PatternScan(GetModuleHandle("engine.dll"), "84 C0 74 04 B0 01 EB 02 32 C0 8B FE 46 3B F3 7E C9 84 C0 0F 84 ? ? ? ?"); //     84 DB 0F 84 ? ? ? ? 8B 8F ? ? ? ? 8B 01 8B 40 1C FF D0

		/*if (nTickBaseShift <= 0 || (DWORD)_ReturnAddress() != ((DWORD)GetModuleHandleA("engine.dll") + 0xCCCA6))
			return ofunc(ECX, nSlot, buf, from, to, isNewCmd);

		if (from != -1)
			return true;

		// CL_SendMove function
		auto CL_SendMove = []()
		{
			using CL_SendMove_t = void(__fastcall*)(void);
			static CL_SendMove_t CL_SendMoveF = (CL_SendMove_t)Utils::PatternScan(GetModuleHandleW(L"engine.dll"), ("55 8B EC A1 ? ? ? ? 81 EC ? ? ? ? B9 ? ? ? ? 53 8B 98"));

			CL_SendMoveF();
		};

		// WriteUsercmd function
		auto WriteUsercmd = [](bf_write * buf, CUserCmd * in, CUserCmd * out)
		{
			//using WriteUsercmd_t = void(__fastcall*)(bf_write*, CUserCmd*, CUserCmd*);
			//static DWORD WriteUsercmdF = (DWORD)Utils::PatternScan(GetModuleHandleW(L"client_panorama.dll"), ("55 8B EC 83 E4 F8 51 53 56 8B D9 8B 0D"));
			static auto WriteUsercmdFn = (bool(__fastcall*)(bf_write*, CUserCmd*, CUserCmd*))Utils::PatternScan(GetModuleHandleA("client_panorama.dll"), ("55 8B EC 83 E4 F8 51 53 56 8B D9 8B 0D"));
			return WriteUsercmdFn(buf, in, out);

			/*__asm
			{
				mov     ecx, buf
				mov     edx, in
				push	out
				call    WriteUsercmdF
				add     esp, 4
			}*/
		//};

		/*uintptr_t framePtr;
		__asm mov framePtr, ebp;
		auto msg = reinterpret_cast<CCLCMsg_Move_t*>(framePtr + 0xFCC);*/
		/*int* pNumBackupCommands = (int*)(reinterpret_cast<uintptr_t>(buf) - 0x30);
		int* pNumNewCommands = (int*)(reinterpret_cast<uintptr_t>(buf) - 0x2C);
		auto net_channel = g_ClientState->m_NetChannel;

		int32_t new_commands = *pNumNewCommands;

		if (!bInSendMove)
		{
			if (new_commands <= 0)
				return false;

			bInSendMove = true;
			bFirstSendMovePack = true;
			nTickBaseShift += new_commands;

			while (nTickBaseShift > 0)
			{
				CL_SendMove();
				net_channel->Transmit(false);
				bFirstSendMovePack = false;
			}

			bInSendMove = false;
			return false;
		}

		if (!bFirstSendMovePack)
		{
			int32_t loss = std::min(nTickBaseShift, 10);

			nTickBaseShift -= loss;
			net_channel->m_nOutSequenceNr += loss;
		}

		int32_t next_cmdnr = g_ClientState->lastoutgoingcommand + g_ClientState->chokedcommands + 1;
		int32_t total_new_commands = std::min(nTickBaseShift, 62);
		nTickBaseShift -= total_new_commands;

		from = -1;
		*pNumNewCommands = total_new_commands;
		*pNumBackupCommands = 0;

		for (to = next_cmdnr - new_commands + 1; to <= next_cmdnr; to++)
		{
			if (!ofunc(ECX, nSlot, buf, from, to, true))
				return false;

			from = to;
		}

		CUserCmd* last_realCmd = g_Input->GetUserCmd(nSlot, from);
		CUserCmd fromCmd;

		if (last_realCmd)
			fromCmd = *last_realCmd;

		CUserCmd toCmd = fromCmd;
		toCmd.command_number++;
		toCmd.tick_count += 200;

		for (int i = new_commands; i <= total_new_commands; i++)
		{
			WriteUsercmd(buf, &toCmd, &fromCmd);
			fromCmd = toCmd;
			toCmd.command_number++;
			toCmd.tick_count++;
		}

		return true;
	}
    //--------------------------------------------------------------------------------

    /*
    int __fastcall SendDatagram_h(INetChannel * netchan, void *, bf_write * datagram)
    {
    	static auto ofunc = clientstate_hook.get_original<SendDatagram_t>(index::SendDatagram);
    	return ofunc(netchan, datagram);
    	return;
    	int32_t reliable_state = netchan->m_nInReliableState;
    	int32_t sequencenr = netchan->m_nInSequenceNr;
    	int32_t outseqncenr = netchan->m_nOutSequenceNr;

    	/*
    	C_BasePlayer* local = static_cast<C_BasePlayer*>(g_EntityList->GetClientEntity(g_EngineClient->GetLocalPlayer()));

    	if (local && local->IsAlive() && g_Saver.CurrentInLbyBreak)
    	{
    		//g_Logger.Add("m_fClearTime", std::to_string(netchan->m_fClearTime), Color::Blue); + going up (3.850775)
    		netchan->m_nOutSequenceNr *= 150;
    		g_Saver.CurrentInLbyBreak = false;
    	}


    	int ret = ofunc(netchan, datagram);

    	netchan->m_nInReliableState = reliable_state;
    	netchan->m_nInSequenceNr = sequencenr;
    	netchan->m_nOutSequenceNr = outseqncenr;

    	return ret;
    }
    */
}
