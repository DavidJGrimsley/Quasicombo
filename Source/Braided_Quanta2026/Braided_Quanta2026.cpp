// Copyright Epic Games, Inc. All Rights Reserved.

#include "Braided_Quanta2026.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
void RegisterQuasicomboQTEMenu();
#endif
class FQuasicomboGameModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
        FDefaultGameModuleImpl::StartupModule();
#if WITH_EDITOR
        RegisterQuasicomboQTEMenu();
#endif
    }
};
IMPLEMENT_PRIMARY_GAME_MODULE( FQuasicomboGameModule, Braided_Quanta2026, "Braided_Quanta2026" );

DEFINE_LOG_CATEGORY(LogBraided_Quanta2026)
