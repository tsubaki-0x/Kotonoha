#include <Kotonoha/components/Video.hpp>

namespace Kotonoha {

	namespace {
		static const char* KtnDiagStatusName(Kotonoha_Scene_Status status) {
			switch (status) {
			case KOTONOHA_SCENE_NULL:
				return "NULL";
			case KOTONOHA_SCENE_WAITING:
				return "WAITING";
			case KOTONOHA_SCENE_DRAW:
				return "DRAW";
			case KOTONOHA_SCENE_DRAW_LAST:
				return "DRAW_LAST";
			case KOTONOHA_SCENE_DRAW_OVERLAYED:
				return "DRAW_OVERLAYED";
			case KOTONOHA_SCENE_COMPLETE:
				return "COMPLETE";
			case KOTONOHA_SCENE_FATAL_ERROR:
				return "FATAL_ERROR";
			default:
				return "UNKNOWN";
			}
		}
	}

	Video::Video(Kotonoha_time* timeManager) : timeManager(timeManager) {
		lock = SDL_CreateMutex();
		if (lock == nullptr) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				"Failed to create video mutex: %s",
				SDL_GetError());
		}
	}

	bool Video::Register(const char* path, Uint64 startTime, Uint64 endTime,
		bool useOrsFrameTimeline,
		Uint64 orsStartFrame,
		Uint64 orsEndFrame) {
		if (timeManager == nullptr) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				"Video time manager is null.");
			return false;
		}

		if (path == nullptr || *path == '\0') {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				"Video path is invalid.");
			return false;
		}

		Kotonoha_videoData* object =
			Kotonoha_VideoRenderInit(
				path,
				timeManager,
				startTime,
				endTime,
				useOrsFrameTimeline,
				orsStartFrame,
				orsEndFrame);
		if (object == nullptr) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				"Failed to initialize video: %s", path);
			return false;
		}

		if (lock == nullptr) {
			Kotonoha_VideoRenderShutdown(&object);
			return false;
		}

		object->debugLastStatus = -1;

		if (object->useOrsFrameTimeline) {
			SDL_Log(
				"[KTN-TL][REGISTER] ticks=%llu video=%p startMs=%llu endMs=%llu "
				"startFrame=%llu endFrame=%llu path=%s",
				(unsigned long long)SDL_GetTicks(),
				(void*)object,
				(unsigned long long)object->startTime,
				(unsigned long long)object->endTime,
				(unsigned long long)object->orsStartFrame,
				(unsigned long long)object->orsEndFrame,
				path);
		}

		SDL_Log(
			"[KTN-DIAG2][REGISTER] ticks=%llu video=%p start=%llu end=%llu "
			"videoTime=%llu texture=%p frame=%p path=%s",
			(unsigned long long)SDL_GetTicks(),
			(void*)object,
			(unsigned long long)startTime,
			(unsigned long long)endTime,
			(unsigned long long)object->videoTime,
			(void*)object->texture,
			(void*)object->pFrame,
			path);

		SDL_LockMutex(lock);
		videos.push_back(object);
		SDL_UnlockMutex(lock);
		return true;
	}

	Kotonoha_Scene_Status Video::Render(KOTONOHA_SCENE_CALL) {
		Video* here = static_cast<Video*>(userData);
		if (here == nullptr) {
			return KOTONOHA_SCENE_COMPLETE;
		}

		if (here->lock == nullptr) {
			return KOTONOHA_SCENE_COMPLETE;
		}

		Kotonoha_Scene_Status returnStatus = KOTONOHA_SCENE_NULL;
		std::vector<Kotonoha_videoData*> finishedVideos;

		SDL_LockMutex(here->lock);

		for (auto it = here->videos.begin(); it != here->videos.end();) {
			Kotonoha_videoData* currentVideo = *it;
			if (currentVideo == nullptr) {
				it = here->videos.erase(it);
				continue;
			}

			const Kotonoha_Scene_Status status =
				Kotonoha_VideoRenderProcess(currentVideo, render);

			if (currentVideo->debugLastStatus != static_cast<int>(status)) {
				SDL_Log(
					"[KTN-DIAG2][VIDEO_STATUS] ticks=%llu video=%p status=%s "
					"timeline=%llu start=%llu end=%llu videoTime=%llu "
					"texture=%p frame=%p",
					(unsigned long long)SDL_GetTicks(),
					(void*)currentVideo,
					KtnDiagStatusName(status),
					(unsigned long long)Kotonoha_timeGet(here->timeManager),
					(unsigned long long)currentVideo->startTime,
					(unsigned long long)currentVideo->endTime,
					(unsigned long long)currentVideo->videoTime,
					(void*)currentVideo->texture,
					(void*)currentVideo->pFrame);

				currentVideo->debugLastStatus = static_cast<int>(status);
			}

			switch (status) {
			case KOTONOHA_SCENE_DRAW:
				SDL_RenderTexture(render, currentVideo->texture, nullptr, nullptr);
				returnStatus = KOTONOHA_SCENE_DRAW;
				++it;
				break;

			case KOTONOHA_SCENE_COMPLETE:
				SDL_Log(
					"[KTN-DIAG2][VIDEO_COMPLETE] ticks=%llu video=%p "
					"timeline=%llu start=%llu end=%llu texture=%p",
					(unsigned long long)SDL_GetTicks(),
					(void*)currentVideo,
					(unsigned long long)Kotonoha_timeGet(here->timeManager),
					(unsigned long long)currentVideo->startTime,
					(unsigned long long)currentVideo->endTime,
					(void*)currentVideo->texture);

				if (returnStatus != KOTONOHA_SCENE_DRAW) {
					SDL_RenderTexture(render, currentVideo->texture, nullptr, nullptr);
					returnStatus = KOTONOHA_SCENE_DRAW_LAST;
				}
				finishedVideos.push_back(currentVideo);
				it = here->videos.erase(it);
				break;
			case KOTONOHA_SCENE_WAITING:
				// KTN-0001-R6-WAIT:
				// WAITING must advance to the next registered video. It must
				// also not overwrite a DRAW/DRAW_LAST already produced by an
				// overlapping video in this same composite pass.
				if (returnStatus != KOTONOHA_SCENE_DRAW &&
					returnStatus != KOTONOHA_SCENE_DRAW_LAST) {
					returnStatus = KOTONOHA_SCENE_WAITING;
				}
				++it;
				break;
			default:
				++it;
				break;
			}
		}

		const bool isEmpty = here->videos.empty();
		SDL_UnlockMutex(here->lock);

		for (Kotonoha_videoData* video : finishedVideos) {
			Kotonoha_VideoRenderShutdown(&video);
		}

		if (isEmpty && returnStatus != KOTONOHA_SCENE_DRAW_LAST) {
			return KOTONOHA_SCENE_COMPLETE;
		}

		return returnStatus;
	}

	void Video::Reset() {
		if (lock == nullptr) {
			for (auto& video : videos) {
				Kotonoha_VideoRenderShutdown(&video);
			}
			videos.clear();
			return;
		}

		std::vector<Kotonoha_videoData*> oldVideos;

		SDL_LockMutex(lock);
		oldVideos.swap(videos);
		SDL_UnlockMutex(lock);

		for (Kotonoha_videoData* video : oldVideos) {
			Kotonoha_VideoRenderShutdown(&video);
		}
	}

	Video::~Video() {
		Reset();

		if (lock != nullptr) {
			SDL_DestroyMutex(lock);
			lock = nullptr;
		}
	}

} // namespace Kotonoha