// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulUIPanelSurface — the UE seam over the module-gated panel resolver
// (Portable/InsimulUIPanelCatalog.h, US-1 of tasklist 190). The ONE place UMG asks
// "may I show panel K, and which widget serves it": it joins the shipped panel
// catalog (Content/Data/insimul/ui/panels.json — which module owns which panel) to
// the module set this world resolved (core's module contract §7) and to
// UInsimulUIRegistry (which widget class serves a key, with the creator override
// layer).
//
// WHY EVERY PANEL GOES THROUGH HERE. Two things a creator must be able to do without
// touching engine code: swap a panel's widget (the registry's override layer) and
// ship a game whose genre bundle does not select a mechanic (the gate). Widget code
// that reached for its own class, or that decided for itself whether a mechanic is
// on, would defeat both — so the default-UI widgets ask this subsystem and nothing
// else. ResolvePanelClass returning nullptr is a REPORTED state (a log line naming
// the key and the reason), never a silent no-op.
//
// WHO APPLIES THE MODULE SET. The exported game's UInsimulModuleActivator, once it
// has resolved a genre (templates/source/mechanics/InsimulModuleActivator.cpp). A
// game that never applies one — an editor session, a commandlet, a plugin dropped
// into a project with no Insimul data dir — stays UNGATED and shows every panel,
// which is the same answer the pack consult gives for an undeclared genre, and
// DescribeSurface() says which of the two happened.
//
// The resolution SEMANTICS are proven UE-free by FInsimulUIPanelResolver (ctest
// `ui_registry`); this class is the thin, syntax-gated Blueprint / UObject boundary.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InsimulUIPanelSurface.generated.h"

class UInsimulUIRegistry;

// The engine-agnostic resolver this seam wraps (pimpl — never in the reflected
// layout). Host-tested by ctest `ui_registry`.
namespace insimul { class FInsimulUIPanelResolver; struct FInsimulActiveModuleSet; }

/** Why a panel resolved the way it did (mirrors insimul::EInsimulPanelOutcome). */
UENUM(BlueprintType)
enum class EInsimulPanelAvailability : uint8
{
	/** Available, served by the shipped default widget. */
	Shipped UMETA(DisplayName = "Shipped default"),
	/** Available, served by a creator override. */
	Overridden UMETA(DisplayName = "Creator override"),
	/** The panel exists and this world does not activate the module that owns it. */
	Gated UMETA(DisplayName = "Withheld — module not active"),
	/** No such panel key. */
	Unknown UMETA(DisplayName = "Unknown key"),
};

/**
 * The default-UI panel surface for this game instance.
 */
UCLASS()
class INSIMULRUNTIME_API UInsimulUIPanelSurface : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Apply this world's active module ids — the gate goes ON. Called by the
	 * exported game's UInsimulModuleActivator after it resolves a genre; a game with
	 * its own activation may call it directly.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void ApplyActiveModules(const TArray<FString>& ModuleIds, const FString& GenreId);

	/**
	 * Apply a RESOLVED module set — the C++ path, and the one the exported game's
	 * activator takes. It is preferred over ApplyActiveModules() because the
	 * undeclared-genre asymmetry (an undeclared genre activates every pack, so it
	 * withholds no panel either) is then decided in ONE place, the portable resolver,
	 * rather than re-derived by every caller.
	 */
	void ApplyModuleSet(const insimul::FInsimulActiveModuleSet& Set);

	/** Return to "nothing resolved": every panel available (editor / commandlet). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void ClearActiveModules();

	/** True once a module set has been applied and panels are being gated. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	bool IsGated() const;

	/** True when this world may show `PanelKey` at all. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	bool IsPanelAvailable(FName PanelKey) const;

	/** Why `PanelKey` resolved the way it did. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	EInsimulPanelAvailability PanelAvailability(FName PanelKey) const;

	/**
	 * The widget class serving `PanelKey`, or nullptr when the panel is withheld or
	 * unknown — logged with the reason either way, so a creator never debugs a
	 * silent no-op. Prefers the registry asset's binding (that is the creator's
	 * override surface) and falls back to the catalog's widget path.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	TSubclassOf<UUserWidget> ResolvePanelClass(FName PanelKey);

	/** Create the widget serving `PanelKey` (owned by this game instance), or
	 *  nullptr when the panel is withheld or unknown. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	UUserWidget* CreatePanelWidget(FName PanelKey);

	/** Panel keys this world may show, in catalog order. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	TArray<FName> AvailablePanels() const;

	/** Panel keys withheld because their module is not active, in catalog order. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	TArray<FName> WithheldPanels() const;

	/** One line for a boot log or a bug report. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	FString DescribeSurface() const;

	/** The creator's registry asset (from UInsimulSettings), loaded on demand. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	UInsimulUIRegistry* GetRegistry() const;

	/** `<project>/Content/Data/insimul/ui/panels.json` — the shipped catalog. */
	static FString CatalogPath();

private:
	/** Load the shipped catalog; falls back to the built-in map (and logs) when the
	 *  data dir is absent or the document is not a catalog. */
	void LoadCatalog();

	insimul::FInsimulUIPanelResolver& EnsureResolver() const;

	/** The host-tested portable resolver (mutable: the accessors are BlueprintPure
	 *  but the resolver is created lazily). */
	mutable TUniquePtr<insimul::FInsimulUIPanelResolver> Resolver;

	UPROPERTY(Transient)
	mutable TObjectPtr<UInsimulUIRegistry> Registry;

	mutable bool bRegistryResolved = false;
};
