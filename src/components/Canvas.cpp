#include <algorithm>
#include <Kotonoha/components/Canvas.hpp>

namespace Kotonoha {

	namespace {
		static bool CanvasItemLess(const CanvasItem& a,
			const CanvasItem& b) {
			return a.zIndex < b.zIndex;
		}

		static bool IsValidSize(const SDL_FRect& place) {
			return place.w > 0.0f && place.h > 0.0f;
		}
	} // namespace

	void Canvas::RegisterCanva(
		Kotonoha_Scene_Status(*drawingPoint)(KOTONOHA_SCENE_CALL),
		Sint16 zIndex,
		SDL_FRect place,
		void* userData) {
		if (drawingPoint == nullptr || userData == nullptr) {
			return;
		}

		CanvasItem item{};
		item.drawingPoint = drawingPoint;
		item.zIndex = zIndex;
		item.place = place;
		item.userData = userData;
		item.target = nullptr;
		item.swapTexture = false;
		item.debugLastStatus = -1;

		drawingList.push_back(item);
		std::sort(drawingList.begin(), drawingList.end(), CanvasItemLess);
	}

	void Canvas::UnregisterCanva(
		Kotonoha_Scene_Status(*drawingPoint)(KOTONOHA_SCENE_CALL)) {
		for (auto& item : drawingList) {
			if (item.drawingPoint == drawingPoint && item.target != nullptr) {
				SDL_DestroyTexture(item.target);
				item.target = nullptr;
			}
		}

		drawingList.erase(
			std::remove_if(drawingList.begin(), drawingList.end(),
				[drawingPoint](const CanvasItem& item) {
					return item.drawingPoint == drawingPoint;
				}),
			drawingList.end());
	}

	void Canvas::UpdateCanva(
		Kotonoha_Scene_Status(*drawingPoint)(KOTONOHA_SCENE_CALL),
		Sint16 zIndex,
		SDL_FRect place) {
		if (drawingPoint == nullptr) {
			dirtyPlace = place;
		}

		bool needsResort = false;

		for (auto& item : drawingList) {
			if (drawingPoint != nullptr && item.drawingPoint != drawingPoint) {
				continue;
			}

			if (item.place.w != place.w || item.place.h != place.h) {
				item.swapTexture = true;
			}

			if (zIndex != -1 && item.zIndex != zIndex) {
				item.zIndex = zIndex;
				needsResort = true;
			}

			item.place = place;
		}

		if (needsResort) {
			std::sort(drawingList.begin(), drawingList.end(), CanvasItemLess);
		}
	}

	SDL_AppResult Canvas::RenderCanvas(SDL_Window* window,
		SDL_Renderer* render,
		struct Kotonoha_eventStack* eventQueu) {
		if (render == nullptr) {
			return SDL_APP_FAILURE;
		}

		if (dirtyTexture != nullptr) {
			SDL_RenderTexture(render, dirtyTexture, nullptr, &dirtyPlace);
		}

		for (auto& item : drawingList) {
			if (item.userData == nullptr || item.drawingPoint == nullptr) {
				continue;
			}

			if (!IsValidSize(item.place)) {
				continue;
			}

			if (item.swapTexture && item.target != nullptr) {
				SDL_DestroyTexture(item.target);
				item.target = nullptr;
				item.swapTexture = false;
			}

			if (item.target == nullptr) {
				item.target = SDL_CreateTexture(
					render,
					SDL_PIXELFORMAT_RGBA8888,
					SDL_TEXTUREACCESS_TARGET,
					static_cast<int>(item.place.w),
					static_cast<int>(item.place.h));

				if (item.target == nullptr) {
					continue;
				}
				SDL_SetTextureBlendMode(item.target, SDL_BLENDMODE_BLEND);
			}

			SDL_SetRenderTarget(render, item.target);
			const Kotonoha_Scene_Status result =
				item.drawingPoint(window, render, eventQueu, item.userData, item.target);

			if (item.debugLastStatus != static_cast<int>(result)) {
				SDL_Log(
					"[KTN-DIAG2][CANVAS] ticks=%llu z=%d status=%d "
					"target=%p dirty=%p",
					(unsigned long long)SDL_GetTicks(),
					static_cast<int>(item.zIndex),
					static_cast<int>(result),
					(void*)item.target,
					(void*)dirtyTexture);

				item.debugLastStatus = static_cast<int>(result);
			}

			// KTN-0001-R4:
			// WAITING means the component does not yet have a frame that is
			// ready to be presented. Do not composite its fresh target over
			// dirtyTexture; leave the retained previous frame visible.
			if (result == KOTONOHA_SCENE_WAITING) {
				SDL_SetRenderTarget(render, nullptr);
				continue;
			}

			if (result == KOTONOHA_SCENE_NULL || result == KOTONOHA_SCENE_COMPLETE)
				continue;

			if (result == KOTONOHA_SCENE_DRAW_LAST) {
				if (dirtyTexture != nullptr) {
					SDL_DestroyTexture(dirtyTexture);
				}

				dirtyTexture = item.target;
				dirtyPlace = item.place;
				item.target = nullptr;

				// KTN-0001-R2:
				// Main.cpp clears the window before Canvas::RenderCanvas().
				// DRAW_LAST transfers the final rendered target into dirtyTexture.
				// The old code nulled item.target and therefore did not present
				// that retained frame until the NEXT SDL_AppIterate(), exposing
				// the black clear for one frame.
				SDL_SetRenderTarget(render, nullptr);
				if (dirtyTexture != nullptr) {
					SDL_RenderTexture(render, dirtyTexture, nullptr, &dirtyPlace);
				}

				SDL_Log("[KTN-0001-R2] DRAW_LAST presented immediately");
				continue;
			}

			if (item.target != nullptr) {
				SDL_SetRenderTarget(render, nullptr);
				SDL_RenderTexture(render, item.target, nullptr, &item.place);
			}
		}
		return SDL_APP_CONTINUE;
	}
	bool Canvas::TransferLastFrameTo(Canvas* target) {
		if (target == nullptr || target == this || dirtyTexture == nullptr) {
			return false;
		}

		if (target->dirtyTexture != nullptr) {
			SDL_DestroyTexture(target->dirtyTexture);
			target->dirtyTexture = nullptr;
		}

		target->dirtyTexture = dirtyTexture;
		target->dirtyPlace = dirtyPlace;

		// Ownership moved to the target Canvas. The source Reset() must not
		// destroy the retained frame after the scene switch.
		dirtyTexture = nullptr;
		dirtyPlace = { 0, 0, 0, 0 };

		return true;
	}

	int Canvas::CanvasCount() {
		return static_cast<int>(drawingList.size());
	}

	void Canvas::Reset() {
		for (auto& item : drawingList) {
			if (item.target != nullptr) {
				SDL_DestroyTexture(item.target);
				item.target = nullptr;
			}
		}

		if (dirtyTexture != nullptr) {
			SDL_DestroyTexture(dirtyTexture);
			dirtyTexture = nullptr;
		}
	}

	Canvas::~Canvas() {
		Canvas::Reset();
	}

} // namespace Kotonoha