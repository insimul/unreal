// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulModuleActivator — which mechanic modules THIS world turns on, and the
// consequences of the ones it does not (US-3 of tasklist 146,
// RUNTIME_CORE_ADOPTION.md §14).
//
// THE ACTIVE SET IS DATA. This subsystem reads the genre out of the exported World IR
// (`Content/Data/WorldIR.json`, `meta.genreConfig.id`), looks it up in the table core
// emitted (`Content/Data/insimul/modules/genre-activation.json`) and consults exactly
// the rule packs that table names, in core's consult order. Adding a module to a genre
// bundle in core re-emits that table; re-vendoring it is the WHOLE of the engine-side
// change. Nothing in this file or in the portable resolver behind it names a mechanic,
// and `tools/verify-mechanics/check-activation.mjs` fails if one ever appears.
//
// AND THE INACTIVE ONES COST SOMETHING. Core's module contract §7.3: an unselected
// module gets no consulted rule pack and no registered system. Both halves happen
// here — the packs it owns are never consulted (its vocabulary does not exist in the
// KB at all, which ctest `activation_witness` proves against a real libinsimul), and
// UInsimulMechanicHostBinder asks this subsystem for the active host interfaces and
// UNREGISTERS every host outside them. The third consequence is what the PLAYER sees:
// the resolved set is handed to UInsimulUIPanelSurface, which withholds the default-UI
// panels an inactive module owns rather than showing a panel over predicates that have
// no solutions (tasklist 190 US-1).
//
// WHERE THE PACK TEXT COMES FROM. Vendored, as data the game ships, because no C ABI
// row returns one — see Portable/InsimulModulePacks.h and §14.1. A `prolog.packs` row
// would delete `Content/Data/insimul/packs/` and one implementation here.
//
// THE GENRE MAY BE MISSING, AND THAT IS REPORTED. A resumed save carries no genre at
// all (§14.2), so a game that boots from one must state its genre — set
// `DeclaredGenre` before Initialize, or call ActivateForGenre() after. A build that
// declares nothing activates EVERY pack, which is right for an editor session and
// wrong for a shipped game; the boot log says which of the three happened.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InsimulModuleActivation.h"
#include "InsimulModulePacks.h"
#include "InsimulModuleActivator.generated.h"

class UInsimulPrologSubsystem;

/** Where the genre came from — mirrors insimul::EInsimulGenreSource for Blueprints. */
UENUM(BlueprintType)
enum class EInsimulGenreOrigin : uint8
{
    /** Nothing declared one; every pack is active. A shipped game here is a bug in
     *  its export. */
    Undeclared UMETA(DisplayName = "Undeclared"),
    /** Read from the exported World IR. */
    WorldIr UMETA(DisplayName = "World IR"),
    /** Stated by this game — the documented workaround while a save carries none. */
    Declared UMETA(DisplayName = "Declared by the game"),
};

/**
 * Resolves and applies this world's active module set.
 */
UCLASS()
class INSIMULEXPORT_API UInsimulModuleActivator : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * A genre this game states itself, used when the World IR declares none. Set it
     * from a settings asset or a commandlet argument before the subsystem initializes;
     * ActivateForGenre() is the runtime equivalent.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Mechanics")
    FString DeclaredGenre;

    /**
     * Resolve from the exported World IR and consult the active packs. Called once at
     * Initialize; call it again after loading a different world.
     * @return true when every ACTIVE pack was found and consulted.
     */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Mechanics")
    bool ActivateFromWorldIr();

    /**
     * Resolve a genre this game states, rather than one the IR declares — the second
     * boot path, where a SaveFile carries no genre (§14.2).
     */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Mechanics")
    bool ActivateForGenre(const FString& GenreId);

    /** The resolved genre, or empty when nothing declared one. */
    UFUNCTION(BlueprintPure, Category = "Insimul|Mechanics")
    FString GetActiveGenre() const;

    /** Where that genre came from. "Which modules are on" is only as trustworthy as
     *  the answer to "who said so". */
    UFUNCTION(BlueprintPure, Category = "Insimul|Mechanics")
    EInsimulGenreOrigin GetGenreOrigin() const;

    /** The ids of the modules this world activates. */
    UFUNCTION(BlueprintPure, Category = "Insimul|Mechanics")
    TArray<FString> GetActiveModules() const;

    /** The rule packs consulted into the KB, in core's consult order. */
    UFUNCTION(BlueprintPure, Category = "Insimul|Mechanics")
    TArray<FString> GetConsultedPacks() const;

    /** The packs this build carries and this world did NOT activate — the refusal,
     *  named rather than implied. */
    UFUNCTION(BlueprintPure, Category = "Insimul|Mechanics")
    TArray<FString> GetSkippedPacks() const;

    /** Whether a module is active, by core's id. */
    UFUNCTION(BlueprintPure, Category = "Insimul|Mechanics")
    bool IsModuleActive(const FString& ModuleId) const;

    /** One line for a boot log or a bug report. */
    UFUNCTION(BlueprintPure, Category = "Insimul|Mechanics")
    FString DescribeActivation() const;

    /** True once a resolution has happened at all — a host binder must not restrict
     *  against an unresolved set. */
    bool IsResolved() const { return bResolved; }

    /** The host interfaces the active modules name. UInsimulMechanicHostBinder
     *  restricts to exactly this list. */
    const std::vector<std::string>& ActiveHostInterfaces() const { return ActiveSet.HostInterfaces; }

    /** The resolved set, for a system that needs more than the Blueprint surface. */
    const insimul::FInsimulActiveModuleSet& ActiveModuleSet() const { return ActiveSet; }

    /** `<project>/Content/Data/insimul` — where the vendored table, packs and
     *  scenarios live in an exported game. */
    static FString DataRoot();

private:
    /** Hand the resolved set to the plugin's UI panel surface, so the default-UI
     *  panels a module owns are withheld from a world that did not select it (core
     *  §7.3's "no registered system", on the UI side). The asymmetry an undeclared
     *  genre gets is the resolver's, not this file's. */
    void ApplyToPanelSurface();

    /** Read PACKS.json and genre-activation.json. False (and logged) when either is
     *  missing — a game whose activation data is absent must not boot pretending it
     *  activated something. */
    bool LoadTables();

    /** Consult the active packs into the plugin's KB, in core's consult order. */
    bool ConsultActive();

    UInsimulPrologSubsystem* ResolveProlog() const;

    bool bTablesLoaded = false;
    bool bResolved = false;
    insimul::FInsimulActivationTable Table;
    insimul::FInsimulPredicatePackManifest Manifest;
    insimul::FInsimulActiveModuleSet ActiveSet;
    insimul::FInsimulPackConsultReport ConsultReport;
};
