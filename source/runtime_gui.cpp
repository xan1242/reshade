/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "version.h"
#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "runtime.hpp"
#include "runtime_config.hpp"
#include "runtime_objects.hpp"
#include "input.hpp"
#include "imgui_widgets.hpp"
#include <cassert>
#include <fstream>
#include <algorithm>
#include <shellapi.h>
#ifdef GAME_MW
#include "NFSMW_PreFEngHook.h"
#endif
#ifdef GAME_CARBON
#include "NFSC_PreFEngHook.h"
#endif
#ifdef GAME_UG2
#include "NFSU2_PreFEngHook.h"
#endif
#ifdef GAME_UG
#include "NFSU_PreFEngHook.h"
#endif
#ifdef GAME_PS
#include "NFSPS_PreFEngHook.h"
#endif
#ifdef GAME_UC
#include "NFSUC_PreFEngHook.h"
#endif

using namespace reshade::gui;

static std::string g_window_state_path;

static const ImVec4 COLOR_RED = ImColor(240, 100, 100);
static const ImVec4 COLOR_YELLOW = ImColor(204, 204, 0);

void reshade::runtime::init_gui()
{
	if (g_window_state_path.empty())
		g_window_state_path = (g_reshade_base_path / L"ReShadeGUI.ini").u8string();

	// Default shortcut: Home
	_overlay_key_data[0] = 0x24;
	_overlay_key_data[1] = false;
	_overlay_key_data[2] = false;
	_overlay_key_data[3] = false;

	_editor.set_readonly(true);
	_viewer.set_readonly(true); // Viewer is always read-only

	_imgui_context = ImGui::CreateContext();
	auto &imgui_io = _imgui_context->IO;
	auto &imgui_style = _imgui_context->Style;
	imgui_io.IniFilename = nullptr;
	imgui_io.KeyMap[ImGuiKey_Tab] = 0x09; // VK_TAB
	imgui_io.KeyMap[ImGuiKey_LeftArrow] = 0x25; // VK_LEFT
	imgui_io.KeyMap[ImGuiKey_RightArrow] = 0x27; // VK_RIGHT
	imgui_io.KeyMap[ImGuiKey_UpArrow] = 0x26; // VK_UP
	imgui_io.KeyMap[ImGuiKey_DownArrow] = 0x28; // VK_DOWN
	imgui_io.KeyMap[ImGuiKey_PageUp] = 0x21; // VK_PRIOR
	imgui_io.KeyMap[ImGuiKey_PageDown] = 0x22; // VK_NEXT
	imgui_io.KeyMap[ImGuiKey_Home] = 0x24; // VK_HOME
	imgui_io.KeyMap[ImGuiKey_End] = 0x23; // VK_END
	imgui_io.KeyMap[ImGuiKey_Insert] = 0x2D; // VK_INSERT
	imgui_io.KeyMap[ImGuiKey_Delete] = 0x2E; // VK_DELETE
	imgui_io.KeyMap[ImGuiKey_Backspace] = 0x08; // VK_BACK
	imgui_io.KeyMap[ImGuiKey_Space] = 0x20; // VK_SPACE
	imgui_io.KeyMap[ImGuiKey_Enter] = 0x0D; // VK_RETURN
	imgui_io.KeyMap[ImGuiKey_Escape] = 0x1B; // VK_ESCAPE
	imgui_io.KeyMap[ImGuiKey_A] = 'A';
	imgui_io.KeyMap[ImGuiKey_C] = 'C';
	imgui_io.KeyMap[ImGuiKey_V] = 'V';
	imgui_io.KeyMap[ImGuiKey_X] = 'X';
	imgui_io.KeyMap[ImGuiKey_Y] = 'Y';
	imgui_io.KeyMap[ImGuiKey_Z] = 'Z';
	imgui_io.ConfigFlags = ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
	imgui_io.BackendFlags = ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_RendererHasVtxOffset;

	// Disable rounding by default
	imgui_style.GrabRounding = 0.0f;
	imgui_style.FrameRounding = 0.0f;
	imgui_style.ChildRounding = 0.0f;
	imgui_style.ScrollbarRounding = 0.0f;
	imgui_style.WindowRounding = 0.0f;
	imgui_style.WindowBorderSize = 0.0f;

	ImGui::SetCurrentContext(nullptr);

	subscribe_to_ui("Home", [this]() { draw_gui_home(); });
	subscribe_to_ui("Settings", [this]() { draw_gui_settings(); });
	subscribe_to_ui("Statistics", [this]() { draw_gui_statistics(); });
	subscribe_to_ui("Log", [this]() { draw_gui_log(); });
	subscribe_to_ui("About", [this]() { draw_gui_about(); });
	subscribe_to_ui("NFS Tweaks", [this]() { draw_gui_nfs(); });

	_load_config_callables.push_back([this, &imgui_io, &imgui_style](const ini_file &config) {
		config.get("INPUT", "KeyOverlay", _overlay_key_data);
		config.get("INPUT", "InputProcessing", _input_processing_mode);

		config.get("OVERLAY", "ClockFormat", _clock_format);
		config.get("OVERLAY", "FPSPosition", _fps_pos);
		config.get("OVERLAY", "NoFontScaling", _no_font_scaling);
		config.get("OVERLAY", "ShowClock", _show_clock);
		config.get("OVERLAY", "ShowForceLoadEffectsButton", _show_force_load_effects_button);
		config.get("OVERLAY", "ShowFPS", _show_fps);
		config.get("OVERLAY", "ShowFrameTime", _show_frametime);
		config.get("OVERLAY", "ShowScreenshotMessage", _show_screenshot_message);
		config.get("OVERLAY", "TutorialProgress", _tutorial_index);
		config.get("OVERLAY", "VariableListHeight", _variable_editor_height);
		config.get("OVERLAY", "VariableListUseTabs", _variable_editor_tabs);

		bool save_imgui_window_state = false;
		config.get("OVERLAY", "SaveWindowState", save_imgui_window_state);
		imgui_io.IniFilename = save_imgui_window_state ? g_window_state_path.c_str() : nullptr;

		config.get("STYLE", "Alpha", imgui_style.Alpha);
		config.get("STYLE", "ChildRounding", imgui_style.ChildRounding);
		config.get("STYLE", "ColFPSText", _fps_col);
		config.get("STYLE", "EditorFont", _editor_font);
		config.get("STYLE", "EditorFontSize", _editor_font_size);
		config.get("STYLE", "EditorStyleIndex", _editor_style_index);
		config.get("STYLE", "Font", _font);
		config.get("STYLE", "FontSize", _font_size);
		config.get("STYLE", "FPSScale", _fps_scale);
		config.get("STYLE", "FrameRounding", imgui_style.FrameRounding);
		config.get("STYLE", "GrabRounding", imgui_style.GrabRounding);
		config.get("STYLE", "PopupRounding", imgui_style.PopupRounding);
		config.get("STYLE", "ScrollbarRounding", imgui_style.ScrollbarRounding);
		config.get("STYLE", "StyleIndex", _style_index);
		config.get("STYLE", "TabRounding", imgui_style.TabRounding);
		config.get("STYLE", "WindowRounding", imgui_style.WindowRounding);

		// For compatibility with older versions, set the alpha value if it is missing
		if (_fps_col[3] == 0.0f)
			_fps_col[3]  = 1.0f;

		load_custom_style();
	});
	_save_config_callables.push_back([this, &imgui_io, &imgui_style](ini_file &config) {
		config.set("INPUT", "KeyOverlay", _overlay_key_data);
		config.set("INPUT", "InputProcessing", _input_processing_mode);

		config.set("OVERLAY", "ClockFormat", _clock_format);
		config.set("OVERLAY", "FPSPosition", _fps_pos);
		config.set("OVERLAY", "NoFontScaling", _no_font_scaling);
		config.set("OVERLAY", "ShowClock", _show_clock);
		config.set("OVERLAY", "ShowForceLoadEffectsButton", _show_force_load_effects_button);
		config.set("OVERLAY", "ShowFPS", _show_fps);
		config.set("OVERLAY", "ShowFrameTime", _show_frametime);
		config.set("OVERLAY", "ShowScreenshotMessage", _show_screenshot_message);
		config.set("OVERLAY", "TutorialProgress", _tutorial_index);
		config.set("OVERLAY", "VariableListHeight", _variable_editor_height);
		config.set("OVERLAY", "VariableListUseTabs", _variable_editor_tabs);

		const bool save_imgui_window_state = imgui_io.IniFilename != nullptr;
		config.set("OVERLAY", "SaveWindowState", save_imgui_window_state);

		config.set("STYLE", "Alpha", imgui_style.Alpha);
		config.set("STYLE", "ChildRounding", imgui_style.ChildRounding);
		config.set("STYLE", "ColFPSText", _fps_col);
		config.set("STYLE", "EditorFont", _editor_font);
		config.set("STYLE", "EditorFontSize", _editor_font_size);
		config.set("STYLE", "EditorStyleIndex", _editor_style_index);
		config.set("STYLE", "Font", _font);
		config.set("STYLE", "FontSize", _font_size);
		config.set("STYLE", "FPSScale", _fps_scale);
		config.set("STYLE", "FrameRounding", imgui_style.FrameRounding);
		config.set("STYLE", "GrabRounding", imgui_style.GrabRounding);
		config.set("STYLE", "PopupRounding", imgui_style.PopupRounding);
		config.set("STYLE", "ScrollbarRounding", imgui_style.ScrollbarRounding);
		config.set("STYLE", "StyleIndex", _style_index);
		config.set("STYLE", "TabRounding", imgui_style.TabRounding);
		config.set("STYLE", "WindowRounding", imgui_style.WindowRounding);

		// Do not save custom style colors by default, only when actually used and edited
	});
}
void reshade::runtime::deinit_gui()
{
	ImGui::DestroyContext(_imgui_context);
}

void reshade::runtime::build_font_atlas()
{
	ImFontAtlas *const atlas = _imgui_context->IO.Fonts;
	// Remove any existing fonts from atlas first
	atlas->Clear();

	for (unsigned int i = 0; i < 2; ++i)
	{
		ImFontConfig cfg;
		cfg.SizePixels = static_cast<float>(i == 0 ? _font_size : _editor_font_size);

		const std::filesystem::path &font_path = i == 0 ? _font : _editor_font;
		if (std::error_code ec; !std::filesystem::is_regular_file(font_path, ec) || !atlas->AddFontFromFileTTF(font_path.u8string().c_str(), cfg.SizePixels))
			atlas->AddFontDefault(&cfg); // Use default font if custom font failed to load or does not exist

		if (i == 0)
		{
			// Merge icons into main font (was generated from Fork Awesome font https://forkawesome.github.io with https://github.com/aiekick/ImGuiFontStudio)
			static const char icon_font_data[] =
				"7])#######qgGmo'/###V),##+Sl##Q6>##w#S+Hh=?<a7*&T&d.7m/oJ[^IflZg#BfG<-iNE/1-2JuBw0'B)i,>>#'tEn/<_[FHkp#L#,)m<-:qEn/@d@UCGD7s$_gG<-]rK8/XU#[A"
				">7X*M^iEuLQaX1DIMr62DXe(#=eR%#_AFmBFF1J5h@6gLYwG`-77LkOETt?0(MiSAq@ClLS[bfL)YZ##E)1w--Aa+MNq;?#-D^w'0bR5'Cv9N(f$IP/371^#IhOSMoH<mL6kSG2mEexF"
				"TP'##NjToIm3.AF4@;=-1`/,M)F5gL':#gLIlJGMIfG<-IT*COI=-##.<;qMTl:$#L7cwL#3#&#W(^w5i*l.q3;02qtKJ5q'>b9qx:`?q*R[SqOim4vII3L,J-eL,o[njJ@Ro?93VtA#"
				"n;4L#?C(DNgJG&#D;B;-KEII-O&Ys1,AP##0Mc##4Yu##fRXgLC;E$#@(V$#jDA]%f,);?o[3YckaZfCj>)Mp64YS76`JYYZGUSItO,AtgC_Y#>v&##je+gL)SL*57*vM($i?X-,BG`a"
				"*HWf#Ybf;-?G^##$VG13%&>uuYg8e$UNc##D####,03/MbPMG)@f)T/^b%T%S2`^#J:8Y.pV*i(T=)?#h-[guT#9iu&](?#A]wG;[Dm]uB*07QK(]qFV=fV$H[`V$#kUK#$8^fLmw@8%"
				"P92^uJ98=/+@u2OFC.o`5BOojOps+/q=110[YEt$bcx5#kN:tLmb]s$wkH>#iE(E#VA@r%Mep$-#b?1,G2J1,mQOhuvne.Mv?75/Huiw'6`?$=VC]&,EH*7M:9s%,:7QVQM]X-?^10ip"
				"&ExrQF7$##lnr?#&r&t%NEE/2XB+a4?c=?/^0Xp%kH$IM$?YCjwpRX:CNNjL<b7:.rH75/1uO]unpchLY.s%,R=q9V%M,)#%)###Tx^##x@PS.kN%##AFDZ#5D-W.-kr.0oUFb35IL,3"
				"%A2*/RtC.3j85L#gJ))3rx3I):2Cv-FX(9/vBo8%=[%?5BKc8/t=r$#<MfD*.gB.*Q&WS7#r&ipf9b^EhD]:/%A@`aU5b'O]I03N2cUN0ebYF':?AE+OaUq).]tfd]s0$d0C9E#enTtQ"
				"&(oqL^)sB#`:5Yu9A;TFN%MeMBhZd3J0(6:8mc0CdpC,)#5>X(3^*rMo&Y9%)[[c;QIt<1q3n0#MI`QjeUB,MCG#&#n#]I*/=_hLfM]s$Cr&t%#M,W-d9'hlM2'J35i4f)_Y_Z-Mx;=."
				"Z&f.:a[xw'2q'Y$G38$5Zs<7.;G(<-$87V?M42X-n+[w'1q-[Bv(ofL2@R2L/%dDE=?CG)bhho.&1(a4D/NF3;G`=.Su$s$]WD.3jY5lLBMuM(Hnr?#FTFo26N.)*,/_Yox3prHH]G>u"
				"?#Ke$+tG;%M)H3uH@ta$/$bku/1.E4:po2/Z5wGE^e*:*Mgj8&=B]'/-=h1B-n4GVaVQu.gm^6X,&Gj/Run+M,jd##kJ,/1=-U,2QNv)42.,Q'iUKF*wCXI)+f1B4./.&4maJX-'$fF4"
				"uX@8%5Y,>#r:N&l,=GNGA5AZ$XA`0(Q^(k'(GB.WKx+M/mi5###%D1Mh;BE+O.1w,p8QSV`E3$%sMwH)/t6iLF.'DX[G&8Remo7/,w/RNRPUV$)8[0#G,>>#d=OZ-$_Aj0^ll%0uAOZ6"
				"m('J3*`4-6Rq@.*G7K,3L1O058b4-5mS4'5841'5%J/GV/1:B#'at%$q^DIM=X0DMg*^fL6)0/Ldbb(NRdimM-a4o7qPa>$cMDX:W<=&5t^Dv$)2mJ)lb+Ze`eHQ%:oSfLiMK/L@i6o7"
				":fKb@'x_5/Q1wPMIR0cMmbR`aRLb>-w..e-R3n0#bsn`$bA%%#N,>>#MhSM'Z`qdm?D,c4A]DD3mMWB#,5Rv$g&?a3?9wGMeANv>s#V&1iJ&%bE2ZA#-.^g1j;R)4443%b7RU`u`Kk(N"
				"HbeV@gJS'-BWcP3m?+#-VaDuL:`WY5kEaau^=lJ(_24J-WvW+.VxIfLXqd##Q3=&#d72mLvG(u$+dfF4<BPF%MPEb3:]Me.d(4I)(P;e.)_K#$^n;#MjXGp%jDJeM20P.)'9_-2[x):8"
				",VjfLQJa0V*#4o7dD24'^bO-)GX3/U@@%P'9/8b*;XmGA9Gw;8i=I`(sZhM'8A]?cof-6M>Awx-tS<GMRoS+MWWB@#WG9J'P@)?uNUYI8#-EE#[Yw_#;R,<8+b?:3=t$gLFf]0#G?O&#"
				"DoA*#_>QJ(PGpkL'(MB#/T01YP6;hLeb6lL7$(f):3ou-KHaJM[TXD#'1;p72$[p.Z9OA#RcB##Oi./LlA)ZuTBqn'B]7%b/WM<LrFcS7XOtILd9b<UxX'^#)J/Dt]il3++^tpAu_L%,"
				"w$[gu5[-['#**&+*wwGXO4h=#%H3'50S6##l9f/)m`):)t1@k=?\?#]u3i8-#%/5##)]$s$+JNh#jl###cU^F*Vs'Y$Lov[-0<a?$Hni?#+?2?up1m%.*%%-NPB`>$agNe'Qk[X'ep.'5"
				"8=B_A+L1_A'fp`Nae&%#aKb&#W>gkLZ/(p$V2Cv-_uv20tt?X-BL75/#(KU)N0;hLr75c4br9s-;va.3exLG`^U;4F9D+tqKKGSIULS:d=vRduSxXCuOq$0ufu'L#>Y[OVk1k3G(kZoA"
				"O9iQN'h5',b_mL,v?qr&4uG##%J/GV5J?`a'=@@M#w-tL0+xRV6,A48_fa-Zups+;(=rhZ;ktD#c9OA#axJ+*?7%s$DXI5/CKU:%eQ+,2=C587Rg;E4Z3f.*?ulW-tYqw0`EmS/si+C#"
				"[<;S&mInS%FAov#Fu[E+*L'O'GWuN'+)U40`a5k'sA;)*RIRF%Tw/Z--1=e?5;1X:;vPk&CdNjL*(KJ1/,TV-G(^S*v14gL#8,,M*YPgLaII@b+s[&#n+d3#1jk$#';P>#,Gc>#0Su>#"
				"4`1?#8lC?#<xU?#@.i?#D:%@#i'LVCn)fQD[.C(%ea@uBoF/ZGrDFVCjZ/NB0)61Fg&cF$v:7FHFDRb3bW<2Be(&ZG17O(Ie4;hFwf1eGEmqA4sS4VCoC%eG1PM*HsH%'I]MBnD+'],M"
				"w)n,Ga8q`EgKJ.#wZ/x=v`#/#";

			ImFontConfig icon_config;
			icon_config.MergeMode = true;
			icon_config.PixelSnapH = true;
			const ImWchar icon_ranges[] = { 0xF002, 0xF1C9, 0 };
			atlas->AddFontFromMemoryCompressedBase85TTF(icon_font_data, cfg.SizePixels, &icon_config, icon_ranges);
		}
	}

	// If unable to build font atlas due to an invalid font, revert to the default font
	if (!atlas->Build())
	{
		_font.clear();
		_editor_font.clear();

		atlas->Clear();

		for (unsigned int i = 0; i < 2; ++i)
		{
			ImFontConfig cfg;
			cfg.SizePixels = static_cast<float>(i == 0 ? _font_size : _editor_font_size);

			atlas->AddFontDefault(&cfg);
		}
	}

	_show_splash = true;
	_rebuild_font_atlas = false;

	int width, height;
	unsigned char *pixels;
	atlas->GetTexDataAsRGBA32(&pixels, &width, &height);

	// Create font atlas texture and upload it
	if (_imgui_font_atlas != nullptr)
		destroy_texture(*_imgui_font_atlas);
	if (_imgui_font_atlas == nullptr)
		_imgui_font_atlas = std::make_unique<texture>();

	_imgui_font_atlas->width = width;
	_imgui_font_atlas->height = height;
	_imgui_font_atlas->format = reshadefx::texture_format::rgba8;
	_imgui_font_atlas->unique_name = "ImGUI Font Atlas";
	if (init_texture(*_imgui_font_atlas))
		upload_texture(*_imgui_font_atlas, pixels);
	else
		_imgui_font_atlas.reset();
}

void reshade::runtime::load_custom_style()
{
	const ini_file &config = ini_file::load_cache(_config_path);

	ImVec4 *const colors = _imgui_context->Style.Colors;
	switch (_style_index)
	{
	case 0:
		ImGui::StyleColorsDark(&_imgui_context->Style);
		break;
	case 1:
		ImGui::StyleColorsLight(&_imgui_context->Style);
		break;
	case 2:
		colors[ImGuiCol_Text] = ImVec4(0.862745f, 0.862745f, 0.862745f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.58f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.117647f, 0.117647f, 0.117647f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 0.00f);
		colors[ImGuiCol_Border] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.30f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.470588f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.588235f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.45f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.35f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.58f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 0.57f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.31f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.78f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.117647f, 0.117647f, 0.117647f, 0.92f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.80f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.784314f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.44f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.86f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.76f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.86f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.32f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.78f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.862745f, 0.862745f, 0.862745f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.20f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.78f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_Tab] = colors[ImGuiCol_Button];
		colors[ImGuiCol_TabActive] = colors[ImGuiCol_ButtonActive];
		colors[ImGuiCol_TabHovered] = colors[ImGuiCol_ButtonHovered];
		colors[ImGuiCol_TabUnfocused] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
		colors[ImGuiCol_TabUnfocusedActive] = ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
		colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_Header] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
		colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.63f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.63f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.43f);
		break;
	case 5:
		colors[ImGuiCol_Text] = ImColor(0xff969483);
		colors[ImGuiCol_TextDisabled] = ImColor(0xff756e58);
		colors[ImGuiCol_WindowBg] = ImColor(0xff362b00);
		colors[ImGuiCol_ChildBg] = ImColor();
		colors[ImGuiCol_PopupBg] = ImColor(0xfc362b00); // Customized
		colors[ImGuiCol_Border] = ImColor(0xff423607);
		colors[ImGuiCol_BorderShadow] = ImColor();
		colors[ImGuiCol_FrameBg] = ImColor(0xfc423607); // Customized
		colors[ImGuiCol_FrameBgHovered] = ImColor(0xff423607);
		colors[ImGuiCol_FrameBgActive] = ImColor(0xff423607);
		colors[ImGuiCol_TitleBg] = ImColor(0xff362b00);
		colors[ImGuiCol_TitleBgActive] = ImColor(0xff362b00);
		colors[ImGuiCol_TitleBgCollapsed] = ImColor(0xff362b00);
		colors[ImGuiCol_MenuBarBg] = ImColor(0xff423607);
		colors[ImGuiCol_ScrollbarBg] = ImColor(0xff362b00);
		colors[ImGuiCol_ScrollbarGrab] = ImColor(0xff423607);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(0xff423607);
		colors[ImGuiCol_ScrollbarGrabActive] = ImColor(0xff423607);
		colors[ImGuiCol_CheckMark] = ImColor(0xff756e58);
		colors[ImGuiCol_SliderGrab] = ImColor(0xff5e5025); // Customized
		colors[ImGuiCol_SliderGrabActive] = ImColor(0xff5e5025); // Customized
		colors[ImGuiCol_Button] = ImColor(0xff423607);
		colors[ImGuiCol_ButtonHovered] = ImColor(0xff423607);
		colors[ImGuiCol_ButtonActive] = ImColor(0xff362b00);
		colors[ImGuiCol_Header] = ImColor(0xff423607);
		colors[ImGuiCol_HeaderHovered] = ImColor(0xff423607);
		colors[ImGuiCol_HeaderActive] = ImColor(0xff423607);
		colors[ImGuiCol_Separator] = ImColor(0xff423607);
		colors[ImGuiCol_SeparatorHovered] = ImColor(0xff423607);
		colors[ImGuiCol_SeparatorActive] = ImColor(0xff423607);
		colors[ImGuiCol_ResizeGrip] = ImColor(0xff423607);
		colors[ImGuiCol_ResizeGripHovered] = ImColor(0xff423607);
		colors[ImGuiCol_ResizeGripActive] = ImColor(0xff756e58);
		colors[ImGuiCol_Tab] = ImColor(0xff362b00);
		colors[ImGuiCol_TabHovered] = ImColor(0xff423607);
		colors[ImGuiCol_TabActive] = ImColor(0xff423607);
		colors[ImGuiCol_TabUnfocused] = ImColor(0xff362b00);
		colors[ImGuiCol_TabUnfocusedActive] = ImColor(0xff423607);
		colors[ImGuiCol_DockingPreview] = ImColor(0xee837b65); // Customized
		colors[ImGuiCol_DockingEmptyBg] = ImColor();
		colors[ImGuiCol_PlotLines] = ImColor(0xff756e58);
		colors[ImGuiCol_PlotLinesHovered] = ImColor(0xff756e58);
		colors[ImGuiCol_PlotHistogram] = ImColor(0xff756e58);
		colors[ImGuiCol_PlotHistogramHovered] = ImColor(0xff756e58);
		colors[ImGuiCol_TextSelectedBg] = ImColor(0xff756e58);
		colors[ImGuiCol_DragDropTarget] = ImColor(0xff756e58);
		colors[ImGuiCol_NavHighlight] = ImColor();
		colors[ImGuiCol_NavWindowingHighlight] = ImColor(0xee969483); // Customized
		colors[ImGuiCol_NavWindowingDimBg] = ImColor(0x20e3f6fd); // Customized
		colors[ImGuiCol_ModalWindowDimBg] = ImColor(0x20e3f6fd); // Customized
		break;
	case 6:
		colors[ImGuiCol_Text] = ImColor(0xff837b65);
		colors[ImGuiCol_TextDisabled] = ImColor(0xffa1a193);
		colors[ImGuiCol_WindowBg] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_ChildBg] = ImColor();
		colors[ImGuiCol_PopupBg] = ImColor(0xfce3f6fd); // Customized
		colors[ImGuiCol_Border] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_BorderShadow] = ImColor();
		colors[ImGuiCol_FrameBg] = ImColor(0xfcd5e8ee); // Customized
		colors[ImGuiCol_FrameBgHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_FrameBgActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_TitleBg] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_TitleBgActive] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_TitleBgCollapsed] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_MenuBarBg] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ScrollbarBg] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_ScrollbarGrab] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ScrollbarGrabActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_CheckMark] = ImColor(0xffa1a193);
		colors[ImGuiCol_SliderGrab] = ImColor(0xffc3d3d9); // Customized
		colors[ImGuiCol_SliderGrabActive] = ImColor(0xffc3d3d9); // Customized
		colors[ImGuiCol_Button] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ButtonHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ButtonActive] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_Header] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_HeaderHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_HeaderActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_Separator] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_SeparatorHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_SeparatorActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ResizeGrip] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ResizeGripHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ResizeGripActive] = ImColor(0xffa1a193);
		colors[ImGuiCol_Tab] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_TabHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_TabActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_TabUnfocused] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_TabUnfocusedActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_DockingPreview] = ImColor(0xeea1a193); // Customized
		colors[ImGuiCol_DockingEmptyBg] = ImColor();
		colors[ImGuiCol_PlotLines] = ImColor(0xffa1a193);
		colors[ImGuiCol_PlotLinesHovered] = ImColor(0xffa1a193);
		colors[ImGuiCol_PlotHistogram] = ImColor(0xffa1a193);
		colors[ImGuiCol_PlotHistogramHovered] = ImColor(0xffa1a193);
		colors[ImGuiCol_TextSelectedBg] = ImColor(0xffa1a193);
		colors[ImGuiCol_DragDropTarget] = ImColor(0xffa1a193);
		colors[ImGuiCol_NavHighlight] = ImColor();
		colors[ImGuiCol_NavWindowingHighlight] = ImColor(0xee837b65); // Customized
		colors[ImGuiCol_NavWindowingDimBg] = ImColor(0x20362b00); // Customized
		colors[ImGuiCol_ModalWindowDimBg] = ImColor(0x20362b00); // Customized
		break;
	default:
		for (ImGuiCol i = 0; i < ImGuiCol_COUNT; i++)
			config.get("STYLE", ImGui::GetStyleColorName(i), (float(&)[4])colors[i]);
		break;
	}

	switch (_editor_style_index)
	{
	case 0:
		_editor.set_palette({ // Dark
			0xffffffff, 0xffd69c56, 0xff00ff00, 0xff7070e0, 0xffffffff, 0xff409090, 0xffaaaaaa,
			0xff9bc64d, 0xffc040a0, 0xff206020, 0xff406020, 0xff101010, 0xffe0e0e0, 0x80a06020,
			0x800020ff, 0x8000ffff, 0xff707000, 0x40000000, 0x40808080, 0x40a0a0a0 });
		break;
	case 1:
		_editor.set_palette({ // Light
			0xff000000, 0xffff0c06, 0xff008000, 0xff2020a0, 0xff000000, 0xff409090, 0xff404040,
			0xff606010, 0xffc040a0, 0xff205020, 0xff405020, 0xffffffff, 0xff000000, 0x80600000,
			0xa00010ff, 0x8000ffff, 0xff505000, 0x40000000, 0x40808080, 0x40000000 });
		break;
	case 3:
		_editor.set_palette({ // Solarized Dark
			0xff969483, 0xff0089b5, 0xff98a12a, 0xff98a12a, 0xff969483, 0xff164bcb, 0xff969483,
			0xff969483, 0xffc4716c, 0xff756e58, 0xff756e58, 0xff362b00, 0xff969483, 0xA0756e58,
			0x7f2f32dc, 0x7f0089b5, 0xff756e58, 0x7f423607, 0x7f423607, 0x7f423607 });
		break;
	case 4:
		_editor.set_palette({ // Solarized Light
			0xff837b65, 0xff0089b5, 0xff98a12a, 0xff98a12a, 0xff756e58, 0xff164bcb, 0xff837b65,
			0xff837b65, 0xffc4716c, 0xffa1a193, 0xffa1a193, 0xffe3f6fd, 0xff837b65, 0x60a1a193,
			0x7f2f32dc, 0x7f0089b5, 0xffa1a193, 0x7fd5e8ee, 0x7fd5e8ee, 0x7fd5e8ee });
		break;
	default:
	case 2:
		ImVec4 value;
		for (ImGuiCol i = 0; i < code_editor::color_palette_max; i++)
			value = ImGui::ColorConvertU32ToFloat4(_editor.get_palette_index(i)), // Get default value first
			config.get("STYLE", code_editor::get_palette_color_name(i), (float(&)[4])value),
			_editor.get_palette_index(i) = ImGui::ColorConvertFloat4ToU32(value);
		break;
	}

	_viewer.set_palette(_editor.get_palette());
}
void reshade::runtime::save_custom_style()
{
	ini_file &config = ini_file::load_cache(_config_path);

	if (_style_index == 3 || _style_index == 4) // Custom Simple, Custom Advanced
	{
		for (ImGuiCol i = 0; i < ImGuiCol_COUNT; i++)
			config.set("STYLE", ImGui::GetStyleColorName(i), (const float(&)[4])_imgui_context->Style.Colors[i]);
	}

	if (_editor_style_index == 2) // Custom
	{
		ImVec4 value;
		for (ImGuiCol i = 0; i < code_editor::color_palette_max; i++)
			value = ImGui::ColorConvertU32ToFloat4(_editor.get_palette_index(i)),
			config.set("STYLE", code_editor::get_palette_color_name(i), (const float(&)[4])value);
	}
}

