
#include "RealExecutionMain.h"
#include "LaunchEngineLoop.h"
#include "StandaloneRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "ISlateReflectorModule.h"
#include "SWebBrowser.h"
#include "IWebBrowserWindow.h"
#include "IWebBrowserPopupFeatures.h"

// project files
#include "SlateWidget/SConfPanel.h"
#include "Data/FLaunchConf.h"
#include "Tools/EngineLaunchTools.h"
#include "Tools/SerializationTools.h"
#include "Tools/CommandLineParase.h"
#include <cctype>

#define LOCTEXT_NAMESPACE "UE4Launcher"

IMPLEMENT_APPLICATION(UE4Launcher, "UE4Launcher");
namespace CommandHandler{
	void HandleEParamLogic(const FString& Param);
	void HandleCParamLogic(const FString& Param);
	void HandleGParamLogic(const FString& Param);

	static bool HasWindow=false;
};

TSharedPtr<SWindow> CreateConfWindow(const FLaunchConf& Conf);

int RealExecutionMain(const TCHAR* pCmdLine)
{
	// add .uejson file mapping for administrator user
	EngineLaunchTools::UE4LauncherRegisterWriter();

	FCommandLine::Set(pCmdLine);
	// start up the main loop
	GEngineLoop.PreInit(pCmdLine);
	// Make sure all UObject classes are registered and default properties have been initialized
	ProcessNewlyLoadedUObjects();
	// Tell the module manager it may now process newly-loaded UObjects when new C++ modules are loaded
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();
	// crank up a normal Slate application using the platform's standalone renderer
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());
	// set the application name
	FGlobalTabmanager::Get()->SetApplicationTitle(LOCTEXT("AppTitle", "UE4 Launcher"));
	FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").RegisterTabSpawner(FWorkspaceItem::NewGroup(LOCTEXT("DeveloperMenu", "Developer")));


	TMap<FString, void(*)(const FString&)> ParamsHandlerMaps;
	ParamsHandlerMaps.Add(TEXT("e"), &CommandHandler::HandleEParamLogic);
	ParamsHandlerMaps.Add(TEXT("c"), &CommandHandler::HandleCParamLogic);
	ParamsHandlerMaps.Add(TEXT("g"), &CommandHandler::HandleGParamLogic);

	TMap<FString, FString> CommandArgsMaps = CommandLineParase::GetCommandLineParamsMap(FCommandLine::Get());
	TArray<FString> AllParamsKeys;
	CommandArgsMaps.GetKeys(AllParamsKeys);

	for (const auto& ParamItem : AllParamsKeys)
	{
		if (ParamsHandlerMaps.Contains(ParamItem))
		{
			void(*Handler)(const FString&) = *ParamsHandlerMaps.Find(ParamItem);
			if (Handler)
			{
				Handler(*CommandArgsMaps.Find(ParamItem));
			}
		}
	}

	if (!AllParamsKeys.Num())
		CreateConfWindow(FLaunchConf{});

	if (CommandHandler::HasWindow)
	{
		// main loop
		while (!GIsRequestingExit)
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FStats::AdvanceFrame(false);
			FTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());
			FSlateApplication::Get().PumpMessages();
			FSlateApplication::Get().Tick();
		}
	}

	FModuleManager::Get().UnloadModulesAtShutdown();
	FSlateApplication::Shutdown();

	return 0;
}

TSharedPtr<SWindow> CreateConfWindow(const FLaunchConf& Conf)
{
	TSharedPtr < SConfPanel > LauncherPanel;
	TSharedPtr<SWindow> ConfWindow = SNew(SWindow)
		.Title(LOCTEXT("MainWindow_Title", "UE4 Launcher"))
		.ScreenPosition(FVector2D(520, 550))
		.ClientSize(FVector2D(520, 550))
		.SupportsMaximize(false)
		.AutoCenter(EAutoCenter::PrimaryWorkArea)
		.MaxHeight(800)
		.MaxWidth(650)
		.MinHeight(580)
		.MinWidth(520)
		.IsTopmostWindow(false)
		[
			SAssignNew(LauncherPanel, SConfPanel)
		];
	// show the window
	FSlateApplication::Get().AddWindow(ConfWindow.ToSharedRef());
	// use config
	LauncherPanel->UpdateAll(Conf);

	CommandHandler::HasWindow = true;
	return ConfWindow;
}
namespace CommandHandler{
	void HandleEParamLogic(const FString& Param)
	{
		FLaunchConf Conf;
		FString jsonFile = Param;
		FString jsonValue(TEXT(""));
		FString JsonfullPath = FPaths::ConvertRelativePathToFull(jsonFile);
		if (FFileHelper::LoadFileToString(jsonValue, *JsonfullPath))
		{
			Conf = SerializationTools::DeSerializationConf(jsonValue);
		}
		CreateConfWindow(Conf);
	}
	void HandleCParamLogic(const FString& Param)
	{
		FLaunchConf Conf;
		FString jsonFile = Param;
		FString jsonValue(TEXT(""));
		FString JsonfullPath = FPaths::ConvertRelativePathToFull(jsonFile);
		if (FFileHelper::LoadFileToString(jsonValue, *JsonfullPath))
		{
			Conf = SerializationTools::DeSerializationConf(jsonValue);
		}
		EngineLaunchTools::EngineLauncher(Conf);
	}
	void HandleGParamLogic(const FString& Param)
	{
		FLaunchConf Conf;
		FString EnginePath=EngineLaunchTools::GetUEProjectEnginePath(Param);
		Conf.Engine = EnginePath;
		Conf.Tool = TEXT("UE4Editor");
		Conf.Project = Param;
		while (std::isspace(Conf.Project[Conf.Project.Len() - 1]))
			Conf.Project.RemoveAt(Conf.Project.Len() - 1);

		FString ProjectDir = EngineLaunchTools::GetProjectDir(Param);
		for (uint32 Index = 0; Index < 100; ++Index)
		{
			FString SaveFile = FPaths::Combine(ProjectDir, TEXT("LaunchConf_")+FString::Printf(TEXT("%d"), Index) + TEXT(".uejson"));
			if (!FPaths::FileExists(SaveFile))
			{
				
				FFileHelper::SaveStringToFile(SerializationTools::SerializationConf(Conf), *SaveFile);
				break;
			}
		}
	}
};
#undef LOCTEXT_NAMESPACE