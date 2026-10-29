// Copyright Big Shot Pictures. Based on Loops 2D Pan/Zoom, Copyright 2026 Loops Creative Studio (MIT, see LICENSE).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BSPPanZoomLibrary.generated.h"

/**
 * Scripting access to 2D Pan/Zoom. Every call acts on the viewport under the mouse,
 * else the active viewport.
 *
 * Python:
 *   unreal.BSPPanZoomLibrary.set_zoom(2.0)
 *   unreal.BSPPanZoomLibrary.set_pan(unreal.Vector2D(0.5, 0.0))
 *   unreal.BSPPanZoomLibrary.reset()
 */
UCLASS()
class BSP_PANZOOM_API UBSPPanZoomLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Turn 2D Pan/Zoom on or off. Off restores the view but keeps the values for next time. */
	UFUNCTION(BlueprintCallable, Category = "BSP|2D Pan Zoom")
	static void SetEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "BSP|2D Pan Zoom")
	static bool IsEnabled();

	/** 1 = the camera's own framing, 2 = twice as close. Turns 2D Pan/Zoom on. */
	UFUNCTION(BlueprintCallable, Category = "BSP|2D Pan Zoom")
	static void SetZoom(float Zoom);

	UFUNCTION(BlueprintPure, Category = "BSP|2D Pan Zoom")
	static float GetZoom();

	/**
	 * Perspective: fraction of the unzoomed frame, X = 1 brings its right edge to the centre,
	 * Y = 1 its top edge. Orthographic: world units. Turns 2D Pan/Zoom on.
	 */
	UFUNCTION(BlueprintCallable, Category = "BSP|2D Pan Zoom")
	static void SetPan(FVector2D Pan);

	UFUNCTION(BlueprintPure, Category = "BSP|2D Pan Zoom")
	static FVector2D GetPan();

	/** Zoom 100%, no pan. Leaves 2D Pan/Zoom on or off as it was. */
	UFUNCTION(BlueprintCallable, Category = "BSP|2D Pan Zoom")
	static void Reset();
};