void reshade::runtime::draw_gui()
{
	assert(_is_initialized);

	const bool show_splash = _show_splash && (is_loading() || !_reload_compile_queue.empty() || (_reload_count <= 1 && (_last_present_time - _last_reload_time) < std::chrono::seconds(5)));
	// Do not show this message in the same frame the screenshot is taken (so that it won't show up on the UI screenshot)
	const bool show_screenshot_message = (_show_screenshot_message || !_screenshot_save_success) && !_should_save_screenshot && (_last_present_time - _last_screenshot_time) < std::chrono::seconds(_screenshot_save_success ? 3 : 5);

	if (_show_overlay && !_ignore_shortcuts && !_imgui_context->IO.NavVisible && _input->is_key_pressed(0x1B /* VK_ESCAPE */))
		_show_overlay = false; // Close when pressing the escape button and not currently navigating with the keyboard
	else if (!_ignore_shortcuts && _input->is_key_pressed(_overlay_key_data, _force_shortcut_modifiers) && _imgui_context->ActiveId == 0)
		_show_overlay = !_show_overlay;

	_ignore_shortcuts = false;
	_effects_expanded_state &= 2;

	if (_rebuild_font_atlas)
		build_font_atlas();
	if (_imgui_font_atlas == nullptr)
		return; // Cannot render UI without font atlas

	ImGui::SetCurrentContext(_imgui_context);
	auto &imgui_io = _imgui_context->IO;
	imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
	imgui_io.MouseDrawCursor = _show_overlay && (!_should_save_screenshot || !_screenshot_save_ui);
	imgui_io.MousePos.x = static_cast<float>(_input->mouse_position_x());
	imgui_io.MousePos.y = static_cast<float>(_input->mouse_position_y());
	imgui_io.DisplaySize.x = static_cast<float>(_width);
	imgui_io.DisplaySize.y = static_cast<float>(_height);
	imgui_io.Fonts->TexID = _imgui_font_atlas->impl;

	// Add wheel delta to the current absolute mouse wheel position
	imgui_io.MouseWheel += _input->mouse_wheel_delta();

	// Scale mouse position in case render resolution does not match the window size
	if (_window_width != 0 && _window_height != 0)
	{
		imgui_io.MousePos.x *= imgui_io.DisplaySize.x / _window_width;
		imgui_io.MousePos.y *= imgui_io.DisplaySize.y / _window_height;
	}

	// Update all the button states
	imgui_io.KeyAlt = _input->is_key_down(0x12); // VK_MENU
	imgui_io.KeyCtrl = _input->is_key_down(0x11); // VK_CONTROL
	imgui_io.KeyShift = _input->is_key_down(0x10); // VK_SHIFT
	for (unsigned int i = 0; i < 256; i++)
		imgui_io.KeysDown[i] = _input->is_key_down(i);
	for (unsigned int i = 0; i < 5; i++)
		imgui_io.MouseDown[i] = _input->is_mouse_button_down(i);
	for (wchar_t c : _input->text_input())
		imgui_io.AddInputCharacter(c);

	ImGui::NewFrame();

	ImVec2 viewport_offset = ImVec2(0, 0);

	// Create ImGui widgets and windows
	if (show_splash || show_screenshot_message || !_preset_save_success || (!_show_overlay && _tutorial_index == 0))
	{
		ImGui::SetNextWindowPos(ImVec2(10, 10));
		ImGui::SetNextWindowSize(ImVec2(imgui_io.DisplaySize.x - 20.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.862745f, 0.862745f, 0.862745f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.117647f, 0.117647f, 0.117647f, 0.7f));
		ImGui::Begin("Splash Screen", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoFocusOnAppearing);

		if (!_preset_save_success)
		{
			ImGui::TextColored(COLOR_RED, "Unable to save current preset. Make sure you have write permissions to %s.", _current_preset_path.u8string().c_str());
		}
		else if (show_screenshot_message)
		{
			if (!_screenshot_save_success)
				if (std::error_code ec; std::filesystem::exists(_screenshot_path, ec))
					ImGui::TextColored(COLOR_RED, "Unable to save screenshot because of an internal error (the format may not be supported).");
				else
					ImGui::TextColored(COLOR_RED, "Unable to save screenshot because path doesn't exist: %s.", _screenshot_path.u8string().c_str());
			else
				ImGui::Text("Screenshot successfully saved to %s", _last_screenshot_file.u8string().c_str());
		}
		else
		{
			ImGui::TextUnformatted("ReShade " VERSION_STRING_PRODUCT);

			if (_needs_update)
			{
				ImGui::TextColored(COLOR_YELLOW,
					"An update is available! Please visit https://reshade.me and install the new version (v%lu.%lu.%lu).",
					_latest_version[0], _latest_version[1], _latest_version[2]);
			}
			else
			{
				ImGui::TextUnformatted("Visit https://reshade.me for news, updates, shaders and discussion.");
			}

			ImGui::Spacing();

			if (_reload_remaining_effects != 0 && _reload_remaining_effects != std::numeric_limits<size_t>::max())
			{
				ImGui::ProgressBar((_effects.size() - _reload_remaining_effects) / float(_effects.size()), ImVec2(-1, 0), "");
				ImGui::SameLine(15);
				ImGui::Text(
					"Loading (%zu effects remaining) ... "
					"This might take a while. The application could become unresponsive for some time.",
					_reload_remaining_effects.load());
			}
			else if (!_reload_compile_queue.empty())
			{
				ImGui::ProgressBar((_effects.size() - _reload_compile_queue.size()) / float(_effects.size()), ImVec2(-1, 0), "");
				ImGui::SameLine(15);
				ImGui::Text(
					"Compiling (%zu effects remaining) ... "
					"This might take a while. The application could become unresponsive for some time.",
					_reload_compile_queue.size());
			}
			else if (_tutorial_index == 0)
			{
				ImGui::ProgressBar(0.0f, ImVec2(-1, 0), "");
				ImGui::SameLine(15);
				ImGui::TextUnformatted("ReShade is now installed successfully! Press '");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", input::key_name(_overlay_key_data).c_str());
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextUnformatted("' to start the tutorial.");
			}
			else
			{
				ImGui::ProgressBar(0.0f, ImVec2(-1, 0), "");
				ImGui::SameLine(15);
				ImGui::TextUnformatted("Press '");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", input::key_name(_overlay_key_data).c_str());
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextUnformatted("' to open the configuration overlay.");
			}

			if (!_last_shader_reload_successfull)
			{
				ImGui::Spacing();
				ImGui::TextColored(COLOR_RED,
					"There were errors compiling some shaders. Check the log for more details.");
			}
		}

		viewport_offset.y += ImGui::GetWindowHeight() + 10; // Add small space between windows

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}
	else if (_show_clock || _show_fps || _show_frametime)
	{
		float window_height = _imgui_context->FontBaseSize * _fps_scale + _imgui_context->Style.ItemSpacing.y;
		window_height *= (_show_clock ? 1 : 0) + (_show_fps ? 1 : 0) + (_show_frametime ? 1 : 0);
		window_height += _imgui_context->Style.FramePadding.y * 4.0f;

		ImVec2 fps_window_pos(5, 5);
		if (_fps_pos % 2)
			fps_window_pos.x = imgui_io.DisplaySize.x - 200.0f;
		if (_fps_pos > 1)
			fps_window_pos.y = imgui_io.DisplaySize.y - window_height - 5;

		ImGui::SetNextWindowPos(fps_window_pos);
		ImGui::SetNextWindowSize(ImVec2(200.0f, window_height));
		ImGui::PushStyleColor(ImGuiCol_Text, (const ImVec4 &)_fps_col);
		ImGui::Begin("FPS", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBackground);

		ImGui::SetWindowFontScale(_fps_scale);

		char temp[512];

		if (_show_clock)
		{
			const int hour = _date[3] / 3600;
			const int minute = (_date[3] - hour * 3600) / 60;
			const int seconds = _date[3] - hour * 3600 - minute * 60;

			ImFormatString(temp, sizeof(temp), _clock_format != 0 ? "%02u:%02u:%02u" : "%02u:%02u", hour, minute, seconds);
			if (_fps_pos % 2) // Align text to the right of the window
				ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_fps)
		{
			ImFormatString(temp, sizeof(temp), "%.0f fps", imgui_io.Framerate);
			if (_fps_pos % 2)
				ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_frametime)
		{
			ImFormatString(temp, sizeof(temp), "%5.2f ms", 1000.0f / imgui_io.Framerate);
			if (_fps_pos % 2)
				ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}

		ImGui::End();
		ImGui::PopStyleColor();
	}

	if (_show_overlay)
	{
		// Change font size if user presses the control key and moves the mouse wheel
		if (imgui_io.KeyCtrl && imgui_io.MouseWheel != 0 && !_no_font_scaling)
		{
			_font_size = ImClamp(_font_size + static_cast<int>(imgui_io.MouseWheel), 8, 32);
			_editor_font_size = ImClamp(_editor_font_size + static_cast<int>(imgui_io.MouseWheel), 8, 32);
			_rebuild_font_atlas = true;
			save_config();
		}

		const ImGuiID root_space_id = ImGui::GetID("Dockspace");
		const ImGuiViewport *const viewport = ImGui::GetMainViewport();

		// Set up default dock layout if this was not done yet
		const bool init_window_layout = !ImGui::DockBuilderGetNode(root_space_id);
		if (init_window_layout)
		{
			// Add the root node
			ImGui::DockBuilderAddNode(root_space_id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(root_space_id, viewport->Size);

			// Split root node into two spaces
			ImGuiID main_space_id = 0;
			ImGuiID right_space_id = 0;
			ImGui::DockBuilderSplitNode(root_space_id, ImGuiDir_Left, 0.35f, &main_space_id, &right_space_id);

			// Attach most windows to the main dock space
			for (const auto &widget : _menu_callables)
				ImGui::DockBuilderDockWindow(widget.first.c_str(), main_space_id);

			// Attach editor window to the remaining dock space
			ImGui::DockBuilderDockWindow("###editor", right_space_id);
			ImGui::DockBuilderDockWindow("###viewer", right_space_id);

			// Commit the layout
			ImGui::DockBuilderFinish(root_space_id);
		}

		ImGui::SetNextWindowPos(viewport->Pos + viewport_offset);
		ImGui::SetNextWindowSize(viewport->Size - viewport_offset);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::Begin("Viewport", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoDocking | // This is the background viewport, the docking space is a child of it
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoBackground);
		ImGui::DockSpace(root_space_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::End();

		for (const auto &widget : _menu_callables)
		{
			if (ImGui::Begin(widget.first.c_str(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) // No focus so that window state is preserved between opening/closing the UI
				widget.second();
			ImGui::End();
		}

		if (_show_code_editor)
		{
			const std::string title = "Editing " + _editor_file.filename().u8string() + " ###editor";
			if (ImGui::Begin(title.c_str(), &_show_code_editor, _editor.is_modified() ? ImGuiWindowFlags_UnsavedDocument : 0))
				draw_code_editor();
			ImGui::End();
		}
		if (_show_code_viewer)
		{
			if (ImGui::Begin("Viewing generated code###viewer", &_show_code_viewer))
				draw_code_viewer();
			ImGui::End();
		}
	}

	if (_preview_texture != nullptr && _effects_enabled)
	{
		if (!_show_overlay)
		{
			// Create a temporary viewport window to attach image to when overlay is not open
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2(imgui_io.DisplaySize.x, imgui_io.DisplaySize.y));
			ImGui::Begin("Viewport", nullptr,
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoNav |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoDocking |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoBackground);
			ImGui::End();
		}

		// The preview texture is unset in 'unload_effects', so should not be able to reach this while loading
		assert(!is_loading() && _reload_compile_queue.empty());

		// Scale image to fill the entire viewport by default
		ImVec2 preview_min = ImVec2(0, 0);
		ImVec2 preview_max = imgui_io.DisplaySize;

		// Positing image in the middle of the viewport when using original size
		if (_preview_size[0])
		{
			preview_min.x = (preview_max.x * 0.5f) - (_preview_size[0] * 0.5f);
			preview_max.x = (preview_max.x * 0.5f) + (_preview_size[0] * 0.5f);
		}
		if (_preview_size[1])
		{
			preview_min.y = (preview_max.y * 0.5f) - (_preview_size[1] * 0.5f);
			preview_max.y = (preview_max.y * 0.5f) + (_preview_size[1] * 0.5f);
		}

		ImGui::FindWindowByName("Viewport")->DrawList->AddImage(_preview_texture, preview_min, preview_max, ImVec2(0, 0), ImVec2(1, 1), _preview_size[2]);
	}

	// Disable keyboard shortcuts while typing into input boxes
	_ignore_shortcuts |= ImGui::IsAnyItemActive();

	// Render ImGui widgets and windows
	ImGui::Render();

	_input->block_mouse_input(_input_processing_mode != 0 && _show_overlay && (imgui_io.WantCaptureMouse || _input_processing_mode == 2));
	_input->block_keyboard_input(_input_processing_mode != 0 && _show_overlay && (imgui_io.WantCaptureKeyboard || _input_processing_mode == 2));

	if (_input->is_blocking_mouse_input())
	{
		// Some games setup ClipCursor with a tiny area which could make the cursor stay in that area instead of the whole window
		ClipCursor(nullptr);
	}
	
	if (ImDrawData *const draw_data = ImGui::GetDrawData();
		draw_data != nullptr && draw_data->CmdListsCount != 0 && draw_data->TotalVtxCount != 0)
		render_imgui_draw_data(draw_data);
}

void reshade::runtime::draw_gui_home()
{
	const char *tutorial_text =
		"Welcome! Since this is the first time you start ReShade, we'll go through a quick tutorial covering the most important features.\n\n"
		"If you have difficulties reading this text, press the 'Ctrl' key and adjust the font size with your mouse wheel. "
		"The window size is variable as well, just grab the right edge and move it around.\n\n"
		"You can also use the keyboard for navigation in case mouse input does not work. Use the arrow keys to navigate, space bar to confirm an action or enter a control and the 'Esc' key to leave a control. "
		"Press 'Ctrl + Tab' to switch between tabs and windows (use this to focus this page in case the other navigation keys do not work at first).\n\n"
		"Click on the 'Continue' button to continue the tutorial.";

	// It is not possible to follow some of the tutorial steps while performance mode is active, so skip them
	if (_performance_mode && _tutorial_index <= 3)
		_tutorial_index = 4;

	if (_tutorial_index > 0)
	{
		if (_tutorial_index == 1)
		{
			tutorial_text =
				"This is the preset selection. All changes will be saved to the selected preset file.\n\n"
				"Click on the '+' button to name and add a new one.\n\n"
				"Make sure you always have a preset selected here before starting to tweak any values later, or else your changes won't be saved!";

			ImGui::PushStyleColor(ImGuiCol_FrameBg, COLOR_RED);
			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_RED);
		}

		const float button_size = ImGui::GetFrameHeight();
		const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;
		const float browse_button_width = ImGui::GetWindowContentRegionWidth() - (button_spacing + button_size) * 3;

		bool reload_preset = false;

		ImGuiButtonFlags button_flags = ImGuiButtonFlags_NoNavFocus;
		if (is_loading())
		{
			button_flags |= ImGuiButtonFlags_Disabled;
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		}

		if (ImGui::ButtonEx("<", ImVec2(button_size, 0), button_flags))
			if (switch_to_next_preset(_current_preset_path.parent_path(), true))
				reload_preset = true;
		ImGui::SameLine(0, button_spacing);
		if (ImGui::ButtonEx(">", ImVec2(button_size, 0), button_flags))
			if (switch_to_next_preset(_current_preset_path.parent_path(), false))
				reload_preset = true;

		ImGui::SameLine(0, button_spacing);
		const ImVec2 popup_pos = ImGui::GetCursorScreenPos() + ImVec2(-_imgui_context->Style.WindowPadding.x, ImGui::GetFrameHeightWithSpacing());

		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
		if (ImGui::ButtonEx(_current_preset_path.stem().u8string().c_str(), ImVec2(browse_button_width, 0), button_flags))
		{
			_file_selection_path = _current_preset_path;
			ImGui::OpenPopup("##browse");
		}
		ImGui::PopStyleVar();

		ImGui::SameLine(0, button_spacing);
		if (ImGui::ButtonEx("+", ImVec2(button_size, 0), button_flags | ImGuiButtonFlags_PressedOnClick))
		{
			_file_selection_path = _current_preset_path.parent_path();
			ImGui::OpenPopup("##create");
		}

		if (is_loading())
			ImGui::PopStyleColor();

		ImGui::SetNextWindowPos(popup_pos);
		if (widgets::file_dialog("##browse", _file_selection_path, browse_button_width, { L".ini", L".txt" }))
		{
			// Check that this is actually a valid preset file
			if (ini_file::load_cache(_file_selection_path).has("", "Techniques"))
			{
				reload_preset = true;
				_current_preset_path = _file_selection_path;
			}
		}

		if (ImGui::BeginPopup("##create"))
		{
			ImGui::Checkbox("Duplicate current preset", &_duplicate_current_preset);

			char preset_name[260] = "";
			if (ImGui::InputText("Name", preset_name, sizeof(preset_name), ImGuiInputTextFlags_EnterReturnsTrue) && preset_name[0] != '\0')
			{
				std::filesystem::path new_preset_path = _file_selection_path / std::filesystem::u8path(preset_name);
				if (new_preset_path.extension() != L".ini" && new_preset_path.extension() != L".txt")
					new_preset_path += L".ini";

				std::error_code ec;
				const std::filesystem::file_type file_type = std::filesystem::status(new_preset_path, ec).type();
				if (file_type != std::filesystem::file_type::directory)
				{
					reload_preset =
						file_type == std::filesystem::file_type::not_found ||
						ini_file::load_cache(new_preset_path).has("", "Techniques");

					if (_duplicate_current_preset && file_type == std::filesystem::file_type::not_found)
						CopyFileW(_current_preset_path.c_str(), new_preset_path.c_str(), TRUE);
				}

				if (reload_preset)
				{
					ImGui::CloseCurrentPopup();
					_current_preset_path = new_preset_path;
				}
				else
				{
					ImGui::SetKeyboardFocusHere();
				}
			}

			if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere();

			if (preset_name[0] == '\0' && ImGui::IsKeyPressedMap(ImGuiKey_Backspace))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		if (reload_preset)
		{
			_show_splash = true;

			save_config();
			load_current_preset();
		}

		if (_tutorial_index == 1)
			ImGui::PopStyleColor(2);
	}

	if (_tutorial_index > 1)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}

	if (is_loading())
	{
		const char *const loading_message = ICON_REFRESH " Loading ... ";
		ImGui::SetCursorPos((ImGui::GetWindowSize() - ImGui::CalcTextSize(loading_message)) * 0.5f);
		ImGui::TextUnformatted(loading_message);
		return; // Cannot show techniques and variables while effects are loading, since they are being modified in other different threads during that time
	}

	if (!_effects_enabled)
		ImGui::Text("Effects are disabled. Press '%s' to enable them again.", input::key_name(_effects_key_data).c_str());

	if (!_last_shader_reload_successfull)
	{
		std::string shader_list;
		for (const effect &effect : _effects)
			if (!effect.compiled)
				shader_list += ' ' + effect.source_file.filename().u8string() + ',';

		// Make sure there are actually effects that failed to compile, since the last reload flag may not have been reset
		if (shader_list.empty())
		{
			_last_shader_reload_successfull = true;
		}
		else
		{
			shader_list.pop_back();
			ImGui::TextColored(COLOR_RED, "Some shaders failed to compile:%s", shader_list.c_str());
			ImGui::Spacing();
		}
	}
	if (!_last_texture_reload_successfull)
	{
		std::string texture_list;
		for (const texture &tex : _textures)
			if (tex.impl != nullptr && !tex.loaded && !tex.annotation_as_string("source").empty())
				texture_list += ' ' + tex.unique_name + ',';

		if (texture_list.empty())
		{
			_last_texture_reload_successfull = true;
		}
		else
		{
			texture_list.pop_back();
			ImGui::TextColored(COLOR_RED, "Some textures failed to load:%s", texture_list.c_str());
			ImGui::Spacing();
		}
	}

	if (_tutorial_index > 1)
	{
		const bool show_clear_button = _effect_filter[0] != '\0';

		if (ImGui::InputTextEx("##filter", "Search " ICON_SEARCH, _effect_filter, sizeof(_effect_filter),
			ImVec2((_variable_editor_tabs ? -10.0f : -20.0f) * _font_size - (show_clear_button ? ImGui::GetFrameHeight() + _imgui_context->Style.ItemSpacing.x : 0), 0), ImGuiInputTextFlags_AutoSelectAll))
		{
			_effects_expanded_state = 3;
			const std::string_view filter_view = _effect_filter;

			for (technique &technique : _techniques)
			{
				std::string_view label = technique.annotation_as_string("ui_label");
				if (label.empty())
					label = technique.name;

				technique.hidden = technique.annotation_as_int("hidden") != 0 || (
					!filter_view.empty() && // Reset visibility state if filter is empty
					std::search(label.begin(), label.end(), filter_view.begin(), filter_view.end(), // Search case insensitive
						[](const char c1, const char c2) { return (('a' <= c1 && c1 <= 'z') ? static_cast<char>(c1 - ' ') : c1) == (('a' <= c2 && c2 <= 'z') ? static_cast<char>(c2 - ' ') : c2); }) == label.end());
			}
		}

		ImGui::SameLine();

		if (show_clear_button && ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0)))
		{
			_effect_filter[0] = '\0';

			// Reset visibility state of all techniques since no filter is active anymore
			for (technique &technique : _techniques)
				technique.hidden = technique.annotation_as_int("hidden") != 0;
		}

		ImGui::SameLine();

		if (ImGui::Button("Active to top", ImVec2(10 * _font_size - _imgui_context->Style.ItemSpacing.x, 0)))
		{
			for (auto i = _techniques.begin(); i != _techniques.end(); ++i)
			{
				if (!i->enabled && i->toggle_key_data[0] == 0)
				{
					for (auto k = i + 1; k != _techniques.end(); ++k)
					{
						if (k->enabled || k->toggle_key_data[0] != 0)
						{
							std::iter_swap(i, k);
							break;
						}
					}
				}
			}

			if (const auto it = std::find_if_not(_techniques.begin(), _techniques.end(),
				[](const reshade::technique &a) { return a.enabled || a.toggle_key_data[0] != 0; }); it != _techniques.end())
			{
				std::stable_sort(it, _techniques.end(), [](const reshade::technique &lhs, const reshade::technique &rhs) {
						std::string lhs_label(lhs.annotation_as_string("ui_label"));
						if (lhs_label.empty()) lhs_label = lhs.name;
						std::transform(lhs_label.begin(), lhs_label.end(), lhs_label.begin(), [](char c) { return static_cast<char>(toupper(c)); });
						std::string rhs_label(rhs.annotation_as_string("ui_label"));
						if (rhs_label.empty()) rhs_label = rhs.name;
						std::transform(rhs_label.begin(), rhs_label.end(), rhs_label.begin(), [](char c) { return static_cast<char>(toupper(c)); });
						return lhs_label < rhs_label;
					});
			}

			save_current_preset();
		}

		ImGui::SameLine();

		if (ImGui::Button((_effects_expanded_state & 2) ? "Collapse all" : "Expand all", ImVec2(10 * _font_size - _imgui_context->Style.ItemSpacing.x, 0)))
			_effects_expanded_state = (~_effects_expanded_state & 2) | 1;

		if (_tutorial_index == 2)
		{
			tutorial_text =
				"This is the list of effects. It contains all techniques found in the effect files (*.fx) from the effect search paths as specified in the settings.\n\n"
				"Enter text in the box at the top to filter it and search for specific techniques.\n\n"
				"Click on a technique to enable or disable it or drag it to a new location in the list to change the order in which the effects are applied.\n"
				"Use the right mouse button and click on an item to open the context menu with additional options.\n\n";

			ImGui::PushStyleColor(ImGuiCol_Border, COLOR_RED);
		}

		ImGui::Spacing();

		const float bottom_height = _performance_mode ? ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y : (_variable_editor_height + (_tutorial_index == 3 ? 175 : 0));

		if (ImGui::BeginChild("##techniques", ImVec2(0, -bottom_height), true))
			draw_technique_editor();
		ImGui::EndChild();

		if (_tutorial_index == 2)
			ImGui::PopStyleColor();
	}

	if (_tutorial_index > 2 && !_performance_mode)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::ButtonEx("##splitter", ImVec2(ImGui::GetContentRegionAvail().x, 5));
		ImGui::PopStyleVar();

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		if (ImGui::IsItemActive())
		{
			_variable_editor_height -= _imgui_context->IO.MouseDelta.y;
			save_config();
		}

		if (_tutorial_index == 3)
		{
			tutorial_text =
				"This is the list of variables. It contains all tweakable options the active effects expose. Values here apply in real-time.\n\n"
				"Enter text in the box at the top to filter it and search for specific variables.\n\n"
				"Press 'Ctrl' and click on a widget to manually edit the value.\n"
				"Use the right mouse button and click on an item to open the context menu with additional options.\n\n"
				"Once you have finished tweaking your preset, be sure to enable the 'Performance Mode' check box. "
				"This will recompile all shaders into a more optimal representation that can give a performance boost, but will disable variable tweaking and this list.";

			ImGui::PushStyleColor(ImGuiCol_Border, COLOR_RED);
		}

		const float bottom_height = ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y + (_tutorial_index == 3 ? 175 : 0);

		if (ImGui::BeginChild("##variables", ImVec2(0, -bottom_height), true))
			draw_variable_editor();
		ImGui::EndChild();

		if (_tutorial_index == 3)
			ImGui::PopStyleColor();
	}

	if (_tutorial_index > 3)
	{
		ImGui::Spacing();

		if (ImGui::Button(ICON_REFRESH " Reload", ImVec2(-11.5f * _font_size, 0)))
		{
			load_effects();
		}

		ImGui::SameLine();

		if (ImGui::Checkbox("Performance Mode", &_performance_mode))
		{
			save_config();
			load_effects(); // Reload effects after switching
		}
	}
	else
	{
		ImGui::BeginChildFrame(ImGui::GetID("tutorial"), ImVec2(0, 175));
		ImGui::TextWrapped(tutorial_text);
		ImGui::EndChildFrame();

		const float max_button_width = ImGui::GetContentRegionAvail().x;

		if (_tutorial_index == 0)
		{
			if (ImGui::Button("Continue", ImVec2(max_button_width * 0.66666666f, 0)))
			{
				_tutorial_index++;

				save_config();
			}

			ImGui::SameLine();

			if (ImGui::Button("Skip Tutorial", ImVec2(max_button_width * 0.33333333f - _imgui_context->Style.ItemSpacing.x, 0)))
			{
				_tutorial_index = 4;
				_no_font_scaling = true;

				save_config();
			}
		}
		else
		{
			if (ImGui::Button(_tutorial_index == 3 ? "Finish" : "Continue", ImVec2(max_button_width, 0)))
			{
				_tutorial_index++;

				if (_tutorial_index == 4)
				{
					// Disable font scaling after tutorial
					_no_font_scaling = true;

					save_config();
				}
			}
		}
	}
}
void reshade::runtime::draw_gui_settings()
{
	bool modified = false;
	bool modified_custom_style = false;

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= widgets::key_input_box("Overlay key", _overlay_key_data, *_input);

		modified |= widgets::key_input_box("Effect toggle key", _effects_key_data, *_input);
		modified |= widgets::key_input_box("Effect reload key", _reload_key_data, *_input);

		modified |= widgets::key_input_box("Performance mode toggle key", _performance_mode_key_data, *_input);

		const float inner_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		ImGui::PushItemWidth((ImGui::CalcItemWidth() - inner_spacing) / 2);
		modified |= widgets::key_input_box("##prev_preset_key", _prev_preset_key_data, *_input);
		ImGui::SameLine(0, inner_spacing);
		modified |= widgets::key_input_box("##next_preset_key", _next_preset_key_data, *_input);
		ImGui::PopItemWidth();
		ImGui::SameLine(0, inner_spacing);
		ImGui::TextUnformatted("Preset switching keys");

		modified |= ImGui::SliderInt("Preset transition delay", reinterpret_cast<int *>(&_preset_transition_delay), 0, 10 * 1000);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Makes a smooth transition, but only for floating point values.\nRecommended for multiple presets that contain the same shaders, otherwise set this to zero.\nValues are in milliseconds.");

		modified |= ImGui::Combo("Input processing", &_input_processing_mode,
			"Pass on all input\0"
			"Block input when cursor is on overlay\0"
			"Block all input when overlay is visible\0");

		ImGui::Spacing();

		modified |= widgets::path_list("Effect search paths", _effect_search_paths, _file_selection_path, g_reshade_base_path);
		modified |= widgets::path_list("Texture search paths", _texture_search_paths, _file_selection_path, g_reshade_base_path);

		modified |= ImGui::Checkbox("Load only enabled effects", &_effect_load_skipping);

		if (ImGui::Button("Clear effect cache", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
			// Find all cached effect files and delete them
			std::error_code ec;
			for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(g_reshade_base_path / _intermediate_cache_path, std::filesystem::directory_options::skip_permission_denied, ec))
			{
				if (entry.is_directory(ec))
					continue;

				const std::filesystem::path filename = entry.path().filename();
				const std::filesystem::path extension = entry.path().extension();
				if (filename.native().compare(0, 8, L"reshade-") != 0 || (extension != ".i" && extension != ".cso" && extension != ".asm"))
					continue;

				DeleteFileW(entry.path().c_str());
			}
		}
	}

	if (ImGui::CollapsingHeader("Screenshots", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= widgets::key_input_box("Screenshot key", _screenshot_key_data, *_input);
		modified |= widgets::directory_input_box("Screenshot path", _screenshot_path, _file_selection_path);

		const int hour = _date[3] / 3600;
		const int minute = (_date[3] - hour * 3600) / 60;
		const int seconds = _date[3] - hour * 3600 - minute * 60;

		char timestamp[21];
		sprintf_s(timestamp, " %.4d-%.2d-%.2d %.2d-%.2d-%.2d", _date[0], _date[1], _date[2], hour, minute, seconds);

		std::string screenshot_naming_items;
		screenshot_naming_items += g_target_executable_path.stem().string() + timestamp + '\0';
		screenshot_naming_items += g_target_executable_path.stem().string() + timestamp + ' ' + _current_preset_path.stem().string() + '\0';
		modified |= ImGui::Combo("Screenshot name", reinterpret_cast<int *>(&_screenshot_naming), screenshot_naming_items.c_str());

		modified |= ImGui::Combo("Screenshot format", reinterpret_cast<int *>(&_screenshot_format), "Bitmap (*.bmp)\0Portable Network Graphics (*.png)\0JPEG (*.jpeg)\0");

		if (_screenshot_format == 2)
			modified |= ImGui::SliderInt("JPEG quality", reinterpret_cast<int *>(&_screenshot_jpeg_quality), 1, 100);
		else
			modified |= ImGui::Checkbox("Clear alpha channel", &_screenshot_clear_alpha);

		modified |= ImGui::Checkbox("Save current preset file", &_screenshot_include_preset);
		modified |= ImGui::Checkbox("Save before and after images", &_screenshot_save_before);
		modified |= ImGui::Checkbox("Save separate image with the overlay visible", &_screenshot_save_ui);
	}

	if (ImGui::CollapsingHeader("Overlay & Styling", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Button("Restart tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			_tutorial_index = 0;

		modified |= ImGui::Checkbox("Show screenshot message", &_show_screenshot_message);

		if (_effect_load_skipping)
			modified |= ImGui::Checkbox("Show \"Force load all effects\" button", &_show_force_load_effects_button);

		bool save_imgui_window_state = _imgui_context->IO.IniFilename != nullptr;
		if (ImGui::Checkbox("Save window state (ReShadeGUI.ini)", &save_imgui_window_state))
		{
			modified = true;
			_imgui_context->IO.IniFilename = save_imgui_window_state ? g_window_state_path.c_str() : nullptr;
		}

		modified |= ImGui::Checkbox("Group effect files with tabs instead of a tree", &_variable_editor_tabs);

		#pragma region Style
		if (ImGui::Combo("Global style", &_style_index, "Dark\0Light\0Default\0Custom Simple\0Custom Advanced\0Solarized Dark\0Solarized Light\0"))
		{
			modified = true;
			load_custom_style();
		}

		if (_style_index == 3) // Custom Simple
		{
			ImVec4 *const colors = _imgui_context->Style.Colors;

			if (ImGui::BeginChild("##colors", ImVec2(0, 105), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				modified_custom_style |= ImGui::ColorEdit3("Background", &colors[ImGuiCol_WindowBg].x);
				modified_custom_style |= ImGui::ColorEdit3("ItemBackground", &colors[ImGuiCol_FrameBg].x);
				modified_custom_style |= ImGui::ColorEdit3("Text", &colors[ImGuiCol_Text].x);
				modified_custom_style |= ImGui::ColorEdit3("ActiveItem", &colors[ImGuiCol_ButtonActive].x);
				ImGui::PopItemWidth();
			} ImGui::EndChild();

			// Change all colors using the above as base
			if (modified_custom_style)
			{
				colors[ImGuiCol_PopupBg] = colors[ImGuiCol_WindowBg]; colors[ImGuiCol_PopupBg].w = 0.92f;

				colors[ImGuiCol_ChildBg] = colors[ImGuiCol_FrameBg]; colors[ImGuiCol_ChildBg].w = 0.00f;
				colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_FrameBg]; colors[ImGuiCol_MenuBarBg].w = 0.57f;
				colors[ImGuiCol_ScrollbarBg] = colors[ImGuiCol_FrameBg]; colors[ImGuiCol_ScrollbarBg].w = 1.00f;

				colors[ImGuiCol_TextDisabled] = colors[ImGuiCol_Text]; colors[ImGuiCol_TextDisabled].w = 0.58f;
				colors[ImGuiCol_Border] = colors[ImGuiCol_Text]; colors[ImGuiCol_Border].w = 0.30f;
				colors[ImGuiCol_Separator] = colors[ImGuiCol_Text]; colors[ImGuiCol_Separator].w = 0.32f;
				colors[ImGuiCol_SeparatorHovered] = colors[ImGuiCol_Text]; colors[ImGuiCol_SeparatorHovered].w = 0.78f;
				colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_Text]; colors[ImGuiCol_SeparatorActive].w = 1.00f;
				colors[ImGuiCol_PlotLines] = colors[ImGuiCol_Text]; colors[ImGuiCol_PlotLines].w = 0.63f;
				colors[ImGuiCol_PlotHistogram] = colors[ImGuiCol_Text]; colors[ImGuiCol_PlotHistogram].w = 0.63f;

				colors[ImGuiCol_FrameBgHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_FrameBgHovered].w = 0.68f;
				colors[ImGuiCol_FrameBgActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_FrameBgActive].w = 1.00f;
				colors[ImGuiCol_TitleBg] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_TitleBg].w = 0.45f;
				colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_TitleBgCollapsed].w = 0.35f;
				colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_TitleBgActive].w = 0.58f;
				colors[ImGuiCol_ScrollbarGrab] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ScrollbarGrab].w = 0.31f;
				colors[ImGuiCol_ScrollbarGrabHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ScrollbarGrabHovered].w = 0.78f;
				colors[ImGuiCol_ScrollbarGrabActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ScrollbarGrabActive].w = 1.00f;
				colors[ImGuiCol_CheckMark] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_CheckMark].w = 0.80f;
				colors[ImGuiCol_SliderGrab] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_SliderGrab].w = 0.24f;
				colors[ImGuiCol_SliderGrabActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_SliderGrabActive].w = 1.00f;
				colors[ImGuiCol_Button] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_Button].w = 0.44f;
				colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ButtonHovered].w = 0.86f;
				colors[ImGuiCol_Header] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_Header].w = 0.76f;
				colors[ImGuiCol_HeaderHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_HeaderHovered].w = 0.86f;
				colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_HeaderActive].w = 1.00f;
				colors[ImGuiCol_ResizeGrip] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ResizeGrip].w = 0.20f;
				colors[ImGuiCol_ResizeGripHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ResizeGripHovered].w = 0.78f;
				colors[ImGuiCol_ResizeGripActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ResizeGripActive].w = 1.00f;
				colors[ImGuiCol_PlotLinesHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_PlotLinesHovered].w = 1.00f;
				colors[ImGuiCol_PlotHistogramHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_PlotHistogramHovered].w = 1.00f;
				colors[ImGuiCol_TextSelectedBg] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_TextSelectedBg].w = 0.43f;

				colors[ImGuiCol_Tab] = colors[ImGuiCol_Button];
				colors[ImGuiCol_TabActive] = colors[ImGuiCol_ButtonActive];
				colors[ImGuiCol_TabHovered] = colors[ImGuiCol_ButtonHovered];
				colors[ImGuiCol_TabUnfocused] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
				colors[ImGuiCol_TabUnfocusedActive] = ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
				colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_Header] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
				colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
			}
		}
		if (_style_index == 4) // Custom Advanced
		{
			if (ImGui::BeginChild("##colors", ImVec2(0, 300), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				for (ImGuiCol i = 0; i < ImGuiCol_COUNT; i++)
				{
					ImGui::PushID(i);
					modified_custom_style |= ImGui::ColorEdit4("##color", &_imgui_context->Style.Colors[i].x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
					ImGui::SameLine(); ImGui::TextUnformatted(ImGui::GetStyleColorName(i));
					ImGui::PopID();
				}
				ImGui::PopItemWidth();
			} ImGui::EndChild();
		}
		#pragma endregion

		#pragma region Editor Style
		if (ImGui::Combo("Text editor style", &_editor_style_index, "Dark\0Light\0Custom\0Solarized Dark\0Solarized Light\0"))
		{
			modified = true;
			load_custom_style();
		}

		if (_editor_style_index == 2)
		{
			if (ImGui::BeginChild("##editor_colors", ImVec2(0, 300), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				for (ImGuiCol i = 0; i < code_editor::color_palette_max; i++)
				{
					ImVec4 color = ImGui::ColorConvertU32ToFloat4(_editor.get_palette_index(i));
					ImGui::PushID(i);
					modified_custom_style |= ImGui::ColorEdit4("##editor_color", &color.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
					ImGui::SameLine(); ImGui::TextUnformatted(code_editor::get_palette_color_name(i));
					ImGui::PopID();
					_viewer.get_palette_index(i) = _editor.get_palette_index(i) = ImGui::ColorConvertFloat4ToU32(color);
				}
				ImGui::PopItemWidth();
			} ImGui::EndChild();
		}
		#pragma endregion

		if (widgets::font_input_box("Global font", _font, _file_selection_path, _font_size))
		{
			modified = true;
			_rebuild_font_atlas = true;
		}

		if (widgets::font_input_box("Text editor font", _editor_font, _file_selection_path, _editor_font_size))
		{
			modified = true;
			_rebuild_font_atlas = true;
		}

		if (float &alpha = _imgui_context->Style.Alpha; ImGui::SliderFloat("Global alpha", &alpha, 0.1f, 1.0f, "%.2f"))
		{
			// Prevent user from setting alpha to zero
			alpha = std::max(alpha, 0.1f);
			modified = true;
		}

		if (float &rounding = _imgui_context->Style.FrameRounding; ImGui::SliderFloat("Frame rounding", &rounding, 0.0f, 12.0f, "%.0f"))
		{
			// Apply the same rounding to everything
			_imgui_context->Style.GrabRounding = _imgui_context->Style.TabRounding = _imgui_context->Style.ScrollbarRounding = rounding;
			_imgui_context->Style.WindowRounding = _imgui_context->Style.ChildRounding = _imgui_context->Style.PopupRounding = rounding;
			modified = true;
		}

		modified |= ImGui::Checkbox("Show clock", &_show_clock);
		ImGui::SameLine(0, 10); modified |= ImGui::Checkbox("Show FPS", &_show_fps);
		ImGui::SameLine(0, 10); modified |= ImGui::Checkbox("Show frame time", &_show_frametime);
		modified |= ImGui::Combo("Clock format", &_clock_format, "HH:MM\0HH:MM:SS\0");
		modified |= ImGui::SliderFloat("FPS text size", &_fps_scale, 0.2f, 2.5f, "%.1f");
		modified |= ImGui::ColorEdit4("FPS text color", _fps_col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
		modified |= ImGui::Combo("Position on screen", &_fps_pos, "Top Left\0Top Right\0Bottom Left\0Bottom Right\0");
	}

	if (modified)
		save_config();
	if (modified_custom_style)
		save_custom_style();
}
void reshade::runtime::draw_gui_statistics()
{
	unsigned int cpu_digits = 1;
	unsigned int gpu_digits = 1;
	uint64_t post_processing_time_cpu = 0;
	uint64_t post_processing_time_gpu = 0;

	if (!is_loading() && _effects_enabled)
	{
		for (const auto &technique : _techniques)
		{
			cpu_digits = std::max(cpu_digits, technique.average_cpu_duration >= 100'000'000 ? 3u : technique.average_cpu_duration >= 10'000'000 ? 2u : 1u);
			post_processing_time_cpu += technique.average_cpu_duration;
			gpu_digits = std::max(gpu_digits, technique.average_gpu_duration >= 100'000'000 ? 3u : technique.average_gpu_duration >= 10'000'000 ? 2u : 1u);
			post_processing_time_gpu += technique.average_gpu_duration;
		}
	}

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PlotLines("##framerate",
			_imgui_context->FramerateSecPerFrame, 120,
			_imgui_context->FramerateSecPerFrameIdx,
			nullptr,
			_imgui_context->FramerateSecPerFrameAccum / 120 * 0.5f,
			_imgui_context->FramerateSecPerFrameAccum / 120 * 1.5f,
			ImVec2(0, 50));
		ImGui::PopItemWidth();

		ImGui::BeginGroup();

		ImGui::TextUnformatted("Hardware:");
		ImGui::TextUnformatted("Application:");
		ImGui::TextUnformatted("Time:");
		ImGui::TextUnformatted("Network:");
		ImGui::Text("Frame %llu:", _framecount + 1);
		ImGui::NewLine();
		ImGui::TextUnformatted("Post-Processing:");

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.33333333f);
		ImGui::BeginGroup();

		if (_vendor_id != 0)
			ImGui::Text("VEN_%X", _vendor_id);
		else
			ImGui::TextUnformatted("Unknown");
		ImGui::TextUnformatted(g_target_executable_path.filename().u8string().c_str());
		ImGui::Text("%d-%d-%d %d", _date[0], _date[1], _date[2], _date[3]);
		ImGui::Text("%u B", g_network_traffic);
		ImGui::Text("%.2f fps", _imgui_context->IO.Framerate);
		ImGui::Text("%u draw calls", _drawcalls);
		ImGui::Text("%*.3f ms CPU", cpu_digits + 4, post_processing_time_cpu * 1e-6f);

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.66666666f);
		ImGui::BeginGroup();

		if (_device_id != 0)
			ImGui::Text("DEV_%X", _device_id);
		else
			ImGui::TextUnformatted("Unknown");
		ImGui::Text("0x%X", std::hash<std::string>()(g_target_executable_path.stem().u8string()) & 0xFFFFFFFF);
		ImGui::Text("%.0f ms", std::chrono::duration_cast<std::chrono::nanoseconds>(_last_present_time - _start_time).count() * 1e-6f);
		ImGui::NewLine();
		ImGui::Text("%*.3f ms", gpu_digits + 4, _last_frame_duration.count() * 1e-6f);
		ImGui::Text("%u vertices", _vertices);
		if (post_processing_time_gpu != 0)
			ImGui::Text("%*.3f ms GPU", gpu_digits + 4, (post_processing_time_gpu * 1e-6f));

		ImGui::EndGroup();
	}

	if (ImGui::CollapsingHeader("Techniques", ImGuiTreeNodeFlags_DefaultOpen) && !is_loading() && _effects_enabled)
	{
		ImGui::BeginGroup();

		for (const auto &technique : _techniques)
		{
			if (!technique.enabled)
				continue;

			if (technique.passes.size() > 1)
				ImGui::Text("%s (%zu passes)", technique.name.c_str(), technique.passes.size());
			else
				ImGui::TextUnformatted(technique.name.c_str());
		}

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.33333333f);
		ImGui::BeginGroup();

		for (const auto &technique : _techniques)
		{
			if (!technique.enabled)
				continue;

			if (technique.average_cpu_duration != 0)
				ImGui::Text("%*.3f ms CPU", cpu_digits + 4, technique.average_cpu_duration * 1e-6f);
			else
				ImGui::NewLine();
		}

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.66666666f);
		ImGui::BeginGroup();

		for (const auto &technique : _techniques)
		{
			if (!technique.enabled)
				continue;

			// GPU timings are not available for all APIs
			if (technique.average_gpu_duration != 0)
				ImGui::Text("%*.3f ms GPU", gpu_digits + 4, technique.average_gpu_duration * 1e-6f);
			else
				ImGui::NewLine();
		}

		ImGui::EndGroup();
	}

	if (ImGui::CollapsingHeader("Render Targets & Textures", ImGuiTreeNodeFlags_DefaultOpen) && !is_loading())
	{
		const char *texture_formats[] = {
			"unknown",
			"R8", "R16F", "R32F", "RG8", "RG16", "RG16F", "RG32F", "RGBA8", "RGBA16", "RGBA16F", "RGBA32F", "RGB10A2"
		};
		const unsigned int pixel_sizes[] = {
			0,
			1 /*R8*/, 2 /*R16F*/, 4 /*R32F*/, 2 /*RG8*/, 4 /*RG16*/, 4 /*RG16F*/, 8 /*RG32F*/, 4 /*RGBA8*/, 8 /*RGBA16*/, 8 /*RGBA16F*/, 16 /*RGBA32F*/, 4 /*RGB10A2*/
		};

		static_assert(std::size(texture_formats) - 1 == static_cast<size_t>(reshadefx::texture_format::rgb10a2));

		const float total_width = ImGui::GetWindowContentRegionWidth();
		unsigned int texture_index = 0;
		const unsigned int num_columns = static_cast<unsigned int>(std::ceilf(total_width / (50.0f * _font_size)));
		const float single_image_width = (total_width / num_columns) - 5.0f;

		// Variables used to calculate memory size of textures
		ldiv_t memory_view;
		const char *memory_size_unit;
		uint32_t post_processing_memory_size = 0;

		for (const texture &tex : _textures)
		{
			if (tex.impl == nullptr || !tex.semantic.empty() || (tex.shared.size() <= 1 && !_effects[tex.effect_index].rendering))
				continue;

			ImGui::PushID(texture_index);
			ImGui::BeginGroup();

			uint32_t memory_size = 0;
			for (uint32_t level = 0, width = tex.width, height = tex.height; level < tex.levels; ++level, width /= 2, height /= 2)
				memory_size += width * height * pixel_sizes[static_cast<unsigned int>(tex.format)];

			post_processing_memory_size += memory_size;

			if (memory_size >= 1024 * 1024) {
				memory_view = std::ldiv(memory_size, 1024 * 1024);
				memory_view.rem /= 1000;
				memory_size_unit = "MiB";
			}
			else {
				memory_view = std::ldiv(memory_size, 1024);
				memory_size_unit = "KiB";
			}

			ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s%s", tex.unique_name.c_str(), tex.shared.size() > 1 ? " (Pooled)" : "");
			ImGui::Text("%ux%u | %u mipmap(s) | %s | %ld.%03ld %s",
				tex.width,
				tex.height,
				tex.levels - 1,
				texture_formats[static_cast<unsigned int>(tex.format)],
				memory_view.quot, memory_view.rem, memory_size_unit);

			size_t num_referenced_passes = 0;
			std::vector<std::pair<size_t, std::vector<std::string>>> references;
			for (const technique &tech : _techniques)
			{
				if (std::find(tex.shared.begin(), tex.shared.end(), tech.effect_index) == tex.shared.end())
					continue;

				auto &reference = references.emplace_back();
				reference.first = tech.effect_index;

				for (size_t pass_index = 0; pass_index < tech.passes.size(); ++pass_index)
				{
					std::string pass_name = tech.passes[pass_index].name;
					if (pass_name.empty())
						pass_name = "pass " + std::to_string(pass_index);
					pass_name = tech.name + ' ' + pass_name;

					bool referenced = false;
					for (const reshadefx::sampler_info &sampler : tech.passes[pass_index].samplers)
					{
						if (sampler.texture_name == tex.unique_name)
						{
							referenced = true;
							reference.second.emplace_back(pass_name + " (sampler)");
							break;
						}
					}

					for (const reshadefx::storage_info &storage : tech.passes[pass_index].storages)
					{
						if (storage.texture_name == tex.unique_name)
						{
							referenced = true;
							reference.second.emplace_back(pass_name + " (storage)");
							break;
						}
					}

					for (const std::string &render_target : tech.passes[pass_index].render_target_names)
					{
						if (render_target == tex.unique_name)
						{
							referenced = true;
							reference.second.emplace_back(pass_name + " (render target)");
							break;
						}
					}

					if (referenced)
						num_referenced_passes++;
				}
			}

			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
			if (const std::string label = "Referenced by " + std::to_string(num_referenced_passes) + " pass(es) in " + std::to_string(tex.shared.size()) + " effect(s) ...";
				ImGui::ButtonEx(label.c_str(), ImVec2(single_image_width, 0)))
				ImGui::OpenPopup("##references");
			ImGui::PopStyleVar();

			if (!references.empty() && ImGui::BeginPopup("##references"))
			{
				bool is_open = false;
				size_t effect_index = std::numeric_limits<size_t>::max();
				for (const auto &reference : references)
				{
					if (effect_index != reference.first)
					{
						effect_index  = reference.first;
						is_open = ImGui::TreeNodeEx(_effects[effect_index].source_file.filename().u8string().c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen);
					}

					if (is_open)
					{
						for (const auto &pass : reference.second)
						{
							ImGui::Dummy(ImVec2(_imgui_context->Style.IndentSpacing, 0.0f));
							ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextUnformatted(pass.c_str(), pass.c_str() + pass.size());
						}
					}
				}

				ImGui::EndPopup();
			}

			if (bool check = _preview_texture == tex.impl && _preview_size[0] == 0; ImGui::RadioButton("Preview scaled", check)) {
				_preview_size[0] = 0;
				_preview_size[1] = 0;
				_preview_texture = !check ? tex.impl : nullptr;
			}
			ImGui::SameLine();
			if (bool check = _preview_texture == tex.impl && _preview_size[0] != 0; ImGui::RadioButton("Preview original", check)) {
				_preview_size[0] = tex.width;
				_preview_size[1] = tex.height;
				_preview_texture = !check ? tex.impl : nullptr;
			}

			bool r = (_preview_size[2] & 0x000000FF) != 0;
			bool g = (_preview_size[2] & 0x0000FF00) != 0;
			bool b = (_preview_size[2] & 0x00FF0000) != 0;
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 0, 0, 1));
			widgets::toggle_button("R", r);
			ImGui::PopStyleColor();
			if (tex.format >= reshadefx::texture_format::rg8)
			{
				ImGui::SameLine(0, 1);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 1, 0, 1));
				widgets::toggle_button("G", g);
				ImGui::PopStyleColor();
				if (tex.format >= reshadefx::texture_format::rgba8)
				{
					ImGui::SameLine(0, 1);
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 1, 1));
					widgets::toggle_button("B", b);
					ImGui::PopStyleColor();
				}
			}
			_preview_size[2] = (r ? 0x000000FF : 0) | (g ? 0x0000FF00 : 0) | (b ? 0x00FF0000 : 0) | 0xFF000000;

			const float aspect_ratio = static_cast<float>(tex.width) / static_cast<float>(tex.height);
			widgets::image_with_checkerboard_background(tex.impl, ImVec2(single_image_width, single_image_width / aspect_ratio), _preview_size[2]);

			ImGui::EndGroup();
			ImGui::PopID();

			if ((texture_index++ % num_columns) != (num_columns - 1))
				ImGui::SameLine(0.0f, 5.0f);
			else
				ImGui::Spacing();
		}

		if ((texture_index % num_columns) != 0)
			ImGui::NewLine(); // Reset ImGui::SameLine() so the following starts on a new line

		ImGui::Separator();

		if (post_processing_memory_size >= 1024 * 1024) {
			memory_view = std::ldiv(post_processing_memory_size, 1024 * 1024);
			memory_view.rem /= 1000;
			memory_size_unit = "MiB";
		}
		else {
			memory_view = std::ldiv(post_processing_memory_size, 1024);
			memory_size_unit = "KiB";
		}

		ImGui::Text("Total memory usage: %ld.%03ld %s", memory_view.quot, memory_view.rem, memory_size_unit);
	}
}
void reshade::runtime::draw_gui_log()
{
	const std::filesystem::path log_path =
		g_reshade_base_path / g_reshade_dll_path.filename().replace_extension(L".log");

	if (ImGui::Button("Clear Log"))
		// Close and open the stream again, which will clear the file too
		log::open(log_path);

	ImGui::SameLine();
	ImGui::Checkbox("Word Wrap", &_log_wordwrap);
	ImGui::SameLine();

	static ImGuiTextFilter filter; // TODO: Better make this a member of the runtime class, in case there are multiple instances.
	const bool filter_changed = filter.Draw("Filter (inc, -exc)", -150);

	if (ImGui::BeginChild("log", ImVec2(0, 0), true, _log_wordwrap ? 0 : ImGuiWindowFlags_AlwaysHorizontalScrollbar))
	{
		// Limit number of log lines to read, to avoid stalling when log gets too big
		const size_t line_limit = 1000;

		std::error_code ec;
		const auto last_log_size = std::filesystem::file_size(log_path, ec);
		if (filter_changed || _last_log_size != last_log_size)
		{
			_log_lines.clear();
			std::ifstream log_file(log_path);
			for (std::string line; std::getline(log_file, line) && _log_lines.size() < line_limit;)
				if (filter.PassFilter(line.c_str()))
					_log_lines.push_back(line);
			_last_log_size = last_log_size;

			if (_log_lines.size() == line_limit)
				_log_lines.push_back("Log was truncated to reduce memory footprint!");
		}

		ImGuiListClipper clipper(static_cast<int>(_log_lines.size()), ImGui::GetTextLineHeightWithSpacing());
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				ImVec4 textcol = ImGui::GetStyleColorVec4(ImGuiCol_Text);

				if (_log_lines[i].find("ERROR |") != std::string::npos || _log_lines[i].find("error") != std::string::npos)
					textcol = COLOR_RED;
				else if (_log_lines[i].find("WARN  |") != std::string::npos || _log_lines[i].find("warning") != std::string::npos || i == line_limit)
					textcol = COLOR_YELLOW;
				else if (_log_lines[i].find("DEBUG |") != std::string::npos)
					textcol = ImColor(100, 100, 255);

				ImGui::PushStyleColor(ImGuiCol_Text, textcol);
				if (_log_wordwrap) ImGui::PushTextWrapPos();

				ImGui::TextUnformatted(_log_lines[i].c_str());

				if (_log_wordwrap) ImGui::PopTextWrapPos();
				ImGui::PopStyleColor();
			}
		}
	} ImGui::EndChild();
}
void reshade::runtime::draw_gui_about()
{
	ImGui::TextUnformatted("ReShade " VERSION_STRING_PRODUCT);
	ImGui::Separator();

	ImGui::PushTextWrapPos();

	ImGui::TextUnformatted("Developed and maintained by crosire.");
	ImGui::TextUnformatted("Special thanks to CeeJay.dk and Marty McFly for their continued support!");
	ImGui::TextUnformatted("This project makes use of several open source libraries, licenses of which are listed below:");

	if (ImGui::CollapsingHeader("ReShade", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const auto resource = reshade::resources::load_data_resource(IDR_LICENSE_RESHADE);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("MinHook"))
	{
		const auto resource = reshade::resources::load_data_resource(IDR_LICENSE_MINHOOK);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("dear imgui"))
	{
		const auto resource = reshade::resources::load_data_resource(IDR_LICENSE_IMGUI);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("ImGuiColorTextEdit"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2017 BalazsJako

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)");
	}
	if (ImGui::CollapsingHeader("gl3w"))
	{
		const auto resource = reshade::resources::load_data_resource(IDR_LICENSE_GL3W);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("UTF8-CPP"))
	{
		const auto resource = reshade::resources::load_data_resource(IDR_LICENSE_UTFCPP);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("stb_image, stb_image_write"))
	{
		const auto resource = reshade::resources::load_data_resource(IDR_LICENSE_STB);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("DDS loading from SOIL"))
	{
		ImGui::TextUnformatted("Jonathan \"lonesock\" Dummer");
	}
	if (ImGui::CollapsingHeader("SPIR-V"))
	{
		const auto resource = reshade::resources::load_data_resource(IDR_LICENSE_SPIRV);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Vulkan & Vulkan-Loader"))
	{
		const auto resource = reshade::resources::load_data_resource(IDR_LICENSE_VULKAN);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Vulkan Memory Allocator"))
	{
		const auto resource = reshade::resources::load_data_resource(IDR_LICENSE_VMA);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Solarized"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2011 Ethan Schoonover

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)");
	}
	if (ImGui::CollapsingHeader("Fork Awesome"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2018 Fork Awesome (https://forkawesome.github.io)

This Font Software is licensed under the SIL Open Font License, Version 1.1. (http://scripts.sil.org/OFL))");
	}

	ImGui::PopTextWrapPos();
}

// NFS DEBUG/TWEAK MENU
// NFS CODE START
struct bVector3 // same as UMath::Vector3 anyways...
{
	float x;
	float y;
	float z;
};

struct bVector4
{
	float x;
	float y;
	float z;
	float w;
};

struct bMatrix4
{
	bVector4 v0;
	bVector4 v1;
	bVector4 v2;
	bVector4 v3;
};

// math stuff for RigidBody rotations...
void SetZRot(bMatrix4* dest, float zangle)
{
	float v3;
	float v4;
	float v5;
	float v6;
	float v7;
	bMatrix4* v8;
	int v9;

	v3 = (zangle * 6.2831855);
	v4 = cos(v3);
	v5 = v3;
	v6 = v4;
	v7 = sin(v5);
	v8 = dest;
	v9 = 16;
	do
	{
		v8->v0.x = 0.0;
		v8 = (bMatrix4*)((char*)v8 + 4);
		--v9;
	} while (v9);
	dest->v1.y = v6;
	dest->v0.x = v6;
	dest->v0.y = v7;
	dest->v1.x = -(float)v7;
	dest->v2.z = 1.0;
	dest->v3.w = 1.0;
}

void Matrix4Multiply(bMatrix4* m1, bMatrix4* m2, bMatrix4* result)
{
	float matrix1[4][4] = { 0 };
	float matrix2[4][4] = { 0 };
	float resulta[4][4] = { 0 };

	memcpy(&matrix1, m1, sizeof(bMatrix4));
	memcpy(&matrix2, m2, sizeof(bMatrix4));

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			for (int k = 0; k < 4; ++k)
			{
				resulta[i][j] += matrix1[i][k] * matrix2[k][j];
			}
		}
	}
	memcpy(result, &resulta, sizeof(bMatrix4));
}

enum GameFlowState
{
	GAMEFLOW_STATE_NONE = 0,
	GAMEFLOW_STATE_LOADING_FRONTEND = 1,
	GAMEFLOW_STATE_UNLOADING_FRONTEND = 2,
	GAMEFLOW_STATE_IN_FRONTEND = 3,
	GAMEFLOW_STATE_LOADING_REGION = 4,
	GAMEFLOW_STATE_LOADING_TRACK = 5,
	GAMEFLOW_STATE_RACING = 6,
	GAMEFLOW_STATE_UNLOADING_TRACK = 7,
	GAMEFLOW_STATE_UNLOADING_REGION = 8,
	GAMEFLOW_STATE_EXIT_DEMO_DISC = 9,
};

enum GRace_Context // SkipFE Race Type
{
	kRaceContext_QuickRace = 0,
	kRaceContext_TimeTrial = 1,
	kRaceContext_Online = 2,
	kRaceContext_Career = 3,
	kRaceContext_Count = 4,
};

char GameFlowStateNames[10][35] = {
  "NONE",
  "LOADING FRONTEND",
  "UNLOADING FRONTEND",
  "IN FRONTEND",
  "LOADING REGION",
  "LOADING TRACK",
  "RACING",
  "UNLOADING TRACK",
  "UNLOADING REGION",
  "EXIT DEMO DISC",
};

char GRaceContextNames[kRaceContext_Count][24] // SkipFE Race Type
{
	"Quick Race",
	"Time Trial",
	"Online",
	"Career",
};
char SkipFERaceTypeDisplay[64];

char PrecullerModeNames[4][27] = {
	"Preculler Mode: Off",
	"Preculler Mode: On",
	"Preculler Mode: Blinking",
	"Preculler Mode: High Speed",
};

bVector3 TeleportPos = { 0 };

void(__thiscall* GameFlowManager_UnloadFrontend)(void* dis) = (void(__thiscall*)(void*))GAMEFLOWMGR_UNLOADFE_ADDR;
void(__thiscall* GameFlowManager_UnloadTrack)(void* dis) = (void(__thiscall*)(void*))GAMEFLOWMGR_UNLOADTRACK_ADDR;
void(__thiscall* GameFlowManager_LoadRegion)(void* dis) = (void(__thiscall*)(void*))GAMEFLOWMGR_LOADREGION_ADDR;
int SkipFETrackNum = DEFAULT_TRACK_NUM;

void(__stdcall* RaceStarter_StartSkipFERace)() = (void(__stdcall*)())STARTSKIPFERACE_ADDR;

#ifdef GAME_MW
void(__stdcall* BootFlowManager_Init)() = (void(__stdcall*)())BOOTFLOWMGR_INIT_ADDR;
#endif

bool OnlineEnabled_OldState;

#ifndef OLD_NFS
int ActiveHotPos = 0;
bool bTeleFloorSnap = false;
bool bTeleFloorSnap_OldState = false;

void(*Sim_SetStream)(bVector3* location, bool blocking) = (void(*)(bVector3*, bool))SIM_SETSTREAM_ADDR;
bool(__thiscall*WCollisionMgr_GetWorldHeightAtPointRigorous)(void* dis, bVector3* pt, float* height, bVector3* normal) = (bool(__thiscall*)(void*, bVector3*, float*, bVector3*))WCOLMGR_GETWORLDHEIGHT_ADDR;

char SkipFEPlayerCar[64] = { DEFAULT_PLAYERCAR };
char SkipFEPlayerCar2[64] = { DEFAULT_PLAYER2CAR };
#ifdef GAME_PS
char SkipFEPlayerCar3[64] = { DEFAULT_PLAYER3CAR };
char SkipFEPlayerCar4[64] = { DEFAULT_PLAYER4CAR };
char SkipFETurboSFX[64] = "default";
char SkipFEForceHubSelectionSet[64];
char SkipFEForceRaceSelectionSet[64];
char SkipFEForceNIS[64];
char SkipFEForceNISContext[64];
bool bCalledProStreetTele = false;
#endif
#ifdef GAME_MW
char SkipFEOpponentPresetRide[64];
#else
#ifdef GAME_CARBON
char SkipFEPlayerPresetRide[64];
char SkipFEWingmanPresetRide[64];
char SkipFEOpponentPresetRide0[64];
char SkipFEOpponentPresetRide1[64];
char SkipFEOpponentPresetRide2[64];
char SkipFEOpponentPresetRide3[64];
char SkipFEOpponentPresetRide4[64];
char SkipFEOpponentPresetRide5[64];
char SkipFEOpponentPresetRide6[64];
char SkipFEOpponentPresetRide7[64];
#endif
#endif

// camera stuff
void(__cdecl* CameraAI_SetAction)(int EVIEW_ID, const char* action) = (void(__cdecl*)(int, const char*))CAMERA_SETACTION_ADDR;

int(__thiscall* UTL_IList_Find)(void* dis, void* IList) = (int(__thiscall*)(void*, void*))UTL_ILIST_FIND_ADDR;
#ifdef HAS_COPS
bool bDebuggingHeat = false;
bool bSetFEDBHeat = false;
float DebugHeat = 1.0;
#ifndef GAME_UC
void(__thiscall* FECareerRecord_AdjustHeatOnEventWin)(void* dis) = (void(__thiscall*)(void*))HEATONEVENTWIN_ADDR;
void(__cdecl* AdjustStableHeat_EventWin)(int player) = (void(__cdecl*)(int))ADJUSTSTABLEHEAT_EVENTWIN_ADDR;
void __stdcall FECareerRecord_AdjustHeatOnEventWin_Hook()
{
	unsigned int TheThis = 0;
	_asm mov TheThis, ecx
	if (!bDebuggingHeat)
		return FECareerRecord_AdjustHeatOnEventWin((void*)TheThis);
	*(float*)(TheThis + CAREERHEAT_OFFSET) = DebugHeat;
}
#endif

void TriggerSetHeat()
{
	int FirstLocalPlayer = **(int**)PLAYER_LISTABLESET_ADDR;
	int LocalPlayerVtable = *(int*)(FirstLocalPlayer);
	int LocalPlayerSimable = 0;
	int PlayerInstance = 0;

	int(__thiscall * LocalPlayer_GetSimable)(void* dis);

	if (*(int*)GAMEFLOWMGR_STATUS_ADDR == GAMEFLOW_STATE_RACING)
	{
		LocalPlayer_GetSimable = (int(__thiscall*)(void*)) * (int*)(LocalPlayerVtable + GETSIMABLE_OFFSET);
		LocalPlayerSimable = LocalPlayer_GetSimable((void*)FirstLocalPlayer);

		PlayerInstance = UTL_IList_Find(*(void**)(LocalPlayerSimable + 4), (void*)IPERPETRATOR_HANDLE_ADDR);
	

		if (PlayerInstance)
		{
			int(__thiscall *AIPerpVehicle_SetHeat)(void* dis, float heat);
			AIPerpVehicle_SetHeat = (int(__thiscall*)(void*, float))*(int*)((*(int*)PlayerInstance) + PERP_SETHEAT_OFFSET);
			AIPerpVehicle_SetHeat((void*)PlayerInstance, DebugHeat);
		}

	}
#ifndef GAME_UC
	if (bSetFEDBHeat)
	{
		bDebuggingHeat = true;
		AdjustStableHeat_EventWin(0);
		bDebuggingHeat = false;
	}
#endif
}
#endif

// race finish stuff
void(__cdecl* Game_NotifyRaceFinished)(void* ISimable) = (void(__cdecl*)(void*))GAMENOTIFYRACEFINISHED_ADDR;
//void(__cdecl* Game_NotifyLapFinished)(void* ISimable, int unk) = (void(__cdecl*)(void*, int))GAMENOTIFYLAPFINISHED_ADDR;
void(__cdecl* Game_EnterPostRaceFlow)() = (void(__cdecl*)())GAMEENTERPOSTRACEFLOW_ADDR;

int ForceFinishRace() // TODO: fix this, it's broken, either crashes or half-works
{
	int FirstLocalPlayer = **(int**)PLAYER_LISTABLESET_ADDR;
	int LocalPlayerVtable = *(int*)(FirstLocalPlayer);
	int LocalPlayerSimable = 0;

	int(__thiscall * LocalPlayer_GetSimable)(void* dis);

	if (*(int*)GAMEFLOWMGR_STATUS_ADDR == GAMEFLOW_STATE_RACING)
	{
		LocalPlayer_GetSimable = (int(__thiscall*)(void*)) * (int*)(LocalPlayerVtable + GETSIMABLE_OFFSET);
		LocalPlayerSimable = LocalPlayer_GetSimable((void*)FirstLocalPlayer);
		//Game_NotifyLapFinished((void*)LocalPlayerSimable, FinishParam);
		Game_NotifyRaceFinished((void*)LocalPlayerSimable);
		Game_EnterPostRaceFlow();
	}
	return 0;
}
#ifdef GAME_UC
void FlipCar()
{
	int FirstVehicle = **(int**)VEHICLE_LISTABLESET_ADDR;

	int RigidBodyInstance = 0;
	int RigidBodyVtable = 0;

	bMatrix4 rotor = { 0 };
	bMatrix4 result = { 0 };
	bMatrix4* GetMatrixRes = NULL;

	RigidBodyInstance = UTL_IList_Find(*(void**)(FirstVehicle + 4), (void*)IRIGIDBODY_HANDLE_ADDR);


	if (RigidBodyInstance)
	{
		RigidBodyVtable = *(int*)(RigidBodyInstance);
		bMatrix4*(__thiscall * RigidBody_GetMatrix4)(void* dis);
		RigidBody_GetMatrix4 = (bMatrix4*(__thiscall*)(void*)) * (int*)(RigidBodyVtable + RB_GETMATRIX4_OFFSET);

		int(__thiscall * RigidBody_SetOrientation)(void* dis, bMatrix4 * input);
		RigidBody_SetOrientation = (int(__thiscall*)(void*, bMatrix4*)) * (int*)(RigidBodyVtable + RB_SETORIENTATION_OFFSET);

		GetMatrixRes = RigidBody_GetMatrix4((void*)RigidBodyInstance);
		memcpy(&result, GetMatrixRes, 0x40);
		SetZRot(&rotor, 0.5);
		Matrix4Multiply(&result, &rotor, &result);
		RigidBody_SetOrientation((void*)RigidBodyInstance, &result);
	}
}
#else
void FlipCar()
{
	int FirstVehicle = **(int**)VEHICLE_LISTABLESET_ADDR;

	int RigidBodyInstance = 0;
	int RigidBodyVtable = 0;

	bMatrix4 rotor = { 0 };
	bMatrix4 result = { 0 };

	RigidBodyInstance = UTL_IList_Find(*(void**)(FirstVehicle + 4), (void*)IRIGIDBODY_HANDLE_ADDR);


	if (RigidBodyInstance)
	{
		RigidBodyVtable = *(int*)(RigidBodyInstance);
		int(__thiscall * RigidBody_GetMatrix4)(void* dis, bMatrix4* dest);
		RigidBody_GetMatrix4 = (int(__thiscall*)(void*, bMatrix4*)) *(int*)(RigidBodyVtable + RB_GETMATRIX4_OFFSET);

		int(__thiscall * RigidBody_SetOrientation)(void* dis, bMatrix4 * input);
		RigidBody_SetOrientation = (int(__thiscall*)(void*, bMatrix4*)) *(int*)(RigidBodyVtable + RB_SETORIENTATION_OFFSET);

		RigidBody_GetMatrix4((void*)RigidBodyInstance, &result);
		SetZRot(&rotor, 0.5);
		Matrix4Multiply(&result, &rotor, &result);
		RigidBody_SetOrientation((void*)RigidBodyInstance, &result);
	}
}
#endif

// overlay switch stuff that is only found in newer NFS games...
#ifndef GAME_UC
void(__thiscall* cFEng_QueuePackagePop)(void* dis, int num_to_pop) = (void(__thiscall*)(void*, int))FENG_QUEUEPACKAGEPOP_ADDR;
#ifdef GAME_MW
void(__thiscall* cFEng_QueuePackageSwitch)(void* dis, const char* pPackageName, int unk1, unsigned int unk2, bool) = (void(__thiscall*)(void*, const char*, int, unsigned int, bool))FENG_QUEUEPACKAGESWITCH_ADDR;
#else
void(__thiscall* cFEng_QueuePackageSwitch)(void* dis, const char* pPackageName, int pArg) = (void(__thiscall*)(void*, const char*, int))FENG_QUEUEPACKAGESWITCH_ADDR;
#endif
char CurrentOverlay[128] = { "ScreenPrintf.fng" };

void SwitchOverlay(char* overlay_name)
{
	cFEng_QueuePackagePop(*(void**)FENG_MINSTANCE_ADDR, 1);
#ifdef GAME_MW
	cFEng_QueuePackageSwitch(*(void**)FENG_MINSTANCE_ADDR, overlay_name, 0, 0, false);
#else
	cFEng_QueuePackageSwitch(*(void**)FENG_MINSTANCE_ADDR, overlay_name, 0);
#endif
}

unsigned int PlayerBin = 16;
#endif
void TriggerWatchCar(int type)
{
	//*(bool*)CAMERADEBUGWATCHCAR_ADDR = true;

	*(int*)MTOGGLECAR_ADDR = 0;
	*(int*)MTOGGLECARLIST_ADDR = type;
	*(bool*)CAMERADEBUGWATCHCAR_ADDR = !*(bool*)CAMERADEBUGWATCHCAR_ADDR;
	if (*(bool*)CAMERADEBUGWATCHCAR_ADDR)
		CameraAI_SetAction(1, "CDActionDebugWatchCar");
	else
		CameraAI_SetAction(1, "CDActionDrive");
}

#ifdef GAME_MW
void(__thiscall* EEnterBin_EEnterBin)(void* dis, int bin) = (void(__thiscall*)(void*, int))EENTERBIN_ADDR;
void*(__cdecl* Event_Alloc)(int size) = (void*(__cdecl*)(int))EVENTALLOC_ADDR;
char JumpToBinOptionText[32];

void JumpToBin(int bin)
{
	void* EventThingy = NULL;
	if (!*(int*)GRACESTATUS_ADDR && *(unsigned char*)CURRENTBIN_POINTER >= 2)
	{
		EEnterBin_EEnterBin(Event_Alloc(0xC), bin);
	}
}

void PlayMovie()
{
	if (*(int*)GAMEFLOWMGR_STATUS_ADDR == GAMEFLOW_STATE_IN_FRONTEND)
		SwitchOverlay("FEAnyMovie.fng");
	if (*(int*)GAMEFLOWMGR_STATUS_ADDR == GAMEFLOW_STATE_RACING)
		SwitchOverlay("InGameAnyMovie.fng");

}

bVector3 VisualFilterColourPicker = {0.88, 0.80, 0.44};
float VisualFilterColourMultiR = 1.0;
float VisualFilterColourMultiG = 1.0;
float VisualFilterColourMultiB = 1.0;
enum IVisualTreatment_eVisualLookState
{
	HEAT_LOOK = 0,
	COPCAM_LOOK = 1,
	FE_LOOK = 2,
};

#else
char SkipFERaceID[64];

#ifndef GAME_UC
char MovieFilename[64];
void(__cdecl* FEAnyMovieScreen_PushMovie)(const char* package) = (void(__cdecl*)(const char*))PUSHMOVIE_ADDR;

void PlayMovie()
{
	if (*(bool*)ISMOVIEPLAYING_ADDR)
	{
		cFEng_QueuePackagePop(*(void**)FENG_MINSTANCE_ADDR, 1);
	}
	FEAnyMovieScreen_PushMovie(MovieFilename);
}
#endif
#endif

#ifdef GAME_CARBON
// infinite NOS
bool bInfiniteNOS = false;
bool(__thiscall* EasterEggCheck)(void* dis, int cheat) = (bool(__thiscall*)(void*, int))EASTEREGG_CHECK_FUNC;

bool __stdcall EasterEggCheck_Hook(int cheat)
{
	int TheThis = 0;
	_asm mov TheThis, ecx
	if (cheat == 0xA && bInfiniteNOS)
		return true;
	return EasterEggCheck((void*)TheThis, cheat);
}

char BossNames[FAKEBOSS_COUNT][8] = {
	"None",
	"Angie",
	"Darius",
	"Wolf",
	"Kenji",
	"Neville"
};
char BossNames_DisplayStr[64] = "Force Fake Boss: Unknown";

char FeCarPosition_Names[CAR_FEPOSITION_COUNT][28] = {
	"CarPosition_Main",
	"CarPosition_Muscle",
	"CarPosition_Tuner",
	"CarPosition_Exotic",
	"CarPosition_Generic",
	"CarPosition_CarClass",
	"CarPosition_CarLot_Muscle",
	"CarPosition_CarLot_Tuner",
	"CarPosition_CarLot_Exotic",
	"CarPosition_CarLot_Mazda"
};

char FeCarPosition_DisplayStr[64] = "FeLocation: Unknown";

int FeCarPosition = 0;

#endif

#if defined(GAME_MW) || defined(GAME_CARBON)
void __stdcall JumpToNewPos(bVector3* pos)
{
	int FirstLocalPlayer = **(int**)PLAYER_LISTABLESET_ADDR;
	int LocalPlayerVtable;
	bVector3 ActualTeleportPos = { -(*pos).y, (*pos).z, (*pos).x }; // Simables have coordinates in this format...
	float Height = (*pos).z;
	char WCollisionMgrSpace[0x20] = { 0 };

	void(__thiscall*LocalPlayer_SetPosition)(void* dis, bVector3 *position);

	if (FirstLocalPlayer)
	{
		Sim_SetStream(&ActualTeleportPos, true);

		if (bTeleFloorSnap)
		{
			if (!WCollisionMgr_GetWorldHeightAtPointRigorous(WCollisionMgrSpace, &ActualTeleportPos, &Height, NULL))
			{
				Height += 1.0;
			}
			ActualTeleportPos.y = Height; // actually Z in Simables
		}

		LocalPlayerVtable = *(int*)(FirstLocalPlayer);
		LocalPlayer_SetPosition = (void(__thiscall*)(void*, bVector3*))*(int*)(LocalPlayerVtable+0x10);
		LocalPlayer_SetPosition((void*)FirstLocalPlayer, &ActualTeleportPos);
	}
}

#else
// Undercover & ProStreet are special beings. They're actually multi threaded.
// We have to do teleporting during EMainService / World::Service (or same at least in the same thread as World updates), otherwise we cause hanging bugs...

bool bDoTeleport = false;
bVector3 ServiceTeleportPos = { 0 };

// Track loading stuff
bool bDoTrackUnloading = false;
bool bDoFEUnloading= false;
bool bDoLoadRegion = false;

bool bDoTriggerWatchCar = false;
int CarTypeToWatch = CARLIST_TYPE_AIRACER;

bool bDoFlipCar = false;

bool bDoOverlaySwitch = false;
bool bDoPlayMovie = false;

#ifdef GAME_UC
bool bDoAwardCash = false;
float CashToAward = 0.0;
void(__thiscall* GMW2Game_AwardCash)(void* dis, float cash, float unk) = (void(__thiscall*)(void*, float, float))GMW2GAME_AWARDCASH_ADDR;
bool bDisableCops = false;
#endif

void __stdcall JumpToNewPosPropagator(bVector3* pos)
{
	int FirstLocalPlayer = **(int**)PLAYER_LISTABLESET_ADDR;
	int LocalPlayerVtable;
	bVector3 ActualTeleportPos = { -(*pos).y, (*pos).z, (*pos).x }; // Simables have coordinates in this format...
	float Height = (*pos).z;
	char WCollisionMgrSpace[0x20] = { 0 };

	void(__thiscall * LocalPlayer_SetPosition)(void* dis, bVector3 * position);

	if (FirstLocalPlayer)
	{
		Sim_SetStream(&ActualTeleportPos, true);
		if (bTeleFloorSnap)
		{
			if (!WCollisionMgr_GetWorldHeightAtPointRigorous(WCollisionMgrSpace, &ActualTeleportPos, &Height, NULL))
			{
				Height += 1.0;
			}
			ActualTeleportPos.y = Height; // actually Z in Simables
		}

		LocalPlayerVtable = *(int*)(FirstLocalPlayer);
		LocalPlayer_SetPosition = (void(__thiscall*)(void*, bVector3*)) * (int*)(LocalPlayerVtable + 0x10);
		LocalPlayer_SetPosition((void*)FirstLocalPlayer, &ActualTeleportPos);
	}
}

void __stdcall JumpToNewPos(bVector3* pos)
{
	memcpy(&ServiceTeleportPos, pos, sizeof(bVector3));
	bDoTeleport = true;
}

void __stdcall MainService_Hook()
{
	if (bDoTeleport)
	{
#ifdef GAME_PS
		if (bCalledProStreetTele)
		{
			bTeleFloorSnap_OldState = bTeleFloorSnap;
			bTeleFloorSnap = true;
		}
#endif
		JumpToNewPosPropagator(&ServiceTeleportPos);
		bDoTeleport = false;
#ifdef GAME_PS
		if (bCalledProStreetTele)
			bTeleFloorSnap = bTeleFloorSnap_OldState;
#endif
	}
	if (bDoTrackUnloading)
	{
		GameFlowManager_UnloadTrack((void*)GAMEFLOWMGR_ADDR);
		bDoTrackUnloading = false;
	}
	if (bDoFEUnloading)
	{
		*(int*)SKIPFE_ADDR = 1;
		*(int*)SKIPFETRACKNUM_ADDR = SkipFETrackNum;
		GameFlowManager_UnloadFrontend((void*)GAMEFLOWMGR_ADDR);
		bDoFEUnloading = false;
	}
	if (bDoLoadRegion)
	{
		*(int*)SKIPFE_ADDR = 1;
		*(int*)SKIPFETRACKNUM_ADDR = SkipFETrackNum;
		GameFlowManager_LoadRegion((void*)GAMEFLOWMGR_ADDR);
		bDoLoadRegion = false;
	}
	if (bDoTriggerWatchCar)
	{
		TriggerWatchCar(CarTypeToWatch);
		bDoTriggerWatchCar = false;
	}

	if (bDoFlipCar)
	{
		FlipCar();
		bDoFlipCar = false;
	}
#ifdef GAME_UC
	if (bDoAwardCash)
	{
		GMW2Game_AwardCash(*(void**)GMW2GAME_OBJ_ADDR, CashToAward, 0.0);
		bDoAwardCash = false;
	}
#else
	if (bDoOverlaySwitch)
	{
		SwitchOverlay(CurrentOverlay);
		bDoOverlaySwitch = false;
	}

	if (bDoPlayMovie)
	{
		PlayMovie();
		bDoPlayMovie = false;
	}
#endif

}

#endif
#else
#ifdef GAME_UG2
int GlobalRainType = DEFAULT_RAIN_TYPE;

void(__thiscall* Car_ResetToPosition)(unsigned int dis, bVector3* position, float unk, short int angle, bool unk2) = (void(__thiscall*)(unsigned int, bVector3*, float, short int, bool))CAR_RESETTOPOS_ADDR;
int(__cdecl* eGetView)(int view) = (int(__cdecl*)(int))EGETVIEW_ADDR;
void(__thiscall* Rain_Init)(void* dis, int RainType, float unk) = (void(__thiscall*)(void*, int, float))RAININIT_ADDR;


void __stdcall SetRainBase_Custom()
{
	int ViewResult;
	ViewResult = eGetView(1);
	ViewResult = *(int*)(ViewResult + 0x64);
	*(float*)(ViewResult + 0x509C) = 1.0;

	ViewResult = eGetView(2);
	ViewResult = *(int*)(ViewResult + 0x64);
	*(float*)(ViewResult + 0x509C) = 1.0;

	ViewResult = eGetView(1);
	ViewResult = *(int*)(ViewResult + 0x64);
	Rain_Init((void*)ViewResult, GlobalRainType, 1.0);

	ViewResult = eGetView(2);
	ViewResult = *(int*)(ViewResult + 0x64);
	Rain_Init((void*)ViewResult, GlobalRainType, 1.0);
}

void __stdcall JumpToNewPos(bVector3* pos)
{
	int FirstLocalPlayer = *(int*)PLAYERBYINDEX_ADDR;

	if (FirstLocalPlayer)
	{
		Car_ResetToPosition(*(unsigned int*)(FirstLocalPlayer + 4), pos, 0, 0, false);
	}
}

void(__cdecl* FEngPushPackage)(const char* pkg_name, int unk) = (void(__cdecl*)(const char*, int))FENG_PUSHPACKAGE_ADDR;
void(__cdecl* FEngPopPackage)(const char* pkg_name) = (void(__cdecl*)(const char*))FENG_POPPACKAGE_ADDR;
//void(__cdecl* FEngSwitchPackage)(const char* pkg_name, const char* pkg_name2 ,int unk) = (void(__cdecl*)(const char*, const char*, int))FENG_SWITCHPACKAGE_ADDR;
char CurrentOverlay[64] = { "ScreenPrintf.fng" };
char OverlayToPush[64] = { "ScreenPrintf.fng" };

char* cFEng_FindPackageWithControl_Name()
{
	return *(char**)(*(int*)((*(int*)FENG_PINSTANCE_ADDR) + 0x10) + 0x8);
}

void SwitchOverlay(char* overlay_name)
{
	FEngPopPackage(cFEng_FindPackageWithControl_Name());
	strcpy(OverlayToPush, overlay_name);
	FEngPushPackage(OverlayToPush, 0);
	//FEngSwitchPackage(cFEng_FindPackageWithControl_Name(), overlay_name, 0);
}

#else
// Since Undergound 1 on PC was compiled with GOD AWFUL OPTIMIZATIONS, the actual function symbol definition does not match (like it does in *gasp* other platforms and Underground 2)
// Arguments are passed through registers... REGISTERS. ON x86!
// THIS ISN'T MIPS OR PPC FOR CRYING OUT LOUD
// What devilish compiler setting even is this?
// Anyhow, it's still a thiscall, except, get this, POSITION and ANGLE are passed through EDX and EAX respectively
// So it's not even in ORDER, it's ARGUMENT 1 and ARGUMENT 3
void(__thiscall* Car_ResetToPosition)(unsigned int dis, float unk, bool unk2) = (void(__thiscall*)(unsigned int, float, bool))CAR_RESETTOPOS_ADDR;

void __stdcall JumpToNewPos(bVector3* pos)
{
	int FirstLocalPlayer = *(int*)PLAYERBYINDEX_ADDR;

	if (FirstLocalPlayer)
	{
		_asm
		{
			mov edx, pos
			xor eax, eax
		}
		Car_ResetToPosition(*(unsigned int*)(FirstLocalPlayer + 4), 0, false);
	}
}
#endif

#endif

#ifdef GAME_PS
// ProStreet Caves, because a lot of code is either optimized/missing or SecuRom'd, this should in theory work with UC if it is necessary
// A necessary evil to bring some features back.
// Everything else is hooked cleanly, no caves.

bool bToggleAiControl = false;
//int AIControlCaveExit = 0x41D296;
int AIControlCaveExit2 = AICONTROL_CAVE_EXIT;
int UpdateWrongWay = UPDATEWRONGWAY_ADDR;
bool bAppliedSpeedLimiterPatches = false;

void __declspec(naked) ToggleAIControlCave()
{
	_asm
	{
		mov al, bToggleAiControl
		test al, al
		jz AIControlCaveExit
		mov eax, [edi + 0x1A38]
		lea ecx, [edi + 0x1A38]
		call dword ptr [eax+8]
		neg al
		sbb al, al
		inc al
		lea ecx, [edi + 0x1A38]
		push eax
		mov eax, [edi + 0x1A38]
		call dword ptr [eax+0xC]
		mov bToggleAiControl, 0
	AIControlCaveExit:
		push edi
		call UpdateWrongWay
		jmp AIControlCaveExit2
	}
}

bool bInfiniteNOS = false;
void __declspec(naked) InfiniteNOSCave()
{
	if (bInfiniteNOS)
		_asm mov eax, 0xFF

	_asm
	{
		mov[esi + 0x10C], eax
		pop esi
		add esp, 8
		retn 8
	}
}

bool bDrawWorld = true;
int DrawWorldCaveTrueExit = DRAWWORLD_CAVE_TRUE_EXIT;
int DrawWorldCaveFalseExit = DRAWWORLD_CAVE_FALSE_EXIT;
void __declspec(naked) DrawWorldCave()
{
	if (!bDrawWorld)
		_asm jmp DrawWorldCaveFalseExit
	_asm
	{
		mov eax, dword ptr [GAMEFLOWMGR_STATUS_ADDR]
		cmp eax, 4
		jz DWCF_label
		jmp DrawWorldCaveTrueExit
		DWCF_label:
		jmp DrawWorldCaveFalseExit
	}
}

float GameSpeed = 1.0;
float GameSpeedConstant = 1.0;

int GameSpeedCaveExit = GAMESPEED_CAVE_EXIT;
void __declspec(naked) GameSpeedCave()
{
	_asm
	{
		fld GameSpeedConstant
		fld GameSpeed
		mov esi, ecx
		fucompp
		fnstsw ax
		test ah, 0x44
		jnp GSCE_LABEL
		fld GameSpeed
		pop esi
		pop ebx
		add esp, 8
		retn 8
		GSCE_LABEL:
		jmp GameSpeedCaveExit
	}
}

int __stdcall RetZero()
{
	return 0;
}

void ApplySpeedLimiterPatches()
{
	injector::WriteMemory<unsigned int>(0x0040BE15, 0, true);
	injector::WriteMemory<unsigned int>(0x004887A3, 0, true);
	injector::WriteMemory<unsigned int>(0x00488AA9, 0, true);
	injector::WriteMemory<unsigned int>(0x00488AE3, 0, true);
	injector::WriteMemory<unsigned int>(0x00718B3F, 0, true);
	injector::WriteMemory<unsigned int>(0x0071E4E8, 0, true);
	injector::MakeJMP(0x00402820, RetZero, true);
}

void UndoSpeedLimiterPatches()
{
	injector::WriteMemory<unsigned int>(0x0040BE15, 0x0A2D9ECB4, true);
	injector::WriteMemory<unsigned int>(0x004887A3, 0x0A2D9ECB4, true);
	injector::WriteMemory<unsigned int>(0x00488AA9, 0x0A2D9ECB4, true);
	injector::WriteMemory<unsigned int>(0x00488AE3, 0x0A2D9ECB4, true);
	injector::WriteMemory<unsigned int>(0x00718B3F, 0x0A2D9ECB4, true);
	injector::WriteMemory<unsigned int>(0x0071E4E8, 0x0A2D9ECB4, true);
	injector::WriteMemory<unsigned int>(0x00402820, 0x66E9FF6A, true);
	injector::WriteMemory<unsigned int>(0x00402822, 0x0DCA66E9, true);
}

#endif
#ifdef GAME_UC
bool bInfiniteNOS = false;
int InfiniteNOSExitTrue = INFINITENOS_CAVE_EXIT_TRUE;
int InfiniteNOSExitFalse = INFINITENOS_CAVE_EXIT_FALSE;
int InfiniteNOSExitBE = INFINITENOS_CAVE_EXIT_BE;
void __declspec(naked) InfiniteNOSCave()
{
	_asm
	{
		jbe InfNosExitBE
		cmp bInfiniteNOS, 0
		jne InfNosExitTrue
		jmp InfiniteNOSExitFalse
	InfNosExitTrue:
		jmp InfiniteNOSExitTrue
	InfNosExitBE:
		jmp InfiniteNOSExitBE
	}
	
}

bool bToggleAiControl = false;
bool bBeTrafficCar = false;
//int AIControlCaveExit = AICONTROL_CAVE_EXIT;
int AIControlCaveExit2 = AICONTROL_CAVE_EXIT2;
int UpdateWrongWay = 0x0041B490;
int unk_sub_aiupdate = 0x0040EF30;
bool bAppliedSpeedLimiterPatches = false;


void __declspec(naked) ToggleAIControlCave()
{
	_asm
	{
		test bl, bl
		movss dword ptr[edi + 0x1510], xmm0
		jnz AIControlCaveExit2Label

		mov ecx, edi
		call UpdateWrongWay
		mov ecx, edi
		call unk_sub_aiupdate

	AIControlCaveExit2Label:
		mov al, bToggleAiControl
		test al, al
		jz AIControlCaveExitLabel
		mov eax, [edi + 0x14AC]
		lea ecx, [edi + 0x14AC]
		call dword ptr[eax + 8]
		neg al
		sbb al, al
		inc al
		lea ecx, [edi + 0x14AC]
		push bBeTrafficCar
		push eax
		mov eax, [edi + 0x14AC]
		call dword ptr[eax + 0xC]
		mov bToggleAiControl, 0
		lea ebp, [edi + 0x14AC]
		
	AIControlCaveExitLabel:
		jmp AIControlCaveExit2
		
	}
}



#endif

#ifdef HAS_FOG_CTRL
bVector3 FogColourPicker = {0.43, 0.41, 0.29};
#endif

void reshade::runtime::draw_gui_nfs()
{
	bool modified = false;

	ImGui::TextUnformatted("NFS Tweak Menu");
	ImGui::Separator();
	ImGui::Checkbox("Draw FrontEnd", (bool*)DRAW_FENG_BOOL_ADDR);
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Front End", ImGuiTreeNodeFlags_None))
	{
#ifndef GAME_UG
		if (ImGui::CollapsingHeader("Safe House", ImGuiTreeNodeFlags_None))
		{
			ImGui::Checkbox("Unlock All Things", (bool*)UNLOCKALLTHINGS_ADDR);
#ifndef OLD_NFS
			ImGui::Checkbox("Car Guys Camera", (bool*)CARGUYSCAMERA_ADDR);
#ifdef GAME_MW
			if (*(int*)FEDATABASE_ADDR)
			{
				ImGui::InputInt("Player Cash", (int*)PLAYERCASH_POINTER, 1, 100, ImGuiInputTextFlags_None);
				PlayerBin = *(unsigned char*)CURRENTBIN_POINTER;
				if (ImGui::InputInt("Current Bin", (int*)&PlayerBin, 1, 100, ImGuiInputTextFlags_None))
				{
					*(unsigned char*)CURRENTBIN_POINTER = (PlayerBin & 0xFF);
				}
			}
#else
#ifndef GAME_UC
			if (*(int*)FEMANAGER_INSTANCE_ADDR)
			{
				ImGui::InputInt("Player Cash", (int*)PLAYERCASH_POINTER, 1, 100, ImGuiInputTextFlags_None);
				PlayerBin = *(unsigned char*)CURRENTBIN_POINTER;
				if (ImGui::InputInt("Current Bin", (int*)&PlayerBin, 1, 100, ImGuiInputTextFlags_None))
				{
					*(unsigned char*)CURRENTBIN_POINTER = (PlayerBin & 0xFF);
				}
#ifdef GAME_CARBON
				ImGui::InputInt("Player Rep", (int*)PLAYERREP_POINTER, 1, 100, ImGuiInputTextFlags_None);
				ImGui::InputText("Profile Name", (char*)PROFILENAME_POINTER, 31); // figure out the actual size, I assume 31 due to the memory layout

				ImGui::InputText("Crew Name", (char*)CREWNAME_POINTER, 15); // figure out the actual size, I assume 15 due to the memory layout
				FeCarPosition = *(unsigned char*)CAR_FEPOSITION_POINTER;
				sprintf(FeCarPosition_DisplayStr, "FeLocation: %s", FeCarPosition_Names[FeCarPosition]);
				if (ImGui::InputInt(FeCarPosition_DisplayStr, (int*)&FeCarPosition, 1, 100, ImGuiInputTextFlags_None))
				{
					FeCarPosition %= CAR_FEPOSITION_COUNT;
					if (FeCarPosition < 0)
						FeCarPosition = CAR_FEPOSITION_COUNT - 1;
					*(unsigned char*)CAR_FEPOSITION_POINTER = (FeCarPosition & 0xFF);
				}
#endif
				ImGui::Separator();
				ImGui::Text("User Profile pointer: 0x%X", USERPROFILE_POINTER);
#ifdef GAME_PS
				ImGui::Text("FECareer pointer: 0x%X", FECAREER_POINTER);
#endif
			}
#else
			if (*(int*)GMW2GAME_OBJ_ADDR)
			{
				ImGui::InputFloat("Player Cash Adjust", &CashToAward, 1.0, 1000.0, "%.1f", ImGuiInputTextFlags_CharsScientific);
				if (ImGui::Button("Adjust Cash", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bDoAwardCash = true;
				}
			}
#endif
#endif
#endif
#ifdef GAME_UG2
			if (ImGui::Button("Unlimited Casholaz", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				*(bool*)UNLIMITEDCASHOLAZ_ADDR = true;
			}
#endif
	}
#endif
		if (ImGui::CollapsingHeader("Printfs", ImGuiTreeNodeFlags_None))
		{
			ImGui::PushTextWrapPos();
			ImGui::TextUnformatted("NOTE: This doesn't actually do anything in the release builds yet. Only controls the variable.");
			ImGui::PopTextWrapPos();
			ImGui::Checkbox("Screen Printf", (bool*)DOSCREENPRINTF_ADDR);
			ImGui::Separator();
		}
#ifdef GAME_MW
		ImGui::Checkbox("Test Career Customization", (bool*)TESTCAREERCUSTOMIZATION_ADDR);
#endif
		ImGui::Checkbox("Show All Cars in FE", (bool*)SHOWALLCARSINFE_ADDR);
#ifndef OLD_NFS
#ifdef GAME_CARBON
		ImGui::Checkbox("Enable Debug Car Customize", (bool*)ENABLEDCC_ADDR);
#endif
#endif
#ifndef GAME_UG
#ifndef GAME_UC
		if (ImGui::CollapsingHeader("Overlays", ImGuiTreeNodeFlags_None))
		{
			ImGui::InputText("Manual overlay", CurrentOverlay, 128);
			if (ImGui::Button("Switch to the manual", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
#ifdef NFS_MULTITHREAD
				bDoOverlaySwitch = true;
#else
				SwitchOverlay(CurrentOverlay);
#endif
			}
#ifdef GAME_MW
			ImGui::Separator();
			if (ImGui::CollapsingHeader("Loading Screen", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Loading", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Loading.fng");
				if (ImGui::Button("Loading Controller", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Loading_Controller.fng");
				ImGui::Separator();
			}
			if (ImGui::CollapsingHeader("Main Menu", ImGuiTreeNodeFlags_None))
			{
				if (*(int*)GAMEFLOWMGR_STATUS_ADDR != GAMEFLOW_STATE_IN_FRONTEND)
					ImGui::TextUnformatted("WARNING: You're not in Front End. The game might crash if you use these.");
				if (ImGui::Button("Main Menu", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("MainMenu.fng");
				if (ImGui::Button("Main Menu Sub", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("MainMenu_Sub.fng");
				if (ImGui::Button("Options", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Options.fng");
				if (ImGui::Button("Quick Race Brief", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Quick_Race_Brief.fng");
				if (ImGui::Button("Track Select", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Track_Select.fng");
				ImGui::Separator();
			}

			if (ImGui::CollapsingHeader("Gameplay", ImGuiTreeNodeFlags_None))
			{
				if (*(int*)GAMEFLOWMGR_STATUS_ADDR != GAMEFLOW_STATE_RACING)
					ImGui::TextUnformatted("WARNING: You're not in race mode. The game might crash if you use these.");
				if (ImGui::Button("Pause Main", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Pause_Main.fng");
				if (ImGui::Button("Pause Performance Tuning", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Pause_Performance_Tuning.fng");
				if (ImGui::Button("World Map Main", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("WorldMapMain.fng");
				if (ImGui::Button("Pause Options", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Pause_Options.fng");
				if (ImGui::Button("InGame Reputation Overview", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("InGameReputationOverview.fng");
				if (ImGui::Button("InGame Milestones", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("InGameMilestones.fng");
				if (ImGui::Button("InGame Rival Challenge", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("InGameRivalChallenge.fng");
				ImGui::Separator();
			}
			if (ImGui::CollapsingHeader("Career", ImGuiTreeNodeFlags_None))
			{
				if (*(int*)GAMEFLOWMGR_STATUS_ADDR != GAMEFLOW_STATE_IN_FRONTEND)
					ImGui::TextUnformatted("WARNING: You're not in Front End. The game might crash if you use these.");
				if (ImGui::Button("Safehouse Reputation Overview", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("SafehouseReputationOverview.fng");
				if (ImGui::Button("Rap Sheet Overview", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("RapSheetOverview.fng");
				if (ImGui::Button("Rap Sheet Rankings", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("RapSheetRankings.fng");
				if (ImGui::Button("Rap Sheet Infractions", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("RapSheetInfractions.fng");
				if (ImGui::Button("Rap Sheet Cost To State", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("RapSheetCostToState.fng");
				if (ImGui::Button("Safehouse Rival Challenge", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("SafehouseRivalChallenge.fng");
				if (ImGui::Button("Safehouse Milestones", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("SafehouseMilestones.fng");
				if (ImGui::Button("BlackList", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("BlackList.fng");
				if (ImGui::Button("Controller Unplugged", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("ControllerUnplugged.fng");
				ImGui::Separator();
			}
			if (ImGui::CollapsingHeader("Customization (Must be in Customized Car Screen)", ImGuiTreeNodeFlags_None))
			{
				if (*(int*)GAMEFLOWMGR_STATUS_ADDR != GAMEFLOW_STATE_IN_FRONTEND)
					ImGui::TextUnformatted("WARNING: You're not in Front End. The game might crash if you use these.");
				if (ImGui::Button("My Cars Manager", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("MyCarsManager.fng");
				if (ImGui::Button("Debug Car Customize", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_DebugCarCustomize.fng");
				if (ImGui::Button("Customize Parts", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("CustomizeParts.fng");
				if (ImGui::Button("Customize Parts BACKROOM", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("CustomizeParts_BACKROOM.fng");
				if (ImGui::Button("Customize Category", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("CustomHUD.fng");
				if (ImGui::Button("Custom HUD BACKROOM", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("CustomHUD_BACKROOM.fng");
				if (ImGui::Button("Decals", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Decals.fng");
				if (ImGui::Button("Decals BACKROOM", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Decals_BACKROOM.fng");
				if (ImGui::Button("Numbers", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Numbers.fng");
				if (ImGui::Button("Rims BACKROOM", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Rims_BACKROOM.fng");
				ImGui::Separator();
			}
			if (ImGui::CollapsingHeader("Misc", ImGuiTreeNodeFlags_None))
			{
				if (*(int*)GAMEFLOWMGR_STATUS_ADDR != GAMEFLOW_STATE_IN_FRONTEND)
					ImGui::TextUnformatted("WARNING: You're not in Front End. The game might crash if you use these.");
				if (ImGui::Button("Keyboard", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Keyboard.fng");
				if (ImGui::Button("LS LangSelect", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("LS_LangSelect.fng");
				if (ImGui::Button("Loading Controller", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("Loading_Controller.fng");
				if (ImGui::Button("UI_OptionsController", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OptionsController.fng");
				ImGui::Separator();
			}
			if (ImGui::CollapsingHeader("Online (Must be in ONLINE connected)", ImGuiTreeNodeFlags_None))
			{
				if (*(int*)GAMEFLOWMGR_STATUS_ADDR != GAMEFLOW_STATE_IN_FRONTEND)
					ImGui::TextUnformatted("WARNING: You're not in Front End. The game might crash if you use these.");
				if (ImGui::Button("News and Terms", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_News_and_Terms.fng");
				if (ImGui::Button("Lobby Room", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_LobbyRoom.fng");
				if (ImGui::Button("Game Room", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLGameRoom.fng");
				if (ImGui::Button("Game Room host", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_GameRoom_Dialog.fng");
				if (ImGui::Button("Game Room client", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLGameRoom_client.fng");
				if (ImGui::Button("Mode Select List", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_ModeSelectList.fng");
				if (ImGui::Button("Online Main", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_MAIN.fng");
				if (ImGui::Button("Quick Race Main", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_Quickrace_Main.fng");
				if (ImGui::Button("Filters", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLFilters.fng");
				if (ImGui::Button("OptiMatch Available", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLX_OptiMatch_Available.fng");
				if (ImGui::Button("OptiMatch Filters", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLX_OptiMatch_Filters.fng");
				if (ImGui::Button("Rankings Personal", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLRankings_Personal.fng");
				if (ImGui::Button("Rankings Overall", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLRankings_Overall.fng");
				if (ImGui::Button("Rankings Monthly", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLRankings_Monthly.fng");
				if (ImGui::Button("Friend Dialogue", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_FriendDialogue.fng");
				if (ImGui::Button("Feedback", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_Feedback.fng");
				if (ImGui::Button("Voice Chat", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_VoiceChat.fng");
				if (ImGui::Button("Auth DNAS", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLAuthDNAS.fng");
				if (ImGui::Button("ISP Connect", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLISPConnect.fng");
				if (ImGui::Button("Select Persona", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_SelectPersona.fng");
				if (ImGui::Button("Create Account", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_Create_Account.fng");
				if (ImGui::Button("Age Verification", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLAgeVerif.fng");
				if (ImGui::Button("Age Too Young", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_OLAgeTooYoung.fng");
				if (ImGui::Button("Use Existing", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("OL_UseExisting.fng");
				if (ImGui::Button("Date Entry", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("UI_DateEntry.fng");
				ImGui::Separator();
			}
			if (ImGui::CollapsingHeader("Memory Card", ImGuiTreeNodeFlags_None))
			{
				if (*(int*)GAMEFLOWMGR_STATUS_ADDR != GAMEFLOW_STATE_IN_FRONTEND)
					ImGui::TextUnformatted("WARNING: You're not in Front End. The game might crash if you use these.");
				if (ImGui::Button("Profile Manager", ImVec2(ImGui::CalcItemWidth(), 0)))
					SwitchOverlay("MC_ProfileManager.fng");
				ImGui::Separator();
			}
#endif
		}
#endif
#endif
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Career", ImGuiTreeNodeFlags_None))
	{
#ifdef GAME_UG2
		ImGui::Checkbox("Shut Up Rachel", (bool*)SHUTUPRACHEL_ADDR);
#endif
#ifndef OLD_NFS
		ImGui::Checkbox("Skip bin 15 intro", (bool*)SKIPCAREERINTRO_ADDR);
#ifndef NFS_MULTITHREAD
		ImGui::Checkbox("Skip DDay Races", (bool*)SKIPDDAYRACES_ADDR);
#endif
#endif
#ifdef GAME_MW
		if (!*(int*)GRACESTATUS_ADDR && *(unsigned char*)CURRENTBIN_POINTER >= 2)
		{
			sprintf(JumpToBinOptionText, "Jump to bin %d", *(unsigned char*)CURRENTBIN_POINTER - 1);
			if (ImGui::Button(JumpToBinOptionText, ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				JumpToBin(*(unsigned char*)CURRENTBIN_POINTER - 1);
			}
		}
		else
		{
			if (*(int*)GRACESTATUS_ADDR)
			{
				ImGui::Text("Can't jump to bin %d while racing. Go to FE to jump bins.", *(unsigned char*)CURRENTBIN_POINTER - 1);
			}
			if (*(unsigned char*)CURRENTBIN_POINTER < 2)
			{
				ImGui::Text("No more bins left to jump! You're at bin %d!", *(unsigned char*)CURRENTBIN_POINTER);
			}
		}
#endif
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Teleport", ImGuiTreeNodeFlags_None))
	{
		ImGui::InputFloat("X", &TeleportPos.x, 0, 0, "%.3f", ImGuiInputTextFlags_CharsScientific);
		ImGui::InputFloat("Y", &TeleportPos.y, 0, 0, "%.3f", ImGuiInputTextFlags_CharsScientific);
		ImGui::InputFloat("Z", &TeleportPos.z, 0, 0, "%.3f", ImGuiInputTextFlags_CharsScientific);
		if (ImGui::Button("Engage!", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
			JumpToNewPos(&TeleportPos);
		}
		ImGui::Separator();

#ifdef OLD_NFS
#ifdef GAME_UG2
		ImGui::TextUnformatted("Hot Position"); // TODO: maybe port over Hot Position from UG2 to UG1?
		if (ImGui::Button("Save", ImVec2(10 * _font_size - _imgui_context->Style.ItemSpacing.x, 0)))
		{
			*(bool*)SAVEHOTPOS_ADDR = true;
		}

		ImGui::SameLine();
		if (ImGui::Button("Load", ImVec2(10 * _font_size - _imgui_context->Style.ItemSpacing.x, 0)))
		{
			*(bool*)LOADHOTPOS_ADDR = true;
		}
		ImGui::Separator();
#endif
#else
		ImGui::InputInt("Hot Position", &ActiveHotPos, 1, 1, ImGuiInputTextFlags_CharsDecimal);
		if (ActiveHotPos <= 0)
			ActiveHotPos = 1;
		ActiveHotPos %= 6;
		if (ImGui::Button("Save", ImVec2(10 * _font_size - _imgui_context->Style.ItemSpacing.x, 0)))
		{
			*(int*)SAVEHOTPOS_ADDR = ActiveHotPos;
		}

		ImGui::SameLine();
		if (ImGui::Button("Load", ImVec2(10 * _font_size - _imgui_context->Style.ItemSpacing.x, 0)))
		{
			*(int*)LOADHOTPOS_ADDR = ActiveHotPos;
		}
		ImGui::Checkbox("Floor Snapping", &bTeleFloorSnap);
		ImGui::Separator();
#endif		

#ifdef GAME_UG
		if (ImGui::CollapsingHeader("Landmarks (Underground 1 World) (L1RA)", ImGuiTreeNodeFlags_None))
		{
			ImGui::TextUnformatted("Sorry, no landmarks for Underground 1 yet :("); // TODO: make landmarks for UG1 Free Roam (it's such a small map anyway...)
		}
		ImGui::Separator();
#endif

#ifdef GAME_UG2
		if (ImGui::CollapsingHeader("Landmarks (Underground 2 World) (L4RA)", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Button("Garage", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				bVector3 pos = { 654.59, -102.02,   15.75 };
				JumpToNewPos(&pos);
			}
			if (ImGui::CollapsingHeader("Airport", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Terminal Station", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1871.38, -829.58,   34.08 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Parking Lot Front", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1831.13, -818.90,   24.08 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Parking Lot Behind", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1722.82, -795.87,   23.96 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("City Core", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Stadium Entrance", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1183.45, -646.81, 18.03 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Stadium Parking Lot", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1016.02, -849.46, 17.99 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("South Market - Casino Fountain", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -326.62, -559.63, 19.95 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Fort Union Square", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 212.86, -438.44, 18.03 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Best Buy", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 485.37, -617.05, 18.08 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Hotel Plaza", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -282.70, 81.08, 8.39 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Hotel Plaza Fountain", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -433.37, -170.49, 25.98 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Convention Center", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -632.79, 59.24, 10.59 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Main Street", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -758.48, -176.86, 29.38 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Construction Road 1", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 148.85, -830.72, 17.52 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Construction Road 2", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 36.05, -27.53, 16.36 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Parking Garage", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 154.32, 404.37, 4.57 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Basketball Court", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 523.90, -783.58, 8.37 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Hotel Plaza Parking Lot", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -225.51, 315.23, 1.08 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("Beacon Hill", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Parking Lot", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -649.46, 585.84, 25.21 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Burger King", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1390.40, 199.50, 11.03 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Park & Boardwalk", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1638.55, 433.51, 4.35 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1170.94, 564.39, 30.19 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Back Alley Shortcut", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1260.51, 778.16, 48.05 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Restaurant Back Alley", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1004.35, 705.92, 35.82 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Zigzag Bottom", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1643.70, 690.86, 2.73 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Zigzag Top", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1463.39, 859.80, 42.42 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Bridge", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -381.70, 709.03, 26.12 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("Pigeon Park", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Glass Garden", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -10.06, 827.62, 33.57 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Mansion", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 534.27, 988.52, 33.41 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Pavilion 1", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 376.73, 1170.13, 35.19 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Pavilion 2", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 207.75, 1335.30, 49.13 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("BBQ Restaurant", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -185.32, 1599.37, 43.84 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Fountain", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -124.09, 1519.45, 39.65 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Brad Lawless Memorial Statue", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -505.99, 1190.16, 47.18 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("Jackson Heights", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Entrance Gate", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1494.47, 1047.15, 52.78 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Waterfall Bridge 1", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -2126.07, 1651.40, 130.54 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Waterfall Bridge 2", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -2648.50, 2240.41, 229.08 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Large Mansion", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -3085.82, 2334.12, 262.17 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Mansion 2", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -720.69, 1853.10, 154.16 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Observatory", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -2067.00, 2794.99, 318.93 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Observatory Tunnel", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1499.79, 2219.02, 208.01 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Parking Lot", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -2024.93, 2587.49, 325.54 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Radio Tower", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1205.87, 3119.98, 375.72 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("City Vista", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -2155.04, 2016.00, 218.18 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("Coal Harbor", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Shipyard", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -687.03, -1433.25, 18.63 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Trainyard", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1031.52, -1782.72, 16.20 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -243.80, -1537.74, 13.95 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Trashyard", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1397.31, -1544.86, 13.85 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Refinery", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 253.64, -1886.44, 14.01 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("East Hwy Entrance", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 926.80, -1231.56, 14.07 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("Highway", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Hwy 7 North ", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 305.70, -1681.59, 21.09 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Hwy 7 South", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 859.69, -369.76, 18.73 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Hwy 27 North", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 208.38, -1013.74, 25.98 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Hwy 27 East", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1147.26, -270.31, 25.68 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Hwy 27 South", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 69.73, 317.97, 11.17 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Hwy 27 West", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1194.52, -212.97, 24.94 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("Shops", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::CollapsingHeader("City Center Shops", ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Button("Car Lot", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -728.10, -881.46,   18.11 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Performance", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -1051.87, -147.84,   17.19 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("El Norte Performance", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 337.01, -756.14,   13.39 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Body", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -387.00,   37.17,   14.15 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("El Norte Body", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 479.72,  122.67,   10.16 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Graphics", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -278.93, -464.33,   20.14 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Specialty", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -1267.26, -616.56,   19.93 };
						JumpToNewPos(&pos);
					}

				}
				if (ImGui::CollapsingHeader("Beacon Hill Shops", ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Button("Car Lot", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -974.43,  413.24,   11.80 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Performance", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -1306.92,  355.72,    9.27 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Body", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -994.29,  818.47,   39.12 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Graphics", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -491.38,  714.42,   29.13 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Specialty", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -1456.41,  688.46,   24.16 };
						JumpToNewPos(&pos);
					}

				}
				if (ImGui::CollapsingHeader("Jackson Heights Shops", ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Button("Body", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -1928.18, 1130.85,   61.72 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Graphics", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -1886.35, 1157.29,   61.63 };
						JumpToNewPos(&pos);
					}

				}
				if (ImGui::CollapsingHeader("Coal Harbor Shops", ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Button("Car Lot", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 37.30,-1718.94,   14.15 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("East Performance", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 840.05,-1404.39,    9.50 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("East Body", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -69.84,-1474.95,   13.92 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Graphics", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 456.23,-1553.53,   13.92 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("East Specialty", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 731.82,-1174.17,   13.57 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("West Performance", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -527.15,-1566.07,   13.93 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("West Body", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -546.54,-1893.46,    4.12 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("West Specialty", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -1125.19,-1872.93,   13.92 };
						JumpToNewPos(&pos);
					}
				}
			}
		}
		ImGui::Separator();
#endif
#if defined(GAME_MW) || defined(GAME_CARBON)
		if (ImGui::CollapsingHeader("Landmarks (Most Wanted World)", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Button("Jump to Memory High Watermark", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				bVector3 pos = { 1113.0, 3221.0, 394.0 };
				JumpToNewPos(&pos);
			}
			if (ImGui::CollapsingHeader("City Landmarks", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("East Park", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2085.00,  141.00,   98.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("West Park", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 709.00,  155.00,  115.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Stadium", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 934.00, -649.00,  116.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Time Square", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1452.05,  360.00,  101.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Little Italy", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2113.00,  322.00,  100.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Subway Entrance 1", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2147.00, 40.00, 94.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Subway Entrance 2", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 926.00, -556.00, 115.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Subway Entrance 3", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1819.00, 512.00, 94.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Highway North", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1819.00, 916.00, 124.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Highway South", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1972.00, -581.00, 105.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Highway East", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2515.00, 291.00, 93.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Highway West", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 499.00, 176.00, 108.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Safehouse", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2313.00, -70.00, 94.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Museum", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 885.00, 455.00, 113.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Amphitheatre", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 850.00, 246.00, 114.00 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("Coastal Landmarks", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Coastal Safe House", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 4254.00, 75.00, 11.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Coney Island", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 4094.00, 166.00, 23.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Fish Market", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 4629.00, 149.00, 7.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Oil Refinery", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3993.00, 2100.00, 28.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Fishing Village", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3693.00, 3516.00, 26.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Shipyard", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3107.00, 490.00, 18.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Trainyard", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2920.00, 175.00, 28.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Lighthouse", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 4092.00, -173.00, 17.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Drive-in Theatre", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3144.00, 1661.00, 110.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station 1", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3713.00, 3490.00, 27.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station 2", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2749.00, 2159.00, 108.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station 3", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3071.00, 1030.00, 67.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station 4", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3836.00, 605.00, 25.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Trailer Park", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2871.00, 3004.00, 74.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Cannery", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3401.00, 2853.00, 12.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Fire Hall", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3878.00, 459.00, 23.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Amusement Park", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3944.00, 273.00, 17.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Boardwalk", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 4542.00, 118.00, 6.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Prison", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 4093.00, 1302.00, 45.00 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("College Landmarks", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("College Safe House", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1773.00, 2499.00, 150.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Rosewood Park", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 663.00, 4084.00, 210.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Golf Course", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2180.00, 3591.00, 165.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Campus", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1087.00, 3187.00, 202.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Main Street", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1606.00, 2210.00, 145.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Baseball Stadium", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 24.00, 3239.00, 195.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Club House", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2168.00, 3566.00, 162.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Highway North", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1830.00, 4288.00, 233.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Highway South", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1798.00, 2003.00, 152.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Highway East", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2259.00, 2616.00, 141.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Highway West", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -293.00, 3633.00, 207.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Toll Booth 1", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 378.00, 4502.00, 236.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Toll Booth 2", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1580.00, 1800.00, 168.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station 1", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 822.00, 4500.00, 205.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station 2", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1216.00, 3667.00, 201.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station 3", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 549.00, 2622.00, 167.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station 4", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1641.00, 2486.00, 152.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gas Station 5", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1990.00, 1640.00, 152.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Small Parking Lot", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1057.00, 3826.00, 201.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Large Parking Lot", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1227.00, 3259.00, 204.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Tennis Court", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 729.00, 3540.00, 201.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Stadium", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 43.00, 3154.00, 189.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Cemetary", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 846.00, 2439.00, 151.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Hospital", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1419.00, 2613.00, 164.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Fire Hall", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1510.00, 2144.00, 146.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Donut Shop", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1825.00, 1860.00, 151.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Clock Tower", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1683.00, 2114.00, 146.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Strip Mall", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2633.00, 2199.00, 108.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Overpass", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2155.00, 2642.00, 148.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Bus Station", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 2094.00, 1427.00, 152.00 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("Shops", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("North College Chop", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 713.00, 4507.00, 214.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("College Chop", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1513.00, 2550.00, 158.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("College Car", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 990.00, 2164.00, 154.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("North City Chop", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1863.00, 1193.00, 146.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("City Chop", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1086.00, 54.00, 101.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("City Car", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1762.00, 529.00, 93.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("South City Chop", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3410.00, -203.00, 14.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("North Coastal Chop", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3611.00, 3636.00, 31.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Refinery Coastal Chop", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3467.00, 2019.00, 77.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Coastal Car", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 4200.00, 1276.00, 48.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Coastal Chop", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 4234.00, 714.00, 56.00 };
					JumpToNewPos(&pos);
				}
			}
		}
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Landmarks (Carbon World)", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::CollapsingHeader("Landmarks", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Tuner", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1912.00, 1363.00, 109.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Exotic", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 1175.00, 472.00, 60.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Casino", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 4794.00, 2040.00, 101.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Muscle", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3776.00, -1341.00, 12.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Santa Fe", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -2088.00, 1942.00, -6.00 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Palmont Motor Speedway (drift track)", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -1131.29, 7754.64, 1.14 };
					JumpToNewPos(&pos);
				}
			}
			if (ImGui::CollapsingHeader("Pursuit Breakers", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::CollapsingHeader("Casino", ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Button("Casino Archway", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 3993.00, 3459.00, 208.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Casino Gas Homes", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 3909.00, 2844.00, 150.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Casino Donut", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 5271.00, 2119.00, 102.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Casino Motel", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 4840.00, 3364.00, 140.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Casino Petro", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 5351.00, 3426.00, 117.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Casino Scaffold", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 4041.00, 2419.00, 130.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Casino Gas Commercial", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 4281.00, 2152.00, 112.00 };
						JumpToNewPos(&pos);
					}
				}
				if (ImGui::CollapsingHeader("Muscle", ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Button("Muscle Facade", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 2763.00, -602.00, 17.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Muscle Ice Cream", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 2889.00, -1361.00, 4.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Muscle Tire", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 4403.00, -32.00, 37.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Muscle Dock Crane", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 2770.00, -612.00, 17.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Muscle Gas", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 4773.00, -780.00, 27.00 };
						JumpToNewPos(&pos);
					}
				}
				if (ImGui::CollapsingHeader("Santa Fe", ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Button("Santa Fe Scaffold", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -2416.00, 1997.00, 11.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Santa Fe Sculpture", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -1854.00, 1891.00, 7.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Santa Fe Gas", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -2374.00, 1797.00, -5.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Santa Fe Thunderbird", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { -1967.00, 1695.00, 2.00 };
						JumpToNewPos(&pos);
					}
				}
				if (ImGui::CollapsingHeader("Tuner", ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Button("Tuner Gate A", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 4180.00, 588.00, 48.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Tuner Gate B", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 3883.00, 689.00, 32.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Tuner Gas Park", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 5286.00, 1524.00, 68.00 };
						JumpToNewPos(&pos);
					}
					if (ImGui::Button("Tuner Gas Financial", ImVec2(ImGui::CalcItemWidth(), 0)))
					{
						bVector3 pos = { 5563.00, 683.00, 57.00 };
						JumpToNewPos(&pos);
					}
				}
			}
			if (ImGui::CollapsingHeader("Canyons", ImGuiTreeNodeFlags_None))
			{
				if (ImGui::Button("Eternity Pass", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -6328.50, 12952.43, 942.69 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Journeyman's Bane", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -3256.09, 9614.98, 721.28 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Knife's Edge", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -2498.47, 6215.55, 787.85 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Lookout Point", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 3471.10, 10693.66, 602.65 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Devil's Creek Pass", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 7381.51, 11080.98, 598.47 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Lofty Heights Downhill", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -3253.87, 12843.19, 723.47 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Desparation Ridge", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -703.21, 12999.49, 888.32 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Deadfall Junction", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 8139.83, 9541.43, 889.62 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Copper Ridge", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { -7013.55, 6217.82,  787.86 };
					JumpToNewPos(&pos);
				}
				if (ImGui::Button("Gold Valley Run", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bVector3 pos = { 5224.52, 8066.35,  496.12 };
					JumpToNewPos(&pos);
				}
			}
		}
#endif
#ifdef GAME_PS
		if (ImGui::Button("Speed Challenge", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
			bCalledProStreetTele = true;
			bVector3 pos = { 0.0,   5000.0,  5.0 };
			JumpToNewPos(&pos);
		}
		if (ImGui::CollapsingHeader("Landmarks (Ebisu)", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Button("Ebisu West", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				bCalledProStreetTele = true;
				bVector3 pos = { -54.0,   -5.0,  5.0 };
				JumpToNewPos(&pos);
			}
			if (ImGui::Button("Ebisu South", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				bCalledProStreetTele = true;
				bVector3 pos = { -34.0,   240.0,  5.0 };
				JumpToNewPos(&pos);
			}
			if (ImGui::Button("Ebisu Touge", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				bCalledProStreetTele = true;
				bVector3 pos = { -255.0,   728.0,  5.0 };
				JumpToNewPos(&pos);
			}
		}
		if (ImGui::CollapsingHeader("Landmarks (Autopolis)", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Button("Autopolis Main", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				bCalledProStreetTele = true;
				bVector3 pos = { 40.0, 0,  2.0 };
				JumpToNewPos(&pos);
			}
			if (ImGui::Button("Autopolis Lakeside", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				bCalledProStreetTele = true;
				bVector3 pos = { 300.0, -150.0,  5.0 };
				JumpToNewPos(&pos);
			}
		}
		if (ImGui::CollapsingHeader("Landmarks (Willow Springs)", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Button("Willow Springs HorseThief", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				bCalledProStreetTele = true;
				bVector3 pos = { -332.0, 731.0,  5.0 };
				JumpToNewPos(&pos);
			}
			if (ImGui::Button("Willow Springs GP", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				bCalledProStreetTele = true;
				bVector3 pos = { -73.0, 6.0,  5.0 };
				JumpToNewPos(&pos);
			}
			if (ImGui::Button("Willow Springs Street", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				bCalledProStreetTele = true;
				bVector3 pos = { 236.0, 569.0,  5.0 };
				JumpToNewPos(&pos);
			}
		}
#endif
#ifdef GAME_UC
		if (ImGui::CollapsingHeader("Landmarks (Undercover / MW2 World)", ImGuiTreeNodeFlags_None))
		{
			ImGui::TextUnformatted("Sorry, no landmarks for Undercover / MW2 yet :("); // TODO: make / rip landmarks for MW2 Free Roam
		}
#endif
	}
	ImGui::Separator();
#ifndef OLD_NFS
	if (ImGui::CollapsingHeader("Race", ImGuiTreeNodeFlags_None))
	{
#ifdef GAME_CARBON
		if (*(int*)FORCEFAKEBOSS_ADDR >= FAKEBOSS_COUNT || *(int*)FORCEFAKEBOSS_ADDR < 0)
			sprintf(BossNames_DisplayStr, "Force Fake Boss: %s", "Unknown");
		else
			sprintf(BossNames_DisplayStr, "Force Fake Boss: %s", BossNames[*(int*)FORCEFAKEBOSS_ADDR]);
		ImGui::InputInt(BossNames_DisplayStr, (int*)FORCEFAKEBOSS_ADDR, 1, 100, ImGuiInputTextFlags_None);
#endif
		if (ImGui::Button("Force Finish", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
			ForceFinishRace();
		}
	}
	ImGui::Separator();
#endif
	if (ImGui::CollapsingHeader("AI", ImGuiTreeNodeFlags_None))
	{
#ifndef OLD_NFS
		if (ImGui::CollapsingHeader("Car Watches", ImGuiTreeNodeFlags_None))
		{
			ImGui::InputInt("Current car", (int*)MTOGGLECAR_ADDR, 1, 1, ImGuiInputTextFlags_None);
#ifdef NFS_MULTITHREAD
#ifdef HAS_COPS
			if (ImGui::Button("Watch Cop Car", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				CarTypeToWatch = CARLIST_TYPE_COP;
				bDoTriggerWatchCar = true;
			}
			if (ImGui::Button("Watch Traffic Car", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				CarTypeToWatch = CARLIST_TYPE_TRAFFIC;
				bDoTriggerWatchCar = true;
			}
#endif
			if (ImGui::Button("Watch Racer Car", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				CarTypeToWatch = CARLIST_TYPE_AIRACER;
				bDoTriggerWatchCar = true;
			}
#else
#ifdef HAS_COPS
			if (ImGui::Button("Watch Cop Car", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				TriggerWatchCar(CARLIST_TYPE_COP);
			}
			if (ImGui::Button("Watch Traffic Car", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				TriggerWatchCar(CARLIST_TYPE_TRAFFIC);
			}
#endif
			if (ImGui::Button("Watch Racer Car", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				TriggerWatchCar(CARLIST_TYPE_AIRACER);
			}
#endif
			ImGui::Separator();
		}
		if (ImGui::Button("Toggle AI Control", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
#ifdef NFS_MULTITHREAD
			bToggleAiControl = !bToggleAiControl;
#else
			* (bool*)TOGGLEAICONTROL_ADDR = !*(bool*)TOGGLEAICONTROL_ADDR;
#endif
		}
#ifdef GAME_UC
		ImGui::Checkbox("Be a traffic car (after toggle)", &bBeTrafficCar);
#endif
#endif
#ifdef HAS_COPS
#ifdef GAME_MW
		ImGui::Checkbox("Show Non-Pursuit Cops (Minimap)", (bool*)MINIMAP_SHOWNONPURSUITCOPS_ADDR);
		ImGui::Checkbox("Show Pursuit Cops (Minimap)", (bool*)MINIMAP_SHOWPURSUITCOPS_ADDR);
#endif
		if (ImGui::InputFloat("Set Heat", &DebugHeat, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific))
		{
			TriggerSetHeat();
		}
		ImGui::Checkbox("Also set heat to save file", &bSetFEDBHeat);
#ifdef GAME_UC
		bDisableCops = !*(bool*)ENABLECOPS_ADDR;
		if (ImGui::Checkbox("No Cops", &bDisableCops))
			*(bool*)ENABLECOPS_ADDR = !bDisableCops;
#else
		ImGui::Checkbox("No Cops", (bool*)DISABLECOPS_ADDR);
#endif
		ImGui::Checkbox("AI Random Turns", (bool*)AI_RANDOMTURNS_ADDR);
#endif
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_None))
	{
#ifndef OLD_NFS
#ifndef GAME_MW
		ImGui::Checkbox("SmartLookAheadCamera", (bool*)SMARTLOOKAHEADCAMERA_ADDR);
#endif
#endif
#ifdef OLD_NFS
		ImGui::Checkbox("Debug Cameras Enabled", (bool*)DEBUGCAMERASENABLED_ADDR);
#endif
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Car", ImGuiTreeNodeFlags_None))
	{
#ifndef OLD_NFS
		ImGui::PushTextWrapPos();
		ImGui::TextUnformatted("WARNING: Car changing is unstable and may cause the game to crash!");
		if (*(int*)GAMEFLOWMGR_STATUS_ADDR != GAMEFLOW_STATE_RACING)
			ImGui::TextUnformatted("WARNING: You're not in race mode. The game might crash if you use these.");
		ImGui::PopTextWrapPos();
		ImGui::Separator();
		if (ImGui::Button("Change Car", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
			*(bool*)CHANGEPLAYERVEHICLE_ADDR = true;
		}
		if (ImGui::Button("Flip Car", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
#ifdef NFS_MULTITHREAD
			bDoFlipCar = true;
#else
			FlipCar();
#endif
		}
#ifdef GAME_CARBON
		ImGui::Checkbox("Augmented Drift With EBrake", (bool*)AUGMENTEDDRIFT_ADDR);
#endif
#endif
#ifndef OLD_NFS
#ifdef GAME_MW
		ImGui::Checkbox("Infinite NOS", (bool*)INFINITENOS_ADDR);
#else
		ImGui::Checkbox("Infinite NOS", &bInfiniteNOS);
#endif
#ifndef GAME_PS
		ImGui::Checkbox("Infinite RaceBreaker", (bool*)INFINITERACEBREAKER_ADDR);
#else
		if (*(int*)0x0040BE15 == 0)
			bAppliedSpeedLimiterPatches = true;
		if (bAppliedSpeedLimiterPatches)
		{
			if (ImGui::Button("Revive Top Speed Limiter", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				UndoSpeedLimiterPatches();
				bAppliedSpeedLimiterPatches = false;
			}
		}
		else
		{
			if (ImGui::Button("Kill Top Speed Limiter", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				ApplySpeedLimiterPatches();
				bAppliedSpeedLimiterPatches = true;
			}
		}
		if (*(int*)GAMEFLOWMGR_STATUS_ADDR == GAMEFLOW_STATE_RACING)
		{
			ImGui::PushTextWrapPos();
			ImGui::TextUnformatted("NOTE: Top speed patches will take effect after reloading the track! (Go to FrontEnd and back)");
			ImGui::PopTextWrapPos();
		}
#endif
#endif

	}
	
	ImGui::Separator();
	if (ImGui::CollapsingHeader("GameFlow", ImGuiTreeNodeFlags_None))
	{
		ImGui::PushTextWrapPos();
		ImGui::TextUnformatted("WARNING: This feature is still experimental! The game may crash unexpectedly!\nLoad a save profile to avoid any bugs.");
#ifdef OLD_NFS
		ImGui::TextUnformatted("WARNING: You are running NFSU or NFSU2. Please use SkipFE features instead.");
#endif
		ImGui::PopTextWrapPos();
		ImGui::Separator();
		ImGui::Text("Current Track: %d", *(int*)SKIPFETRACKNUM_ADDR);
		ImGui::InputInt("Track Number", &SkipFETrackNum, 1, 100, ImGuiInputTextFlags_None);
		ImGui::Separator();

		if (*(int*)GAMEFLOWMGR_STATUS_ADDR == GAMEFLOW_STATE_IN_FRONTEND)
		{
			if (ImGui::Button("Start Track", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				
#if defined GAME_PS || defined GAME_UC
				bDoFEUnloading = true;
#else
				*(int*)SKIPFETRACKNUM_ADDR = SkipFETrackNum;
				GameFlowManager_UnloadFrontend((void*)GAMEFLOWMGR_ADDR);
#endif
			}
		}
		if (*(int*)GAMEFLOWMGR_STATUS_ADDR == GAMEFLOW_STATE_RACING)
		{
			if (ImGui::Button("Start Track (in game - it may not work, goto FE first)", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
#ifdef NFS_MULTITHREAD
				bDoLoadRegion = true;
#else
				* (int*)SKIPFETRACKNUM_ADDR = SkipFETrackNum;
				GameFlowManager_LoadRegion((void*)GAMEFLOWMGR_ADDR);
#endif
			}
#ifndef GAME_UC
			if (ImGui::Button("Unload Track (Go to FrontEnd)", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
#ifdef NFS_MULTITHREAD
				bDoTrackUnloading = true;
#else
#ifdef GAME_MW
				BootFlowManager_Init(); // otherwise crashes without it in MW...
#endif
				GameFlowManager_UnloadTrack((void*)GAMEFLOWMGR_ADDR);
#endif
			}
#endif
		}
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("SkipFE", ImGuiTreeNodeFlags_None))
	{
		ImGui::Checkbox("SkipFE Status", (bool*)SKIPFE_ADDR);
		ImGui::Text("Current Track: %d", *(int*)SKIPFETRACKNUM_ADDR);
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Track Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::InputInt("Track Number", &SkipFETrackNum, 1, 100, ImGuiInputTextFlags_None);
			ImGui::Checkbox("Track Reverse Direction", (bool*)SKIPFE_TRACKDIRECTION_ADDR);
			ImGui::InputInt("Num Laps", (int*)SKIPFE_NUMLAPS_ADDR, 1, 100, ImGuiInputTextFlags_None);
#ifndef OLD_NFS
#ifdef GAME_MW
			ImGui::InputText("Race ID", (char*)SKIPFE_RACEID_ADDR, 15);
#else
			if (ImGui::InputText("Race ID", SkipFERaceID, 64))
				*(char**)SKIPFE_RACEID_ADDR = SkipFERaceID;
#endif
#ifdef GAME_PS
			if (ImGui::InputText("Force Hub Selection Set", SkipFEForceHubSelectionSet, 64))
				*(char**)SKIPFE_FORCEHUBSELECTIONSET_ADDR = SkipFEForceHubSelectionSet;
			if (ImGui::InputText("Force Race Selection Set", SkipFEForceRaceSelectionSet, 64))
				*(char**)SKIPFE_FORCERACESELECTIONSET_ADDR = SkipFEForceRaceSelectionSet;
#endif
#endif
		}
		if (ImGui::CollapsingHeader("AI Settings", ImGuiTreeNodeFlags_None))
		{
			ImGui::InputInt("Num AI Cars", (int*)SKIPFE_NUMAICARS_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::SliderInt("Difficulty", (int*)SKIPFE_DIFFICULTY_ADDR, 0, 2);
#ifdef OLD_NFS
			ImGui::InputInt("Force All AI Cars To Type", (int*)SKIPFE_FORCEALLAICARSTOBETHISTYPE_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::InputInt("Force AI Car 1 To Type", (int*)SKIPFE_FORCEAICAR1TOBETHISTYPE_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::InputInt("Force AI Car 2 To Type", (int*)SKIPFE_FORCEAICAR2TOBETHISTYPE_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::InputInt("Force AI Car 3 To Type", (int*)SKIPFE_FORCEAICAR3TOBETHISTYPE_ADDR, 1, 100, ImGuiInputTextFlags_None);
#ifdef GAME_UG2
			ImGui::InputInt("Force AI Car 4 To Type", (int*)SKIPFE_FORCEAICAR4TOBETHISTYPE_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::InputInt("Force AI Car 5 To Type", (int*)SKIPFE_FORCEAICAR5TOBETHISTYPE_ADDR, 1, 100, ImGuiInputTextFlags_None);
#endif
			ImGui::SliderFloat("Force All AI Cars to Perf Rating", (float*)SKIPFE_FORCEALLAICARSTOPERFRATING_ADDR, -1.0, 10.0, "%.3f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("Force AI Car 1 to Perf Rating", (float*)SKIPFE_FORCEAICAR1TOPERFRATING_ADDR, -1.0, 10.0, "%.3f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("Force AI Car 2 to Perf Rating", (float*)SKIPFE_FORCEAICAR2TOPERFRATING_ADDR, -1.0, 10.0, "%.3f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("Force AI Car 3 to Perf Rating", (float*)SKIPFE_FORCEAICAR3TOPERFRATING_ADDR, -1.0, 10.0, "%.3f", ImGuiSliderFlags_None);
#ifdef GAME_UG2
			ImGui::SliderFloat("Force AI Car 4 to Perf Rating", (float*)SKIPFE_FORCEAICAR4TOPERFRATING_ADDR, -1.0, 10.0, "%.3f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("Force AI Car 5 to Perf Rating", (float*)SKIPFE_FORCEAICAR5TOPERFRATING_ADDR, -1.0, 10.0, "%.3f", ImGuiSliderFlags_None);
#endif
#else
#ifdef GAME_MW
			if (ImGui::InputText("Opponent Preset Ride", SkipFEOpponentPresetRide, 64))
			{
				*(char**)SKIPFE_OPPONENTPRESETRIDE_ADDR = SkipFEOpponentPresetRide;
			}
#elif GAME_CARBON
			ImGui::Checkbox("No Wingman", (bool*)SKIPFE_NOWINGMAN_ADDR);
			if (ImGui::InputText("Wingman Preset Ride", SkipFEWingmanPresetRide, 64))
				*(char**)SKIPFE_WINGMANPRESETRIDE_ADDR = SkipFEWingmanPresetRide;
			if (ImGui::InputText("Opponent 1 Preset Ride", SkipFEOpponentPresetRide0, 64))
				*(char**)SKIPFE_OPPONENTPRESETRIDE0_ADDR = SkipFEOpponentPresetRide0;
			if (ImGui::InputText("Opponent 2 Preset Ride", SkipFEOpponentPresetRide1, 64))
				*(char**)SKIPFE_OPPONENTPRESETRIDE1_ADDR = SkipFEOpponentPresetRide1;
			if (ImGui::InputText("Opponent 3 Preset Ride", SkipFEOpponentPresetRide2, 64))
				*(char**)SKIPFE_OPPONENTPRESETRIDE2_ADDR = SkipFEOpponentPresetRide2;
			if (ImGui::InputText("Opponent 4 Preset Ride", SkipFEOpponentPresetRide3, 64))
				*(char**)SKIPFE_OPPONENTPRESETRIDE3_ADDR = SkipFEOpponentPresetRide3;
			if (ImGui::InputText("Opponent 5 Preset Ride", SkipFEOpponentPresetRide4, 64))
				*(char**)SKIPFE_OPPONENTPRESETRIDE4_ADDR = SkipFEOpponentPresetRide4;
			if (ImGui::InputText("Opponent 6 Preset Ride", SkipFEOpponentPresetRide5, 64))
				*(char**)SKIPFE_OPPONENTPRESETRIDE5_ADDR = SkipFEOpponentPresetRide5;
			if (ImGui::InputText("Opponent 7 Preset Ride", SkipFEOpponentPresetRide6, 64))
				*(char**)SKIPFE_OPPONENTPRESETRIDE6_ADDR = SkipFEOpponentPresetRide6;
			if (ImGui::InputText("Opponent 8 Preset Ride", SkipFEOpponentPresetRide7, 64))
				*(char**)SKIPFE_OPPONENTPRESETRIDE7_ADDR = SkipFEOpponentPresetRide7;
#endif
#endif
		}
#ifdef HAS_COPS
		if (ImGui::CollapsingHeader("Cop Settings", ImGuiTreeNodeFlags_None))
		{
			ImGui::InputInt("Max Cops", (int*)SKIPFE_MAXCOPS_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::Checkbox("Helicopter", (bool*)SKIPFE_HELICOPTER_ADDR);
			ImGui::Checkbox("Disable Cops", (bool*)SKIPFE_DISABLECOPS_ADDR);
		}
#endif
		if (ImGui::CollapsingHeader("Traffic Settings", ImGuiTreeNodeFlags_None))
		{
#ifndef GAME_UC
#ifdef OLD_NFS
			ImGui::SliderInt("Traffic Density", (int*)SKIPFE_TRAFFICDENSITY_ADDR, 0, 3);
#else
			ImGui::SliderFloat("Traffic Density", (float*)SKIPFE_TRAFFICDENSITY_ADDR, 0.0, 100.0, "%.3f", ImGuiSliderFlags_None);
#endif
#ifndef GAME_UG2
			ImGui::SliderFloat("Traffic Oncoming", (float*)SKIPFE_TRAFFICONCOMING_ADDR, 0.0, 10.0, "%.3f", ImGuiSliderFlags_None);
#endif
#endif
#ifndef OLD_NFS
			ImGui::Checkbox("Disable Traffic", (bool*)SKIPFE_DISABLETRAFFIC_ADDR);
#endif
		}
		if (ImGui::CollapsingHeader("Game Settings", ImGuiTreeNodeFlags_None))
		{
			ImGui::Checkbox("Force Point2Point Mode", (bool*)SKIPFE_P2P_ADDR);
#ifdef GAME_PS
			ImGui::Checkbox("Practice Mode", (bool*)SKIPFE_PRACTICEMODE_ADDR);
#endif
#ifdef OLD_NFS
			ImGui::Checkbox("Force Drag Mode", (bool*)SKIPFE_DRAGRACE_ADDR);
			ImGui::Checkbox("Force Drift Mode", (bool*)SKIPFE_DRIFTRACE_ADDR);
#ifdef GAME_UG2
			ImGui::Checkbox("Force Team Drift Mode", (bool*)SKIPFE_DRIFTRACETEAMED_ADDR);
			ImGui::Checkbox("Force Burnout Mode", (bool*)SKIPFE_BURNOUTRACE_ADDR);
			ImGui::Checkbox("Force Short Track Mode", (bool*)SKIPFE_SHORTTRACK_ADDR);
			ImGui::InputFloat("Rolling Start Speed", (float*)SKIPFE_ROLLINGSTART_SPEED, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
#else
			ImGui::Checkbox("Force Be A Cop Mode", (bool*)SKIPFE_BEACOP_ADDR);
#endif
#endif
			if (*(int*)SKIPFE_RACETYPE_ADDR >= kRaceContext_Count || *(int*)SKIPFE_RACETYPE_ADDR < 0)
				sprintf(SkipFERaceTypeDisplay, "Race Type: %s", "Unknown");
			else
				sprintf(SkipFERaceTypeDisplay, "Race Type: %s", GRaceContextNames[*(int*)SKIPFE_RACETYPE_ADDR]);
			ImGui::InputInt(SkipFERaceTypeDisplay, (int*)SKIPFE_RACETYPE_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::InputInt("Num Player Cars", (int*)SKIPFE_NUMPLAYERCARS_ADDR, 1, 100, ImGuiInputTextFlags_None);
#ifndef OLD_NFS
#ifdef GAME_PS
			ImGui::InputInt("Num Player Screens", (int*)SKIPFE_NUMPLAYERSCREENS_ADDR, 1, 100, ImGuiInputTextFlags_None);
			if (ImGui::InputText("Force NIS", SkipFEForceNIS, 64))
				*(char**)SKIPFE_FORCENIS_ADDR = SkipFEForceNIS;
			if (ImGui::InputText("Force NIS Context", SkipFEForceNISContext, 64))
				*(char**)SKIPFE_FORCENISCONTEXT_ADDR = SkipFEForceNISContext;
			ImGui::Checkbox("Enable Debug Activity", (bool*)SKIPFE_ENABLEDEBUGACTIVITY_ADDR);
			ImGui::Checkbox("Disable Smoke", (bool*)SKIPFE_DISABLESMOKE_ADDR);
			ImGui::Checkbox("Slot Car Race", (bool*)SKIPFE_SLOTCARRACE_ADDR);
#else
#ifndef GAME_UC
			ImGui::Checkbox("Split screen mode", (bool*)SKIPFE_SPLITSCREEN_ADDR);
#endif
#endif
#endif
		}
		if (ImGui::CollapsingHeader("Car Settings", ImGuiTreeNodeFlags_None))
		{
#ifdef OLD_NFS
			ImGui::InputInt("Default Player 1 Car Type", (int*)SKIPFE_DEFAULTPLAYER1CARTYPE_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::InputInt("Default Player 2 Car Type", (int*)SKIPFE_DEFAULTPLAYER2CARTYPE_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::InputInt("Default Player 1 Skin Index", (int*)SKIPFE_DEFAULTPLAYER1SKININDEX_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::InputInt("Default Player 2 Skin Index", (int*)SKIPFE_DEFAULTPLAYER2SKININDEX_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::SliderInt("Player Car Upgrade Level", (int*)SKIPFE_PLAYERCARUPGRADEALL_ADDR, -1, 3);
#ifdef GAME_UG2
			ImGui::InputInt("Force Player 1 Start Position", (int*)SKIPFE_FORCEPLAYER1STARTPOS_ADDR, 1, 100, ImGuiInputTextFlags_None);
			ImGui::InputInt("Force Player 2 Start Position", (int*)SKIPFE_FORCEPLAYER2STARTPOS_ADDR, 1, 100, ImGuiInputTextFlags_None);
#endif
#else
			if (ImGui::InputText("Player Car", SkipFEPlayerCar, 128))
			{
				*(char**)SKIPFE_PLAYERCAR_ADDR = SkipFEPlayerCar;
			}
			if (ImGui::InputText("Player Car 2", SkipFEPlayerCar2, 128))
			{
				*(char**)SKIPFE_PLAYERCAR2_ADDR = SkipFEPlayerCar2;
			}
#ifdef GAME_PS
			if (ImGui::InputText("Player Car 3", SkipFEPlayerCar3, 128))
			{
				*(char**)SKIPFE_PLAYERCAR3_ADDR = SkipFEPlayerCar3;
			}
			if (ImGui::InputText("Player Car 4", SkipFEPlayerCar4, 128))
			{
				*(char**)SKIPFE_PLAYERCAR4_ADDR = SkipFEPlayerCar4;
			}
			if (ImGui::InputText("Turbo SFX", SkipFETurboSFX, 128))
			{
				*(char**)SKIPFE_TURBOSFX_ADDR = SkipFETurboSFX;
			}
			ImGui::InputInt("Transmission Setup", (int*)SKIPFE_TRANSMISSIONSETUP_ADDR, 1, 100);
			if (ImGui::CollapsingHeader("Driver Aids", ImGuiTreeNodeFlags_None))
			{
				ImGui::SliderInt("Traction Control Level", (int*)SKIPFE_TRACTIONCONTROLLEVEL_ADDR, -1, 4);
				ImGui::SliderInt("Stability Control Level", (int*)SKIPFE_STABILITYCONTROLLEVEL_ADDR, -1, 3);
				ImGui::SliderInt("ABS Level", (int*)SKIPFE_ANTILOCKBRAKESLEVEL_ADDR, -1, 3);
				ImGui::SliderInt("Drift Steering", (int*)SKIPFE_DRIFTASSISTLEVEL_ADDR, -1, 10);
				ImGui::SliderInt("Raceline Assist", (int*)SKIPFE_RACELINEASSISTLEVEL_ADDR, -1, 20);
				ImGui::SliderInt("Braking Assist", (int*)SKIPFE_BRAKINGASSISTLEVEL_ADDR, -1, 20);
			}

#endif
#ifdef GAME_CARBON
			if (ImGui::InputText("Player Preset Ride", SkipFEPlayerPresetRide, 64))
				*(char**)SKIPFE_PLAYERPRESETRIDE_ADDR = SkipFEPlayerPresetRide;
#endif
			ImGui::SliderFloat("Player Car Performance", (float*)SKIPFE_PLAYERPERFORMANCE_ADDR, -1.0, 10.0, "%.3f", ImGuiSliderFlags_None);
#endif
		}
		ImGui::Separator();
		if (ImGui::Button("Start SkipFE Race", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
			*(int*)SKIPFETRACKNUM_ADDR2 = SkipFETrackNum;
			*(int*)SKIPFE_ADDR = 1;
#if defined(GAME_MW) || defined(OLD_NFS)
			OnlineEnabled_OldState = *(bool*)ONLINENABLED_ADDR;
			*(int*)ONLINENABLED_ADDR = 0;
#endif
			RaceStarter_StartSkipFERace();
#if defined(GAME_MW) || defined(OLD_NFS)
			*(int*)ONLINENABLED_ADDR = OnlineEnabled_OldState;
#endif
		}
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_None))
	{
#ifdef GAME_UC
		if (ImGui::Checkbox("Motion Blur", &bMotionBlur))
		{
			modified = true;
		}
		ImGui::SliderFloat("Bloom Scale", (float*)0x00D5E154, -10.0, 10.0, "%.3f", ImGuiSliderFlags_None);
#endif
#ifndef OLD_NFS
#ifdef GAME_PS
		ImGui::InputFloat("Game Speed", &GameSpeed, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
#else
		ImGui::InputFloat("Game Speed", (float*)GAMESPEED_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
		ImGui::Checkbox("Visual Look Filter", (bool*)APPLYVISUALLOOK_ADDR);
#endif
		ImGui::InputInt(PrecullerModeNames[*(int*)PRECULLERMODE_ADDR], (int*)PRECULLERMODE_ADDR, 1, 100, ImGuiInputTextFlags_None);
		*(int*)PRECULLERMODE_ADDR %= 4;
		if (*(int*)PRECULLERMODE_ADDR < 0)
			*(int*)PRECULLERMODE_ADDR = 3;
#endif
#ifdef GAME_UG2
		ImGui::Checkbox("Draw Cars", (bool*)DRAWCARS_ADDR);
		ImGui::Checkbox("Draw Car Reflections", (bool*)DRAWCARSREFLECTIONS_ADDR);
		ImGui::Checkbox("Draw Light Flares", (bool*)DRAWLIGHTFLARES_ADDR);
		ImGui::Checkbox("Draw Fancy Car Shadows", (bool*)DRAWFANCYCARSHADOW_ADDR);
		ImGui::InputFloat("Fancy Car Shadow Edge Mult.", (float*)FANCYCARSHADOWEDGEMULT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
		ImGui::InputFloat("Wheel Pivot Translation Amount", (float*)WHEELPIVOTTRANSLATIONAMOUNT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
		ImGui::InputFloat("Wheel Standard Width", (float*)WHEELSTANDARDWIDTH_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);

		if (ImGui::CollapsingHeader("Precipitation & Weather", ImGuiTreeNodeFlags_None))
		{
			ImGui::Checkbox("Precipitation Enable", (bool*)PRECIPITATION_ENABLE_ADDR);
			ImGui::Checkbox("Precipitation Render", (bool*)PRECIPITATION_RENDER_ADDR);
			ImGui::Checkbox("Precipitation Debug Enable", (bool*)PRECIPITATION_DEBUG_ADDR);
			ImGui::Separator();
			ImGui::TextUnformatted("Values");
			if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Precipitation Percentage", (float*)PRECIPITATION_PERCENT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Bound X", (float*)PRECIP_BOUNDX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Bound Y", (float*)PRECIP_BOUNDY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Bound Z", (float*)PRECIP_BOUNDZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Ahead X", (float*)PRECIP_AHEADX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Ahead Y", (float*)PRECIP_AHEADY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Ahead Z", (float*)PRECIP_AHEADZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Weather Change", (float*)PRECIP_WEATHERCHANGE_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Drive Factor", (float*)PRECIP_DRIVEFACTOR_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Prevailing Multiplier", (float*)PRECIP_PREVAILINGMULT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::Checkbox("Always Raining", (bool*)PRECIP_CHANCE100_ADDR);
				ImGui::InputInt("Rain Type (restart world to see diff.)", &GlobalRainType, 1, 100, ImGuiInputTextFlags_None);
			}
			if (ImGui::CollapsingHeader("Wind", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Wind Angle", (float*)PRECIP_WINDANG_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Max Sway", (float*)PRECIP_SWAYMAX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Max Wind Effect", (float*)PRECIP_MAXWINDEFF_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
			if (ImGui::CollapsingHeader("Road Dampness", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Base Dampness", (float*)PRECIP_BASEDAMPNESS_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Uber Dampness", (float*)PRECIP_UBERDAMPNESS_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
			if (ImGui::CollapsingHeader("On-screen FX", ImGuiTreeNodeFlags_None))
			{
				ImGui::Checkbox("OverRide Enable", (bool*)PRECIP_ONSCREEN_OVERRIDE_ADDR);
				ImGui::InputFloat("Drip Speed", (float*)PRECIP_ONSCREEN_DRIPSPEED_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Speed Mod", (float*)PRECIP_ONSCREEN_SPEEDMOD_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
			if (ImGui::CollapsingHeader("Fog", ImGuiTreeNodeFlags_None))
			{
				ImGui::Checkbox("Fog Control OverRide", (bool*)FOG_CTRLOVERRIDE_ADDR);
				ImGui::InputFloat("Precip. Fog Percentage", (float*)PRECIP_FOGPERCENT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Base Fog Falloff", (float*)BASEFOG_FALLOFF_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Base Fog Falloff X", (float*)BASEFOG_FALLOFFX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Base Fog Falloff Y", (float*)BASEFOG_FALLOFFY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Base Weather Fog", (float*)BASEWEATHER_FOG_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Base Weather Fog Start", (float*)BASEWEATHER_FOG_START_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				if (ImGui::CollapsingHeader("Base Weather Fog Colour", ImGuiTreeNodeFlags_None))
				{
					if (ImGui::ColorPicker3("", (float*)&(FogColourPicker.x), ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueWheel))
					{
						*(int*)BASEWEATHER_FOG_COLOUR_R_ADDR = (int)(FogColourPicker.x * 255);
						*(int*)BASEWEATHER_FOG_COLOUR_G_ADDR = (int)(FogColourPicker.y * 255);
						*(int*)BASEWEATHER_FOG_COLOUR_B_ADDR = (int)(FogColourPicker.z * 255);
					}
				}
			}
			if (ImGui::CollapsingHeader("Rain", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Rain X", (float*)PRECIP_RAINX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Y", (float*)PRECIP_RAINY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Z", (float*)PRECIP_RAINZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Z Constant", (float*)PRECIP_RAINZCONSTANT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Radius X", (float*)PRECIP_RAINRADIUSX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Radius Y", (float*)PRECIP_RAINRADIUSY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Radius Z", (float*)PRECIP_RAINRADIUSZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Wind Effect", (float*)PRECIP_RAINWINDEFF_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Percentage", (float*)PRECIP_RAINPERCENT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain in the headlights", (float*)PRECIP_RAININTHEHEADLIGHTS_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
			if (ImGui::CollapsingHeader("Snow", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Snow X", (float*)PRECIP_SNOWX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Snow Y", (float*)PRECIP_SNOWY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Snow Z", (float*)PRECIP_SNOWZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Snow Z Constant", (float*)PRECIP_SNOWZCONSTANT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Snow Radius X", (float*)PRECIP_SNOWRADIUSX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Snow Radius Y", (float*)PRECIP_SNOWRADIUSY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Snow Radius Z", (float*)PRECIP_SNOWRADIUSZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Snow Wind Effect", (float*)PRECIP_SNOWWINDEFF_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Snow Percentage", (float*)PRECIP_SNOWPERCENT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
			if (ImGui::CollapsingHeader("Sleet", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Sleet X", (float*)PRECIP_SLEETX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Sleet Y", (float*)PRECIP_SLEETY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Sleet Z", (float*)PRECIP_SLEETZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Sleet Z Constant", (float*)PRECIP_SLEETZCONSTANT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Sleet Radius X", (float*)PRECIP_SLEETRADIUSX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Sleet Radius Y", (float*)PRECIP_SLEETRADIUSY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Sleet Radius Z", (float*)PRECIP_SLEETRADIUSZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Sleet Wind Effect", (float*)PRECIP_SLEETWINDEFF_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
			if (ImGui::CollapsingHeader("Hail", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Hail X", (float*)PRECIP_HAILX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Hail Y", (float*)PRECIP_HAILY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Hail Z", (float*)PRECIP_HAILZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Hail Z Constant", (float*)PRECIP_HAILZCONSTANT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Hail Radius X", (float*)PRECIP_HAILRADIUSX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Hail Radius Y", (float*)PRECIP_HAILRADIUSY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Hail Radius Z", (float*)PRECIP_HAILRADIUSZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Hail Wind Effect", (float*)PRECIP_HAILWINDEFF_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}

		}
#endif
#ifdef HAS_FOG_CTRL
#ifndef OLD_NFS
		if (ImGui::CollapsingHeader("Precipitation & Weather", ImGuiTreeNodeFlags_None))
		{
			ImGui::Checkbox("Precipitation Enable", (bool*)PRECIPITATION_ENABLE_ADDR);
			ImGui::Checkbox("Precipitation Render", (bool*)PRECIPITATION_RENDER_ADDR);
			ImGui::Checkbox("Precipitation Debug Enable", (bool*)PRECIPITATION_DEBUG_ADDR);
			ImGui::Separator();
			ImGui::TextUnformatted("Values");
			if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Precipitation Percentage", (float*)PRECIPITATION_PERCENT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Bound X", (float*)PRECIP_BOUNDX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Bound Y", (float*)PRECIP_BOUNDY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Bound Z", (float*)PRECIP_BOUNDZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Ahead X", (float*)PRECIP_AHEADX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Ahead Y", (float*)PRECIP_AHEADY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Ahead Z", (float*)PRECIP_AHEADZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Drive Factor", (float*)PRECIP_DRIVEFACTOR_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Prevailing Multiplier", (float*)PRECIP_PREVAILINGMULT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Camera Mod", (float*)PRECIP_CAMERAMOD_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
			if (ImGui::CollapsingHeader("Wind", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Wind Angle", (float*)PRECIP_WINDANG_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Max Sway", (float*)PRECIP_SWAYMAX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Max Wind Effect", (float*)PRECIP_MAXWINDEFF_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
			if (ImGui::CollapsingHeader("Road Dampness", ImGuiTreeNodeFlags_None))
			{
#ifdef GAME_MW
				ImGui::InputFloat("Base Dampness", (float*)PRECIP_BASEDAMPNESS_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
#endif
#ifdef GAME_CARBON
				ImGui::InputFloat("Wet Dampness", (float*)PRECIP_WETDAMPNESS_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Dry Dampness", (float*)PRECIP_DRYDAMPNESS_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);

#endif
			}
			if (ImGui::CollapsingHeader("On-screen FX", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Drip Speed", (float*)PRECIP_ONSCREEN_DRIPSPEED_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Speed Mod", (float*)PRECIP_ONSCREEN_SPEEDMOD_ADDR, 0.0001, 0.001, "%.6f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Drop Shape Speed Change", (float*)PRECIP_ONSCREEN_DROPSHAPESPEEDCHANGE_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
			if (ImGui::CollapsingHeader("Fog", ImGuiTreeNodeFlags_None))
			{
				ImGui::Checkbox("Fog Control OverRide", (bool*)FOG_CTRLOVERRIDE_ADDR);
				ImGui::InputFloat("Precip. Fog Percentage", (float*)PRECIP_FOGPERCENT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Base Fog Falloff", (float*)BASEFOG_FALLOFF_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Base Fog Falloff X", (float*)BASEFOG_FALLOFFX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Base Fog Falloff Y", (float*)BASEFOG_FALLOFFY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
#ifdef GAME_CARBON
				ImGui::InputFloat("Base Fog End", (float*)BASEFOGEND_NONPS2_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Base Fog Exponent", (float*)BASEFOGEXPONENT_NONPS2_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
#endif
#ifdef GAME_CARBON
				ImGui::InputFloat("Base Weather Fog", (float*)BASEWEATHERFOG_NONPS2_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Base Weather Fog (PS2 value)", (float*)BASEWEATHER_FOG_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
#else
				ImGui::InputFloat("Base Weather Fog)", (float*)BASEWEATHER_FOG_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
#endif
				ImGui::InputFloat("Base Weather Fog Start", (float*)BASEWEATHER_FOG_START_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				if (ImGui::CollapsingHeader("Base Weather Fog Colour", ImGuiTreeNodeFlags_None))
				{
					//ImGui::InputInt("R", (int*)BASEWEATHER_FOG_COLOUR_R_ADDR, 1, 100, ImGuiInputTextFlags_None);
					//ImGui::InputInt("G", (int*)BASEWEATHER_FOG_COLOUR_G_ADDR, 1, 100, ImGuiInputTextFlags_None);
					//ImGui::InputInt("B", (int*)BASEWEATHER_FOG_COLOUR_B_ADDR, 1, 100, ImGuiInputTextFlags_None);
					if (ImGui::ColorPicker3("", (float*)&(FogColourPicker.x), ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueWheel))
					{
						*(int*)BASEWEATHER_FOG_COLOUR_R_ADDR = (int)(FogColourPicker.x * 255);
						*(int*)BASEWEATHER_FOG_COLOUR_G_ADDR = (int)(FogColourPicker.y * 255);
						*(int*)BASEWEATHER_FOG_COLOUR_B_ADDR = (int)(FogColourPicker.z * 255);
					}
#ifdef GAME_CARBON
					ImGui::Separator();
#endif
				}
#ifdef GAME_CARBON
				//ImGui::InputFloat("Base Sky Fog Falloff", (float*)BASESKYFOGFALLOFF_ADDR, 0.001, 0.01, "%.6f", ImGuiInputTextFlags_CharsScientific);
				ImGui::SliderFloat("Base Sky Fog Falloff", (float*)BASESKYFOGFALLOFF_ADDR, -0.005, 0.005, "%.6f");
				ImGui::InputFloat("Base Sky Fog Offset", (float*)BASESKYFOGOFFSET_ADDR, 0.1, 1.00, "%.3f", ImGuiInputTextFlags_CharsScientific);
#endif
			}
			if (ImGui::CollapsingHeader("Clouds", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Rate of change", (float*)PRECIP_CLOUDSRATEOFCHANGE_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
			if (ImGui::CollapsingHeader("Rain", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Rain X", (float*)PRECIP_RAINX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Y", (float*)PRECIP_RAINY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Z", (float*)PRECIP_RAINZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Z Constant", (float*)PRECIP_RAINZCONSTANT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Radius X", (float*)PRECIP_RAINRADIUSX_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Radius Y", (float*)PRECIP_RAINRADIUSY_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Radius Z", (float*)PRECIP_RAINRADIUSZ_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Wind Effect", (float*)PRECIP_RAINWINDEFF_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Percentage", (float*)PRECIP_RAINPERCENT_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain in the headlights", (float*)PRECIP_RAININTHEHEADLIGHTS_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Rain Rate of change", (float*)PRECIP_RAINRATEOFCHANGE_ADDR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
			}
		}
#endif
#endif
#ifdef GAME_MW
		if (ImGui::CollapsingHeader("Visual Filter Control", ImGuiTreeNodeFlags_None))
		{

			if (*(int*)VISUALTREATMENT_INSTANCE_ADDR)
			{
				int visualFilterVars = 0;

				if (**(int**)VISUALTREATMENT_INSTANCE_ADDR == COPCAM_LOOK)
					visualFilterVars = (*(int*)VISUALTREATMENT_INSTANCE_ADDR) + 0x1AC;
				else
				{
					**(int**)VISUALTREATMENT_INSTANCE_ADDR = 0;
					visualFilterVars = (*(int*)VISUALTREATMENT_INSTANCE_ADDR) + 0x184;
				}
				if (visualFilterVars)
				{
					visualFilterVars = *(int*)(visualFilterVars + 8);
					if (ImGui::CollapsingHeader("Colour", ImGuiTreeNodeFlags_None))
					{
						ImGui::Checkbox("Cop Cam Look", *(bool**)VISUALTREATMENT_INSTANCE_ADDR);

						(VisualFilterColourPicker.x) = *(float*)(visualFilterVars + 0xC0);
						(VisualFilterColourPicker.y) = *(float*)(visualFilterVars + 0xC4);
						(VisualFilterColourPicker.z) = *(float*)(visualFilterVars + 0xC8);

						if (ImGui::ColorPicker3("", (float*)&(VisualFilterColourPicker.x), ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueWheel))
						{
							*(float*)(visualFilterVars + 0xC0) = (VisualFilterColourPicker.x) * VisualFilterColourMultiR;
							*(float*)(visualFilterVars + 0xC4) = (VisualFilterColourPicker.y) * VisualFilterColourMultiG;
							*(float*)(visualFilterVars + 0xC8) = (VisualFilterColourPicker.z) * VisualFilterColourMultiB;
						}
						ImGui::InputFloat("Multiplier R", &VisualFilterColourMultiR, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
						ImGui::InputFloat("Multiplier G", &VisualFilterColourMultiG, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
						ImGui::InputFloat("Multiplier B", &VisualFilterColourMultiB, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
						ImGui::Separator();
					}
					ImGui::InputFloat("Filter Colour Power", (float*)(visualFilterVars + 0xD0), 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
					ImGui::InputFloat("Saturation", (float*)(visualFilterVars + 0xD4), 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
					ImGui::InputFloat("Black Bloom", (float*)(visualFilterVars + 0xDC), 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
					if (ImGui::CollapsingHeader("Unknown values", ImGuiTreeNodeFlags_None))
					{
						ImGui::InputFloat("Unknown 1", (float*)(visualFilterVars + 0xCC), 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
						ImGui::InputFloat("Unknown 2", (float*)(visualFilterVars + 0xD8), 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
					}
				}
				else
					ImGui::TextUnformatted("ERROR: Cannot fetch visual filter values!");

				ImGui::Text("Base Address: 0x%X", visualFilterVars);
			}
		}
#endif
	}
	ImGui::Separator();
#ifndef OLD_NFS
#ifndef GAME_UC
	if (ImGui::CollapsingHeader("FMV", ImGuiTreeNodeFlags_None))
	{
#ifdef GAME_MW
		char* moviefilename_pointer = (char*)MOVIEFILENAME_ADDR;
		if (*(int*)GAMEFLOWMGR_STATUS_ADDR == GAMEFLOW_STATE_RACING)
			moviefilename_pointer = (char*)INGAMEMOVIEFILENAME_ADDR;
		ImGui::InputText("Movie Filename", moviefilename_pointer, 0x40);
#else
		ImGui::InputText("Movie Filename", MovieFilename, 0x40);
#endif
		if (ImGui::Button("Play Movie", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
#ifdef NFS_MULTITHREAD
			bDoPlayMovie = true;
#else
			PlayMovie();
#endif
		}
		if (ImGui::CollapsingHeader("Movie filename info (how to)", ImGuiTreeNodeFlags_None))
		{
			ImGui::PushTextWrapPos();
			ImGui::TextUnformatted("In the \"Movie Filename\" box only type in the name of the movie, not the full filename\nExample: If you want to play blacklist_01_english_ntsc.vp6, only type in the \"blacklist_01\", not the \"_english_ntsc.vp6\" part of it.\nWhen you go in-game, the input box will change its pointer to the in-game buffer.");
			ImGui::PopTextWrapPos();
		}
#ifdef GAME_MW
		ImGui::Separator();
		if (ImGui::CollapsingHeader("BlackList FMV", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Button("Blacklist 01", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_01");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 02", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_02");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 03", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_03");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 04", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_04");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 05", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_05");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 06", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_06");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 07", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_07");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 08", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_08");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 09", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_09");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 10", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_10");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 11", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_11");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 12", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_12");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 13", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_13");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 14", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_14");
				PlayMovie();
			}
			if (ImGui::Button("Blacklist 15", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "blacklist_15");
				PlayMovie();
			}
		}
		if (ImGui::CollapsingHeader("Story FMV", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Button("storyfmv_bla134", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_bla134");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_bus12", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_bus12");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_cro06_coh06a", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_cro06_coh06a");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_dda01", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_dda01");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_epi138", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_epi138");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_her136", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_her136");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_pin11", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_pin11");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_pol17_mot21", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_pol17_mot21");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_rap30", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_rap30");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_raz08", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_raz08");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_roc02", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_roc02");
				PlayMovie();
			}
			if (ImGui::Button("storyfmv_saf25", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "storyfmv_saf25");
				PlayMovie();
			}
		}
		if (ImGui::CollapsingHeader("Demo FMV", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Button("SSX OnTour Trailer", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "SX-OT-n-480");
				PlayMovie();
			}
			if (ImGui::Button("NBA Live 2006 Trailer", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "NBALive-2006-n");
				PlayMovie();
			}
			if (ImGui::Button("MarvNM_480", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "MarvNM-480");
				PlayMovie();
			}
		}
		if (ImGui::CollapsingHeader("Intro FMV", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Button("Intro", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "intro_movie");
				PlayMovie();
			}
			if (ImGui::Button("Attract", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "attract_movie");
				PlayMovie();
			}
			if (ImGui::Button("EA Logo", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "ealogo");
				PlayMovie();
			}
			if (ImGui::Button("PSA", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "psa");
				PlayMovie();
			}
		}
		if (ImGui::CollapsingHeader("Tutorial FMV", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Button("Drag Tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "drag_tutorial");
				PlayMovie();
			}
			if (ImGui::Button("Sprint Tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "sprint_tutorial");
				PlayMovie();
			}
			if (ImGui::Button("Tollbooth Tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "tollbooth_tutorial");
				PlayMovie();
			}
			if (ImGui::Button("Pursuit Tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "pursuit_tutorial");
				PlayMovie();
			}
			if (ImGui::Button("Bounty Tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				strcpy(moviefilename_pointer, "bounty_tutorial");
				PlayMovie();
			}
		}
#endif
	}
	ImGui::Separator();
#endif
#endif
#ifndef OLD_NFS
	if (ImGui::CollapsingHeader("Rub Test", ImGuiTreeNodeFlags_None))
	{
#ifndef GAME_MW
#ifdef GAME_PS
		ImGui::Checkbox("Draw World", &bDrawWorld);
#else
		ImGui::Checkbox("Draw World", (bool*)DRAWWORLD_ADDR); // TODO - find / make the toggle for MW
#endif
#endif
		ImGui::Checkbox("Draw Cars", (bool*)DRAWCARS_ADDR);
	}
	ImGui::Separator();
#endif
	if (ImGui::CollapsingHeader("Game", ImGuiTreeNodeFlags_None))
	{
#if defined(GAME_MW) || defined(GAME_UG2)
		if (ImGui::CollapsingHeader("Build Version", ImGuiTreeNodeFlags_None))
		{
#ifdef GAME_UG2
			ImGui::Text("Perforce Changelist Number: %d\nPerforce Changelist Name: %s\nBuild Machine: %s", *(int*)BUILDVERSIONCLNUMBER_ADDR, *(char**)BUILDVERSIONCLNAME_ADDR, *(char**)BUILDVERSIONMACHINE_ADDR); // TODO: make these flexible addresses...
#else
			ImGui::Text("Platform: %s\nBuild Type:%s%s\nPerforce Changelist Number: %d\nPerforce Changelist Name: %s\nBuild Date: %s\nBuild Machine: %s", (char*)BUILDVERSIONPLAT_ADDR, (char*)BUILDVERSIONNAME_ADDR, (char*)BUILDVERSIONOPTNAME_ADDR, *(int*)BUILDVERSIONCLNUMBER_ADDR, *(char**)BUILDVERSIONCLNAME_ADDR, *(char**)BUILDVERSIONDATE_ADDR, *(char**)BUILDVERSIONMACHINE_ADDR); // TODO: make these flexible addresses...
#endif
			ImGui::Separator();
		}
#endif
#ifdef HAS_DAL
		if (ImGui::CollapsingHeader("DAL Options", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::CollapsingHeader("FrontEnd", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("FE Scale", (float*)FESCALE_POINTER, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::Checkbox("Widescreen mode", (bool*)WIDESCREEN_POINTER);
#ifdef GAME_CARBON
				ImGui::Separator();
				ImGui::Checkbox("Lap Info", (bool*)FELAPINFO_POINTER);
				ImGui::Checkbox("Score", (bool*)FESCORE_POINTER);
				ImGui::Checkbox("Leaderboard", (bool*)FELEADERBOARD_POINTER);
				ImGui::Checkbox("Crew Info", (bool*)FECREWINFO_POINTER);
				ImGui::Checkbox("Transmission Prompt", (bool*)FETRANSMISSIONPROMPT_POINTER);
				ImGui::Checkbox("Rearview Mirror", (bool*)FERVM_POINTER);
				ImGui::Checkbox("Picture In Picture", (bool*)FEPIP_POINTER);
				ImGui::Checkbox("Metric Speedo Units", (bool*)SPEEDOUNIT_POINTER);
#endif
			}
#ifdef GAME_CARBON
			if (ImGui::CollapsingHeader("Input", ImGuiTreeNodeFlags_None))
			{
				ImGui::Checkbox("Rumble", (bool*)RUMBLEON_POINTER);
				ImGui::InputInt("PC Pad Index", (int*)PCPADIDX_POINTER, 1, 100);
				ImGui::InputInt("PC Device Type", (int*)PCDEVTYPE_POINTER, 1, 100);
			}
#endif
			if (ImGui::CollapsingHeader("Gameplay", ImGuiTreeNodeFlags_None))
			{
				ImGui::InputFloat("Highlight Cam", (float*)HIGHLIGHTCAM_POINTER, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
				ImGui::InputFloat("Time Of Day", (float*)TIMEOFDAY_POINTER, 0.1, 1.0, "%.3f", ImGuiInputTextFlags_CharsScientific);
#ifdef GAME_CARBON
				ImGui::Checkbox("Jump Cam", (bool*)JUMPCAM_POINTER);
				ImGui::Checkbox("Car Damage", (bool*)DAMAGEON_POINTER);
				ImGui::Checkbox("Autosave", (bool*)AUTOSAVEON_POINTER);
#endif
			}
			if (ImGui::CollapsingHeader("Audio Levels", ImGuiTreeNodeFlags_None))
			{
				ImGui::SliderFloat("Master", (float*)MASTERVOL_POINTER, 0.0, 1.0);
				ImGui::SliderFloat("Speech", (float*)SPEECHVOL_POINTER, 0.0, 1.0);
				ImGui::SliderFloat("FE Music", (float*)FEMUSICVOL_POINTER, 0.0, 1.0);
				ImGui::SliderFloat("IG Music", (float*)IGMUSICVOL_POINTER, 0.0, 1.0);
				ImGui::SliderFloat("Sound Effects", (float*)SOUNDEFFECTSVOL_POINTER, 0.0, 1.0);
				ImGui::SliderFloat("Engine", (float*)ENGINEVOL_POINTER, 0.0, 1.0);
				ImGui::SliderFloat("Car", (float*)CARVOL_POINTER, 0.0, 1.0);
				ImGui::SliderFloat("Ambient", (float*)AMBIENTVOL_POINTER, 0.0, 1.0);
				ImGui::SliderFloat("Speed", (float*)SPEEDVOL_POINTER, 0.0, 1.0);
			}

		}
		ImGui::Separator();
#endif
		if (ImGui::Button("Exit Game", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
			*(int*)EXITGAMEFLAG_ADDR = 1;
		}
	}
	ImGui::Separator();
	ImGui::Text("GameFlow State: %s", GameFlowStateNames[*(int*)GAMEFLOWMGR_STATUS_ADDR]);
	ImGui::Separator();
	if (modified)
		save_config();
}
// NFS CODE END
void reshade::runtime::draw_code_editor()
{
	if (ImGui::Button(ICON_SAVE " Save", ImVec2(ImGui::GetContentRegionAvail().x, 0)) || _input->is_key_pressed('S', true, false, false))
	{
		// Write current editor text to file
		const std::string text = _editor.get_text();
		std::ofstream(_editor_file, std::ios::trunc).write(text.c_str(), text.size());

		if (!is_loading() && _selected_effect < _effects.size())
		{
			// Backup effect file path before unloading
			const std::filesystem::path source_file = _effects[_selected_effect].source_file;

			// Hide splash bar when reloading a single effect file
			_show_splash = false;

			// Reload effect file
			unload_effect(_selected_effect);
			load_effect(source_file, ini_file::load_cache(_current_preset_path), _selected_effect);

			// Re-open current file so that errors are updated
			open_file_in_code_editor(_selected_effect, _editor_file);

			// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
			ImGui::FindWindowByName("Statistics")->DrawList->CmdBuffer.clear();
		}
	}

	_editor.render("##editor", false, _imgui_context->IO.Fonts->Fonts[1]);

	// Disable keyboard shortcuts when the window is focused so they don't get triggered while editing text
	const bool is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	_ignore_shortcuts |= is_focused;

	// Disable keyboard navigation starting with next frame when editor is focused so that the Alt key can be used without it switching focus to the menu bar
	if (is_focused)
		_imgui_context->IO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
	else // Enable navigation again if focus is lost
		_imgui_context->IO.ConfigFlags |=  ImGuiConfigFlags_NavEnableKeyboard;
}
void reshade::runtime::draw_code_viewer()
{
	_viewer.render("##viewer", false, _imgui_context->IO.Fonts->Fonts[1]);

	const bool is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	_ignore_shortcuts |= is_focused;

	if (is_focused)
		_imgui_context->IO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
	else
		_imgui_context->IO.ConfigFlags |=  ImGuiConfigFlags_NavEnableKeyboard;
}

void reshade::runtime::draw_variable_editor()
{
	const ImVec2 popup_pos = ImGui::GetCursorScreenPos() + ImVec2(std::max(0.f, ImGui::GetWindowContentRegionWidth() * 0.5f - 200.0f), ImGui::GetFrameHeightWithSpacing());

	if (widgets::popup_button("Edit global preprocessor definitions", ImGui::GetContentRegionAvail().x, ImGuiWindowFlags_NoMove))
	{
		ImGui::SetWindowPos(popup_pos);

		bool modified = false;
		float popup_height = (std::max(_global_preprocessor_definitions.size(), _preset_preprocessor_definitions.size()) + 2) * ImGui::GetFrameHeightWithSpacing();
		popup_height = std::min(popup_height, _window_height - popup_pos.y - 20.0f);
		popup_height = std::max(popup_height, 42.0f); // Ensure window always has a minimum height
		const float button_size = ImGui::GetFrameHeight();
		const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;

		ImGui::BeginChild("##definitions", ImVec2(400.0f, popup_height), false, ImGuiWindowFlags_NoScrollWithMouse);

		if (ImGui::BeginTabBar("##definition_types", ImGuiTabBarFlags_NoTooltip))
		{
			if (ImGui::BeginTabItem("Global"))
			{
				for (size_t i = 0; i < _global_preprocessor_definitions.size(); ++i)
				{
					char name[128] = "";
					char value[128] = "";

					const size_t equals_index = _global_preprocessor_definitions[i].find('=');
					_global_preprocessor_definitions[i].copy(name, std::min(equals_index, sizeof(name) - 1));
					if (equals_index != std::string::npos)
						_global_preprocessor_definitions[i].copy(value, sizeof(value) - 1, equals_index + 1);

					ImGui::PushID(static_cast<int>(i));

					ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() * 0.66666666f - (button_spacing));
					modified |= ImGui::InputText("##name", name, sizeof(name), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter,
						[](ImGuiInputTextCallbackData *data) -> int { return data->EventChar == '=' || (data->EventChar != '_' && !isalnum(data->EventChar)); }); // Filter out invalid characters
					ImGui::PopItemWidth();

					ImGui::SameLine(0, button_spacing);

					ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() * 0.33333333f - (button_spacing + button_size) + 1);
					modified |= ImGui::InputText("##value", value, sizeof(value));
					ImGui::PopItemWidth();

					ImGui::SameLine(0, button_spacing);

					if (ImGui::Button("-", ImVec2(button_size, 0)))
					{
						modified = true;
						_global_preprocessor_definitions.erase(_global_preprocessor_definitions.begin() + i--);
					}
					else if (modified)
					{
						_global_preprocessor_definitions[i] = std::string(name) + '=' + std::string(value);
					}

					ImGui::PopID();
				}

				ImGui::Dummy(ImVec2());
				ImGui::SameLine(0, ImGui::GetWindowContentRegionWidth() - button_size);
				if (ImGui::Button("+", ImVec2(button_size, 0)))
					_global_preprocessor_definitions.emplace_back();

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Current Preset"))
			{
				for (size_t i = 0; i < _preset_preprocessor_definitions.size(); ++i)
				{
					char name[128] = "";
					char value[128] = "";

					const size_t equals_index = _preset_preprocessor_definitions[i].find('=');
					_preset_preprocessor_definitions[i].copy(name, std::min(equals_index, sizeof(name) - 1));
					if (equals_index != std::string::npos)
						_preset_preprocessor_definitions[i].copy(value, sizeof(value) - 1, equals_index + 1);

					ImGui::PushID(static_cast<int>(i));

					ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() * 0.66666666f - (button_spacing));
					modified |= ImGui::InputText("##name", name, sizeof(name), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter,
						[](ImGuiInputTextCallbackData *data) -> int { return data->EventChar == '=' || (data->EventChar != '_' && !isalnum(data->EventChar)); }); // Filter out invalid characters
					ImGui::PopItemWidth();

					ImGui::SameLine(0, button_spacing);

					ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() * 0.33333333f - (button_spacing + button_size) + 1);
					modified |= ImGui::InputText("##value", value, sizeof(value), ImGuiInputTextFlags_AutoSelectAll);
					ImGui::PopItemWidth();

					ImGui::SameLine(0, button_spacing);

					if (ImGui::Button("-", ImVec2(button_size, 0)))
					{
						modified = true;
						_preset_preprocessor_definitions.erase(_preset_preprocessor_definitions.begin() + i--);
					}
					else if (modified)
					{
						_preset_preprocessor_definitions[i] = std::string(name) + '=' + std::string(value);
					}

					ImGui::PopID();
				}

				ImGui::Dummy(ImVec2());
				ImGui::SameLine(0, ImGui::GetWindowContentRegionWidth() - button_size);
				if (ImGui::Button("+", ImVec2(button_size, 0)))
					_preset_preprocessor_definitions.emplace_back();

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::EndChild();

		if (modified)
		{
			save_config();
			save_current_preset();
			_was_preprocessor_popup_edited = true;
		}

		ImGui::EndPopup();
	}
	else if (_was_preprocessor_popup_edited)
	{
		load_effects();
		_was_preprocessor_popup_edited = false;
	}

	const auto find_definition_value = [](auto &list, const auto &name, char value[128] = nullptr)
	{
		for (auto it = list.begin(); it != list.end(); ++it)
		{
			char current_name[128] = "";
			const size_t equals_index = it->find('=');
			it->copy(current_name, std::min(equals_index, sizeof(current_name) - 1));

			if (name == current_name)
			{
				if (equals_index != std::string::npos && value != nullptr)
					value[it->copy(value, 127, equals_index + 1)] = '\0';
				return it;
			}
		}

		return list.end();
	};

	ImGui::BeginChild("##variables");
	if (_variable_editor_tabs)
		ImGui::BeginTabBar("##variables");

	for (size_t effect_index = 0, id = 0; effect_index < _effects.size(); ++effect_index)
	{
		reshade::effect &effect = _effects[effect_index];

		// Hide variables that are not currently used in any of the active effects
		// Also skip showing this effect in the variable list if it doesn't have any uniform variables to show
		if (!effect.rendering || (effect.uniforms.empty() && effect.definitions.empty() && effect.preprocessed))
			continue;
		assert(effect.compiled);

		bool reload_effect = false;
		const bool is_focused = _focused_effect == effect_index;
		const std::string effect_name = effect.source_file.filename().u8string();

		// Create separate tab for every effect file
		if (_variable_editor_tabs)
		{
			ImGuiTabItemFlags flags = 0;
			if (is_focused)
				flags |= ImGuiTabItemFlags_SetSelected;

			if (!ImGui::BeginTabItem(effect_name.c_str(), nullptr, flags))
				continue;
			// Begin a new child here so scrolling through variables does not move the tab itself too
			ImGui::BeginChild("##tab");
		}
		else
		{
			if (is_focused || _effects_expanded_state & 1)
				ImGui::SetNextItemOpen(is_focused || (_effects_expanded_state >> 1) != 0);

			if (!ImGui::TreeNodeEx(effect_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				continue; // Skip rendering invisible items
		}

		if (is_focused)
		{
			ImGui::SetScrollHereY(0.0f);
			_focused_effect = std::numeric_limits<size_t>::max();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(_imgui_context->Style.FramePadding.x, 0));
		if (widgets::popup_button(ICON_RESET " Reset all to default", _variable_editor_tabs ? ImGui::GetContentRegionAvail().x : ImGui::CalcItemWidth()))
		{
			ImGui::Text("Do you really want to reset all values in '%s' to their defaults?", effect_name.c_str());

			if (ImGui::Button("Yes", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				// Reset all uniform variables
				for (uniform &variable_it : effect.uniforms)
					reset_uniform_value(variable_it);

				// Reset all preprocessor definitions
				for (const std::pair<std::string, std::string> &definition : effect.definitions)
					if (const auto preset_it = find_definition_value(_preset_preprocessor_definitions, definition.first);
						preset_it != _preset_preprocessor_definitions.end())
						reload_effect = true, // Need to reload after changing preprocessor defines so to get accurate defaults again
						_preset_preprocessor_definitions.erase(preset_it);

				save_current_preset();

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();

		bool category_closed = false;
		std::string current_category;
		auto modified_definition = _preset_preprocessor_definitions.end();

		size_t active_variable = 0;
		size_t active_variable_index = std::numeric_limits<size_t>::max();
		size_t hovered_variable = 0;
		size_t hovered_variable_index = std::numeric_limits<size_t>::max();

		for (size_t variable_index = 0; variable_index < effect.uniforms.size(); ++variable_index)
		{
			reshade::uniform &variable = effect.uniforms[variable_index];

			// Skip hidden and special variables
			if (variable.annotation_as_int("hidden") || variable.special != special_uniform::none)
			{
				if (variable.special == special_uniform::overlay_active)
					active_variable_index = variable_index;
				else if (variable.special == special_uniform::overlay_hovered)
					hovered_variable_index = variable_index;
				continue;
			}

			if (const std::string_view category = variable.annotation_as_string("ui_category");
				category != current_category)
			{
				current_category = category;

				if (!category.empty())
				{
					std::string category_label(category.data(), category.size());
					if (!_variable_editor_tabs)
						for (float x = 0, space_x = ImGui::CalcTextSize(" ").x, width = (ImGui::CalcItemWidth() - ImGui::CalcTextSize(category_label.data()).x - 45) / 2; x < width; x += space_x)
							category_label.insert(0, " ");

					ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen;
					if (!variable.annotation_as_int("ui_category_closed"))
						flags |= ImGuiTreeNodeFlags_DefaultOpen;

					category_closed = !ImGui::TreeNodeEx(category_label.c_str(), flags);

					if (ImGui::BeginPopupContextItem(category_label.c_str()))
					{
						std::string reset_button_label(category.data(), category.size());
						reset_button_label = ICON_RESET " Reset all in '" + reset_button_label + "' to default";

						if (ImGui::Button(reset_button_label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)))
						{
							for (uniform &variable_it : effect.uniforms)
								if (variable_it.annotation_as_string("ui_category") == category)
									reset_uniform_value(variable_it);

							save_current_preset();

							ImGui::CloseCurrentPopup();
						}

						ImGui::EndPopup();
					}
				}
				else
				{
					category_closed = false;
				}
			}

			// Skip rendering invisible items
			if (category_closed)
				continue;

			// Add spacing before variable widget
			for (int i = 0, spacing = variable.annotation_as_int("ui_spacing"); i < spacing; ++i)
				ImGui::Spacing();

			// Add user-configurable text before variable widget
			if (const std::string_view text = variable.annotation_as_string("ui_text");
				!text.empty())
			{
				ImGui::PushTextWrapPos();
				ImGui::TextUnformatted(text.data());
				ImGui::PopTextWrapPos();
			}

			bool modified = false;
			std::string_view label = variable.annotation_as_string("ui_label");
			if (label.empty())
				label = variable.name;
			const std::string_view ui_type = variable.annotation_as_string("ui_type");

			ImGui::PushID(static_cast<int>(id++));

			switch (variable.type.base)
			{
			case reshadefx::type::t_bool:
			{
				bool data;
				get_uniform_value(variable, &data, 1);

				if (ui_type == "combo")
					modified = widgets::combo_with_buttons(label.data(), data);
				else
					modified = ImGui::Checkbox(label.data(), &data);

				if (modified)
					set_uniform_value(variable, &data, 1);
				break;
			}
			case reshadefx::type::t_int:
			case reshadefx::type::t_uint:
			{
				int data[16];
				get_uniform_value(variable, data, 16);

				const auto ui_min_val = variable.annotation_as_int("ui_min", 0, std::numeric_limits<int>::lowest());
				const auto ui_max_val = variable.annotation_as_int("ui_max", 0, std::numeric_limits<int>::max());
				const auto ui_stp_val = std::max(1, variable.annotation_as_int("ui_step"));

				if (ui_type == "slider")
					modified = widgets::slider_with_buttons(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val);
				else if (ui_type == "drag")
					modified = variable.annotation_as_int("ui_step") == 0 ?
						ImGui::DragScalarN(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, 1.0f, &ui_min_val, &ui_max_val) :
						widgets::drag_with_buttons(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val);
				else if (ui_type == "list")
					modified = widgets::list_with_buttons(label.data(), variable.annotation_as_string("ui_items"), data[0]);
				else if (ui_type == "combo")
					modified = widgets::combo_with_buttons(label.data(), variable.annotation_as_string("ui_items"), data[0]);
				else if (ui_type == "radio")
					modified = widgets::radio_list(label.data(), variable.annotation_as_string("ui_items"), data[0]);
				else if (variable.type.is_matrix())
					for (unsigned int row = 0; row < variable.type.rows; ++row)
						modified = ImGui::InputScalarN((std::string(label) + " [row " + std::to_string(row) + ']').c_str(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, &data[0] + row * variable.type.cols, variable.type.cols) || modified;
				else
					modified = ImGui::InputScalarN(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows);

				if (modified)
					set_uniform_value(variable, data, 16);
				break;
			}
			case reshadefx::type::t_float:
			{
				float data[16];
				get_uniform_value(variable, data, 16);

				const auto ui_min_val = variable.annotation_as_float("ui_min", 0, std::numeric_limits<float>::lowest());
				const auto ui_max_val = variable.annotation_as_float("ui_max", 0, std::numeric_limits<float>::max());
				const auto ui_stp_val = std::max(0.001f, variable.annotation_as_float("ui_step"));

				// Calculate display precision based on step value
				char precision_format[] = "%.0f";
				for (float x = 1.0f; x * ui_stp_val < 1.0f && precision_format[2] < '9'; x *= 10.0f)
					++precision_format[2]; // This changes the text to "%.1f", "%.2f", "%.3f", ...

				if (ui_type == "slider")
					modified = widgets::slider_with_buttons(label.data(), ImGuiDataType_Float, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, precision_format);
				else if (ui_type == "drag")
					modified = variable.annotation_as_float("ui_step") == 0 ?
						ImGui::DragScalarN(label.data(), ImGuiDataType_Float, data, variable.type.rows, ui_stp_val, &ui_min_val, &ui_max_val, precision_format) :
						widgets::drag_with_buttons(label.data(), ImGuiDataType_Float, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, precision_format);
				else if (ui_type == "color" && variable.type.rows == 1)
					modified = widgets::slider_for_alpha_value(label.data(), data);
				else if (ui_type == "color" && variable.type.rows == 3)
					modified = ImGui::ColorEdit3(label.data(), data, ImGuiColorEditFlags_NoOptions);
				else if (ui_type == "color" && variable.type.rows == 4)
					modified = ImGui::ColorEdit4(label.data(), data, ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaBar);
				else if (variable.type.is_matrix())
					for (unsigned int row = 0; row < variable.type.rows; ++row)
						modified = ImGui::InputScalarN((std::string(label) + " [row " + std::to_string(row) + ']').c_str(), ImGuiDataType_Float, &data[0] + row * variable.type.cols, variable.type.cols) || modified;
				else
					modified = ImGui::InputScalarN(label.data(), ImGuiDataType_Float, data, variable.type.rows);

				if (modified)
					set_uniform_value(variable, data, 16);
				break;
			}
			}

			if (ImGui::IsItemActive())
				active_variable = variable_index + 1;
			if (ImGui::IsItemHovered())
				hovered_variable = variable_index + 1;

			// Display tooltip
			if (const std::string_view tooltip = variable.annotation_as_string("ui_tooltip");
				!tooltip.empty() && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", tooltip.data());

			// Create context menu
			if (ImGui::BeginPopupContextItem("##context"))
			{
				if (variable.supports_toggle_key() &&
					widgets::key_input_box("##toggle_key", variable.toggle_key_data, *_input))
					modified = true;

				const float button_width = ImGui::CalcItemWidth();

				if (ImGui::Button(ICON_RESET " Reset to default", ImVec2(button_width, 0)))
				{
					modified = true;
					reset_uniform_value(variable);
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if (variable.toggle_key_data[0] != 0)
			{
				ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 120);
				ImGui::TextDisabled("%s", reshade::input::key_name(variable.toggle_key_data).c_str());
			}

			ImGui::PopID();

			// A value has changed, so save the current preset
			if (modified)
				save_current_preset();
		}

		if (active_variable_index < effect.uniforms.size())
			set_uniform_value(effect.uniforms[active_variable_index], static_cast<uint32_t>(active_variable));
		if (hovered_variable_index < effect.uniforms.size())
			set_uniform_value(effect.uniforms[hovered_variable_index], static_cast<uint32_t>(hovered_variable));

		// Draw preprocessor definition list after all uniforms of an effect file
		if (!effect.definitions.empty() || !effect.preprocessed)
		{
			std::string category_label = "Preprocessor definitions";
			if (!_variable_editor_tabs)
				for (float x = 0, space_x = ImGui::CalcTextSize(" ").x, width = (ImGui::CalcItemWidth() - ImGui::CalcTextSize(category_label.c_str()).x - 45) / 2; x < width; x += space_x)
					category_label.insert(0, " ");

			ImGuiTreeNodeFlags tree_flags = ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (effect.preprocessed) // Do not open tree by default is not yet pre-processed, since that would case an immediate recompile
				tree_flags |= ImGuiTreeNodeFlags_DefaultOpen;

			if (ImGui::TreeNodeEx(category_label.c_str(), tree_flags))
			{
				if (!effect.preprocessed)
					reload_effect = true;

				for (const std::pair<std::string, std::string> &definition : effect.definitions)
				{
					char value[128] = "";
					const auto global_it = find_definition_value(_global_preprocessor_definitions, definition.first, value);
					const auto preset_it = find_definition_value(_preset_preprocessor_definitions, definition.first, value);

					if (global_it == _global_preprocessor_definitions.end() &&
						preset_it == _preset_preprocessor_definitions.end())
						definition.second.copy(value, sizeof(value) - 1); // Fill with default value

					if (ImGui::InputText(definition.first.c_str(), value, sizeof(value),
						global_it != _global_preprocessor_definitions.end() ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
					{
						if (value[0] == '\0') // An empty value removes the definition
						{
							if (preset_it != _preset_preprocessor_definitions.end())
							{
								reload_effect = true;
								_preset_preprocessor_definitions.erase(preset_it);
							}
						}
						else
						{
							reload_effect = true;

							if (preset_it != _preset_preprocessor_definitions.end())
							{
								*preset_it = definition.first + '=' + value;
								modified_definition = preset_it;
							}
							else
							{
								_preset_preprocessor_definitions.push_back(definition.first + '=' + value);
								modified_definition = _preset_preprocessor_definitions.end() - 1;
							}
						}
					}

					if (!reload_effect && // Cannot compare iterators if definitions were just modified above
						ImGui::BeginPopupContextItem())
					{
						const float button_width = ImGui::CalcItemWidth();

						if (ImGui::Button(ICON_RESET " Reset to default", ImVec2(button_width, 0)))
						{
							if (preset_it != _preset_preprocessor_definitions.end())
							{
								reload_effect = true;
								_preset_preprocessor_definitions.erase(preset_it);
							}

							ImGui::CloseCurrentPopup();
						}

						ImGui::EndPopup();
					}
				}
			}
		}

		if (_variable_editor_tabs)
		{
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		else
		{
			ImGui::TreePop();
		}

		if (reload_effect)
		{
			save_current_preset();

			const bool reload_successful_before = _last_shader_reload_successfull;

			// Backup effect file path before unloading
			const std::filesystem::path source_file = effect.source_file;

			// Reload current effect file
			unload_effect(effect_index);
			if (!load_effect(source_file, ini_file::load_cache(_current_preset_path), effect_index, true) &&
				modified_definition != _preset_preprocessor_definitions.end())
			{
				// The preprocessor definition that was just modified caused the shader to not compile, so reset to default and try again
				_preset_preprocessor_definitions.erase(modified_definition);

				unload_effect(effect_index);
				if (load_effect(source_file, ini_file::load_cache(_current_preset_path), effect_index, true))
				{
					_last_shader_reload_successfull = reload_successful_before;
					ImGui::OpenPopup("##pperror"); // Notify the user about this

					// Update preset again now, so that the removed preprocessor definition does not reappear on a reload
					// The preset is actually loaded again next frame to update the technique status (see 'update_and_render_effects'), so cannot use 'save_current_preset' here
					ini_file::load_cache(_current_preset_path).set({}, "PreprocessorDefinitions", _preset_preprocessor_definitions);
				}

				// Re-open file in editor so that errors are updated
				if (_selected_effect == effect_index)
					open_file_in_code_editor(_selected_effect, _editor_file);
			}

			// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
			ImGui::FindWindowByName("Statistics")->DrawList->CmdBuffer.clear();
		}
	}

	if (ImGui::BeginPopup("##pperror"))
	{
		ImGui::TextColored(COLOR_RED, "The shader failed to compile after this change, so it was reverted back to the default.");
		ImGui::EndPopup();
	}

	if (_variable_editor_tabs)
		ImGui::EndTabBar();
	ImGui::EndChild();
}
void reshade::runtime::draw_technique_editor()
{
	bool reload_required = false;

	if (_effect_load_skipping && _show_force_load_effects_button)
	{
		if (size_t skipped_effects = std::count_if(_effects.begin(), _effects.end(),
			[](const effect &effect) { return effect.skipped; }); skipped_effects > 0)
		{
			char buf[60];
			ImFormatString(buf, ARRAYSIZE(buf), "Force load all effects (%lu remaining)", skipped_effects);
			reload_required = ImGui::ButtonEx(buf, ImVec2(ImGui::GetWindowContentRegionWidth(), 0));
		}
	}

	size_t hovered_technique_index = std::numeric_limits<size_t>::max();

	for (size_t index = 0; index < _techniques.size(); ++index)
	{
		reshade::technique &technique = _techniques[index];

		// Skip hidden techniques
		if (technique.hidden)
			continue;

		ImGui::PushID(static_cast<int>(index));

		// Look up effect that contains this technique
		const reshade::effect &effect = _effects[technique.effect_index];

		// Draw border around the item if it is selected
		const bool draw_border = _selected_technique == index;
		if (draw_border)
			ImGui::Separator();

		const bool clicked = _imgui_context->IO.MouseClicked[0];
		assert(effect.compiled || !technique.enabled);

		// Prevent user from enabling the technique when the effect failed to compile
		// Also prevent disabling it for when the technique is set to always be enabled via annotation
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !effect.compiled || technique.annotation_as_int("enabled"));
		// Gray out disabled techniques and mark techniques which failed to compile red
		ImGui::PushStyleColor(ImGuiCol_Text,
			effect.compiled ?
				effect.errors.empty() || technique.enabled ?
					_imgui_context->Style.Colors[technique.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled] :
					COLOR_YELLOW :
				COLOR_RED);

		std::string label(technique.annotation_as_string("ui_label"));
		if (label.empty() || !effect.compiled)
			label = technique.name;
		label += " [" + effect.source_file.filename().u8string() + ']' + (!effect.compiled ? " failed to compile" : "");

		if (bool status = technique.enabled;
			ImGui::Checkbox(label.data(), &status))
		{
			if (status)
				enable_technique(technique);
			else
				disable_technique(technique);
			save_current_preset();
		}

		ImGui::PopStyleColor();
		ImGui::PopItemFlag();

		if (ImGui::IsItemActive())
			_selected_technique = index;
		if (ImGui::IsItemClicked())
			_focused_effect = technique.effect_index;
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
			hovered_technique_index = index;

		// Display tooltip
		if (const std::string_view tooltip = technique.annotation_as_string("ui_tooltip");
			ImGui::IsItemHovered() && (!tooltip.empty() || !effect.errors.empty()))
		{
			ImGui::BeginTooltip();
			if (!tooltip.empty())
			{
				ImGui::TextUnformatted(tooltip.data());
				ImGui::Spacing();
			}
			if (!effect.errors.empty())
			{
				ImGui::PushStyleColor(ImGuiCol_Text, effect.compiled ? COLOR_YELLOW : COLOR_RED);
				ImGui::TextUnformatted(effect.errors.c_str());
				ImGui::PopStyleColor();
			}
			ImGui::EndTooltip();
		}

		// Create context menu
		if (ImGui::BeginPopupContextItem("##context"))
		{
			ImGui::TextUnformatted(technique.name.c_str());
			ImGui::Separator();

			if (widgets::key_input_box("##toggle_key", technique.toggle_key_data, *_input))
				save_current_preset();

			const bool is_not_top = index > 0;
			const bool is_not_bottom = index < _techniques.size() - 1;
			const float button_width = ImGui::CalcItemWidth();

			if (is_not_top && ImGui::Button("Move to top", ImVec2(button_width, 0)))
			{
				_techniques.insert(_techniques.begin(), std::move(_techniques[index]));
				_techniques.erase(_techniques.begin() + 1 + index);
				save_current_preset();
				ImGui::CloseCurrentPopup();
			}
			if (is_not_bottom && ImGui::Button("Move to bottom", ImVec2(button_width, 0)))
			{
				_techniques.push_back(std::move(_techniques[index]));
				_techniques.erase(_techniques.begin() + index);
				save_current_preset();
				ImGui::CloseCurrentPopup();
			}

			ImGui::Separator();

			if (ImGui::Button("Open folder in explorer", ImVec2(button_width, 0)))
			{
				// Use absolute path to explorer to avoid potential security issues when executable is replaced
				WCHAR explorer_path[260] = L"";
				GetWindowsDirectoryW(explorer_path, ARRAYSIZE(explorer_path));
				wcscat_s(explorer_path, L"\\explorer.exe");

				ShellExecuteW(nullptr, L"open", explorer_path, (L"/select,\"" + effect.source_file.wstring() + L"\"").c_str(), nullptr, SW_SHOWDEFAULT);
			}

			ImGui::Separator();

			if (widgets::popup_button(ICON_EDIT " Edit source code", button_width))
			{
				std::filesystem::path source_file;
				if (ImGui::MenuItem(effect.source_file.filename().u8string().c_str()))
					source_file = effect.source_file;

				if (!effect.included_files.empty())
				{
					ImGui::Separator();

					for (const std::filesystem::path &included_file : effect.included_files)
					{
						if (ImGui::MenuItem(included_file.filename().u8string().c_str()))
						{
							source_file = included_file;
						}
					}
				}

				ImGui::EndPopup();

				if (!source_file.empty())
				{
					open_file_in_code_editor(technique.effect_index, source_file);
					ImGui::CloseCurrentPopup();
				}
			}

			if (!effect.module.hlsl.empty() && // Hide if using SPIR-V, since that cannot easily be shown here
				widgets::popup_button("Show compiled results", button_width))
			{
				std::string source_code;
				if (ImGui::MenuItem("Generated code"))
				{
					source_code = effect.preamble + effect.module.hlsl;
					_viewer_entry_point.clear();
				}

				if (!effect.assembly.empty())
				{
					ImGui::Separator();

					for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
					{
						if (const auto assembly_it = effect.assembly.find(entry_point.name);
							assembly_it != effect.assembly.end() && ImGui::MenuItem(entry_point.name.c_str()))
						{
							source_code = assembly_it->second;
							_viewer_entry_point = entry_point.name;
						}
					}
				}

				ImGui::EndPopup();

				if (!source_code.empty())
				{
					_show_code_viewer = true;
					_viewer.set_text(source_code);
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		if (technique.toggle_key_data[0] != 0 && effect.compiled)
		{
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 120);
			ImGui::TextDisabled("%s", reshade::input::key_name(technique.toggle_key_data).c_str());
		}

		if (draw_border)
			ImGui::Separator();

		ImGui::PopID();
	}

	// Move the selected technique to the position of the mouse in the list
	if (_selected_technique < _techniques.size() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		if (hovered_technique_index < _techniques.size() && hovered_technique_index != _selected_technique)
		{
			const auto move_technique = [this](size_t from_index, size_t to_index) {
				if (to_index < from_index) // Up
					for (size_t i = from_index; to_index < i; --i)
						std::swap(_techniques[i - 1], _techniques[i]);
				else // Down
					for (size_t i = from_index; i < to_index; ++i)
						std::swap(_techniques[i], _techniques[i + 1]);
			};

			move_technique(_selected_technique, hovered_technique_index);

			// Pressing shift moves all techniques from the same effect file to the new location as well
			if (ImGui::GetIO().KeyShift)
			{
				for (size_t i = hovered_technique_index + 1, offset = 1; i < _techniques.size(); ++i)
				{
					if (_techniques[i].effect_index == _focused_effect)
					{
						if ((i - hovered_technique_index) > offset)
							move_technique(i, hovered_technique_index + offset);
						offset++;
					}
				}
				for (size_t i = hovered_technique_index - 1, offset = 0; i >= 0 && i != std::numeric_limits<size_t>::max(); --i)
				{
					if (_techniques[i].effect_index == _focused_effect)
					{
						offset++;
						if ((hovered_technique_index - i) > offset)
							move_technique(i, hovered_technique_index - offset);
					}
				}
			}

			_selected_technique = hovered_technique_index;
			save_current_preset();
			return;
		}
	}
	else
	{
		_selected_technique = std::numeric_limits<size_t>::max();
	}

	if (reload_required)
	{
		_load_option_disable_skipping = true;
		load_effects();
	}
}

void reshade::runtime::open_file_in_code_editor(size_t effect_index, const std::filesystem::path &path)
{
	_selected_effect = effect_index;

	if (effect_index >= _effects.size())
	{
		_editor.clear_text();
		_editor.set_readonly(true);
		_editor_file.clear();
		return;
	}

	// Force code editor to become visible
	_show_code_editor = true;

	// Only reload text if another file is opened (to keep undo history intact)
	if (path != _editor_file)
	{
		_editor_file = path;

		// Load file to string and update editor text
		_editor.set_text(std::string(std::istreambuf_iterator<char>(std::ifstream(path).rdbuf()), std::istreambuf_iterator<char>()));
		_editor.set_readonly(false);
	}

	// Update generated code in viewer after a reload
	if (_show_code_viewer && _viewer_entry_point.empty())
	{
		_viewer.set_text(_effects[effect_index].preamble + _effects[effect_index].module.hlsl);
	}

	_editor.clear_errors();
	const std::string &errors = _effects[effect_index].errors;

	for (size_t offset = 0, next; offset != std::string::npos; offset = next)
	{
		const size_t pos_error = errors.find(": ", offset);
		const size_t pos_error_line = errors.rfind('(', pos_error); // Paths can contain '(', but no ": ", so search backwards from th error location to find the line info
		if (pos_error == std::string::npos || pos_error_line == std::string::npos)
			break;

		const size_t pos_linefeed = errors.find('\n', pos_error);

		next = pos_linefeed != std::string::npos ? pos_linefeed + 1 : std::string::npos;

		// Ignore errors that aren't in the current source file
		if (const std::string_view error_file(errors.c_str() + offset, pos_error_line - offset);
			error_file != path.u8string())
			continue;

		const int error_line = std::strtol(errors.c_str() + pos_error_line + 1, nullptr, 10);
		const std::string error_text = errors.substr(pos_error + 2 /* skip space */, pos_linefeed - pos_error - 2);

		_editor.add_error(error_line, error_text, error_text.find("warning") != std::string::npos);
	}
}

#endif
