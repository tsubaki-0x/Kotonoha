#include <cctype>
#include <Kotonoha/components/Events.hpp>
#include <Kotonoha/Gameplay.hpp>
#include <SDL3/SDL.h>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace Kotonoha {

	namespace {
		static std::string BuildString(const char* str,
			const std::string& prefix = "",
			const std::string& suffix = "") {
			if (str == nullptr) {
				return "";
			}

			std::string result = prefix + std::string(str) + suffix;

			size_t lastSlash = result.find_last_of('/');
			if (lastSlash == std::string::npos) {
				lastSlash = 0;
			}
			else {
				++lastSlash;
			}

			if (result.size() - lastSlash >= 4 &&
				result.compare(lastSlash, 4, "UNC_") == 0) {
				result = result.substr(0, lastSlash) + result.substr(lastSlash + 4);
			}

			return result;
		}

		static std::string ToUpper(const std::string& str) {
			std::string upperStr;
			upperStr.reserve(str.size());

			for (unsigned char c : str) {
				upperStr += static_cast<char>(std::toupper(c));
			}

			return upperStr;
		}

		// School Days ORS timestamps are authored as MM:SS:FF, with FF on a
		// 24-fps timeline. The historical parser packs that as
		// MM*60000 + SS*1000 + FF*10, which is NOT true milliseconds.
		//
		// The parser can also add +1 to event->start when it equals the
		// previous event end. allowParserNudge reverses only that known +1.
		static bool DecodeOrs24FpsTimestamp(
			Uint64 packed,
			bool allowParserNudge,
			Uint64* normalizedPacked,
			Uint64* frame1,
			Uint64* msCeil,
			bool* parserNudge) {
			if (normalizedPacked == nullptr ||
				frame1 == nullptr ||
				msCeil == nullptr ||
				parserNudge == nullptr) {
				return false;
			}

			Uint64 candidate = packed;
			bool nudge = false;

			auto decodeCandidate = [](Uint64 value,
				Uint64* outFrame1,
				Uint64* outMsCeil) -> bool {
				const Uint64 minutes = value / 60000;
				const Uint64 rem = value % 60000;
				const Uint64 seconds = rem / 1000;
				const Uint64 tail = rem % 1000;

				if (seconds >= 60 || (tail % 10) != 0) {
					return false;
				}

				const Uint64 frameField = tail / 10;
				if (frameField >= 24) {
					return false;
				}

				const Uint64 zeroBased =
					(minutes * 60 + seconds) * 24 + frameField;

				*outFrame1 = zeroBased + 1;

				// First integer millisecond that has reached this 24-fps
				// frame boundary: ceil(zeroBased * 1000 / 24).
				*outMsCeil = (zeroBased * 1000 + 23) / 24;
				return true;
			};

			Uint64 decodedFrame = 0;
			Uint64 decodedMs = 0;

			if (!decodeCandidate(candidate, &decodedFrame, &decodedMs)) {
				if (!allowParserNudge || candidate == 0 ||
					!decodeCandidate(candidate - 1, &decodedFrame, &decodedMs)) {
					return false;
				}

				candidate--;
				nudge = true;
			}

			*normalizedPacked = candidate;
			*frame1 = decodedFrame;
			*msCeil = decodedMs;
			*parserNudge = nudge;
			return true;
		}

		static void DestroyEventManagerParams(void** parms) {
			if (parms == nullptr) {
				return;
			}

			delete static_cast<std::vector<std::tuple<std::string, int>>*>(parms[3]);
			delete static_cast<std::string*>(parms[4]);
			SDL_free(parms);
		}
	} // namespace

	int Event::EventManager(void* data) {
		void** parms = static_cast<void**>(data);
		if (parms == nullptr) {
			return -1;
		}

		auto* gameplay = static_cast<Gameplay*>(parms[0]);
		auto* gameCtx = static_cast<struct Kotonoha_Game*>(parms[1]);
		auto* classUp = static_cast<Event*>(parms[2]);
		auto* object =
			static_cast<std::vector<std::tuple<std::string, int>>*>(parms[3]);
		auto* lastCreateBg = static_cast<std::string*>(parms[4]);

		if (gameplay == nullptr || gameCtx == nullptr || classUp == nullptr ||
			object == nullptr || lastCreateBg == nullptr ||
			classUp->eventMutex == nullptr) {
			DestroyEventManagerParams(parms);
			return -1;
		}

		std::string prevLastCreateBg = *lastCreateBg;
		bool useExtension = (gameCtx->assetsPath != nullptr);
		const char* assetsPath = useExtension ? gameCtx->assetsPath : "";

		SDL_LockMutex(classUp->eventMutex);
		if (gameplay->tm == nullptr || !gameplay->tm->started) {
			SDL_UnlockMutex(classUp->eventMutex);
			return 0;
		}

		for (auto* event = classUp->eventsFromScript.data; event != nullptr;
			event = event->next) {
			const Uint64 actualTime = Kotonoha_timeGet(gameplay->tm);

			if (actualTime + 10000 < event->start || event->eventTouched) {
				continue;
			}

			event->eventTouched = true;

			if (*lastCreateBg != prevLastCreateBg) {
				object->clear();
				prevLastCreateBg = *lastCreateBg;
			}

			if (event->end < actualTime) {
				continue;
			}

			switch (event->command) {
			case PLAY_VOICE: {
				if (event->data.play_voice->path != nullptr &&
					SDL_strlen(event->data.play_voice->path) > 0) {
					gameplay->audio->AddMedia(
						BuildString(event->data.play_voice->path,
							assetsPath,
							useExtension ? ".OGG" : "")
						.c_str(),
						event->start,
						event->end + 1000,
						false,
						"Voice");

					if (!lastCreateBg->empty() &&
						event->data.play_voice->character_short != nullptr) {
						std::string character =
							ToUpper(event->data.play_voice->character_short);
						int searchImgId = 0;
						bool found = false;

						for (auto& it : *object) {
							if (std::get<0>(it) == character) {
								++std::get<1>(it);
								searchImgId = std::get<1>(it);
								found = true;
								break;
							}
						}

						if (!found) {
							object->emplace_back(character, 0);
						}

						const char suffix = static_cast<char>('A' + searchImgId);
						const std::string pathImg = *lastCreateBg + character + "." + suffix;
						const std::string path =
							BuildString(pathImg.c_str(), assetsPath,
								useExtension ? ".PNG" : "");

						SDL_IOStream* file = SDL_IOFromFile(path.c_str(), "rb");
						if (file != nullptr) {
							SDL_CloseIO(file);
							gameplay->image->Register(path.c_str(),
								event->start,
								event->end,
								1);
						}
					}
				}
				break;
			}

			case PLAY_SE:
				if (event->data.play_se->path != nullptr &&
					SDL_strlen(event->data.play_se->path) > 0) {
					gameplay->audio->AddMedia(
						BuildString(event->data.play_se->path,
							assetsPath,
							useExtension ? ".OGG" : "")
						.c_str(),
						event->start,
						event->end,
						true,
						"Se");
				}
				break;

			case PLAY_BGM:
				if (event->data.path_end->path != nullptr &&
					SDL_strlen(event->data.path_end->path) > 0) {
					std::string str = ToUpper(event->data.path_end->path);
					gameplay->audio->AddMedia(
						BuildString(useExtension ? str.c_str()
							: event->data.path_end->path,
							assetsPath,
							useExtension ? "_LOOP.OGG" : "")
						.c_str(),
						event->start,
						event->end,
						true,
						"BGM");
				}
				break;

			case END_BGM:
				if (event->data.path_end->path != nullptr &&
					SDL_strlen(event->data.path_end->path) > 0) {
					std::string str = ToUpper(event->data.path_end->path);
					gameplay->audio->AddMedia(
						BuildString(useExtension ? str.c_str()
							: event->data.path_end->path,
							assetsPath,
							useExtension ? ".OGG" : "")
						.c_str(),
						event->start,
						event->end,
						true,
						"BGM");
				}
				break;

			case END_ROLL:
				if (event->data.path_end->path != nullptr &&
					SDL_strlen(event->data.path_end->path) > 0) {
					gameplay->video->Register(
						BuildString(event->data.path_end->path,
							assetsPath,
							useExtension ? ".WMV" : "")
						.c_str(),
						event->start,
						event->end);
				}
				break;

			case PLAY_MOVIE:
				if (event->data.play_movie->path != nullptr &&
					SDL_strlen(event->data.play_movie->path) > 0) {
					const std::string moviePath =
						BuildString(event->data.play_movie->path,
							assetsPath,
							useExtension ? ".WMV" : "");

					Uint64 sourceStartPacked = 0;
					Uint64 sourceEndPacked = 0;
					Uint64 startFrame = 0;
					Uint64 endFrame = 0;
					Uint64 startMs = 0;
					Uint64 endMs = 0;
					bool startNudge = false;
					bool endNudge = false;

					const bool startMapped = DecodeOrs24FpsTimestamp(
						event->start,
						true,
						&sourceStartPacked,
						&startFrame,
						&startMs,
						&startNudge);

					const bool endMapped = DecodeOrs24FpsTimestamp(
						event->end,
						false,
						&sourceEndPacked,
						&endFrame,
						&endMs,
						&endNudge);

					if (startMapped && endMapped && endFrame >= startFrame) {
						SDL_Log(
							"[KTN-TL][MAP] path=%s packedStart=%llu packedEnd=%llu "
							"normalizedStart=%llu normalizedEnd=%llu "
							"startFrame=%llu endFrame=%llu startMs=%llu endMs=%llu "
							"startNudge=%d",
							moviePath.c_str(),
							(unsigned long long)event->start,
							(unsigned long long)event->end,
							(unsigned long long)sourceStartPacked,
							(unsigned long long)sourceEndPacked,
							(unsigned long long)startFrame,
							(unsigned long long)endFrame,
							(unsigned long long)startMs,
							(unsigned long long)endMs,
							startNudge ? 1 : 0);

						gameplay->video->Register(
							moviePath.c_str(),
							startMs,
							endMs,
							true,
							startFrame,
							endFrame);
					}
					else {
						// Prototype fallback: preserve the historical behavior
						// when the source timestamp does not satisfy the proven
						// MM:SS:FF@24 mapping. Do not extend compatibility
						// semantics to unproven paths.
						SDL_LogWarn(
							SDL_LOG_CATEGORY_APPLICATION,
							"[KTN-TL][MAP_FALLBACK] path=%s packedStart=%llu "
							"packedEnd=%llu legacyEnd=%llu",
							moviePath.c_str(),
							(unsigned long long)event->start,
							(unsigned long long)event->end,
							(unsigned long long)(event->end + 50));

						gameplay->video->Register(
							moviePath.c_str(),
							event->start,
							event->end + 50);
					}
				}
				break;

			case CREATE_BG:
				if (event->data.create_bg->path != nullptr &&
					SDL_strlen(event->data.create_bg->path) > 0) {
					*lastCreateBg = event->data.create_bg->path;
					gameplay->image->Register(
						BuildString(event->data.create_bg->path,
							assetsPath,
							useExtension ? ".PNG" : "")
						.c_str(),
						event->start,
						event->end,
						0);
				}
				break;

			default:
				break;
			}
		}

		SDL_UnlockMutex(classUp->eventMutex);
		return 0;
	}

	void Event::Reset(void* gameplay) {
		auto* gp = static_cast<Gameplay*>(gameplay);
		if (gp == nullptr || eventMutex == nullptr) {
			return;
		}

		SDL_LockMutex(eventMutex);

		gp->video->Reset();
		gp->image->Reset();
		gp->audio->RemoveMedia(nullptr);

		Uint64 actualTime = Kotonoha_timeGet(gp->tm);

		for (auto* event = this->eventsFromScript.data; event != nullptr;
			event = event->next) {
			if (event->end > actualTime)
				event->eventTouched = false;
		}

		SDL_UnlockMutex(eventMutex);
	}

	Event::Event(const char* orsPath, void* gameplay, struct Kotonoha_Game* gameCtx)
		: eventMutex(nullptr), lastTime(0) {
		eventsFromScript = Kotonoha_OrsParser(orsPath);

		if (eventsFromScript.size == 0) {
			throw std::runtime_error("Ors invalid");
		}

		auto* gp = static_cast<Gameplay*>(gameplay);
		if (gp == nullptr || gameCtx == nullptr) {
			Kotonoha_OrsClean(&eventsFromScript);
			throw std::runtime_error("Invalid gameplay context");
		}

		std::stringstream subSs;
		subSs << "[Script Info]\nTitle:" << orsPath
			<< "\nScriptType: v4.00+\nWrapStyle: 0\nScaledBorderAndShadow: yes\n"
			<< "YCbCr Matrix: None\n\n"
			<< (gameCtx->styleStr == nullptr ? "" : gameCtx->styleStr) << std::endl;

		gp->sb->track = ass_new_track(gp->sb->ass_library);

		ass_process_data(gp->sb->track,
			const_cast<char*>(subSs.str().c_str()),
			static_cast<int>(subSs.str().size()));

		for (auto* event = eventsFromScript.data; event != nullptr;
			event = event->next) {
			switch (event->command) {
			case PRINT_TEXT: {
				ass_alloc_event(gp->sb->track);
				ASS_Event* subtitleEvent = gp->sb->track->events + (gp->sb->track->n_events - 1);
				subtitleEvent->Start = event->start;
				subtitleEvent->Duration = event->end - event->start;
				subtitleEvent->Text =
					SDL_strdup(BuildString(event->data.print_text->text).c_str());

				for (int i = 0; i < gp->sb->track->n_styles; ++i) {
					ASS_Style* style = gp->sb->track->styles + i;

					if (SDL_strcmp(style->Name, "Default") == 0) {
						subtitleEvent->Style = i;
					}

					if (SDL_strcmp(style->Name,
						BuildString(event->data.print_text->character).c_str()) == 0) {
						subtitleEvent->Style = i;
						break;
					}
				}
				break;
			}

			case SetSELECT: {
				std::vector<std::string> options;
				for (char** it = event->data.set_select->options; *it != nullptr; ++it) {
					options.push_back(BuildString(*it));
				}

				gp->prompt = new Prompt(
					options, &gp->promptId, event->start, event->end, gp->tm);
				gp->putPrompt = true;
				break;
			}

			case SkipFRAME:
			case Next:
				lastTime = event->start;
				break;

			default:
				break;
			}
		}

		eventMutex = SDL_CreateMutex();
		if (eventMutex == nullptr) {
			Kotonoha_OrsClean(&eventsFromScript);
			throw std::runtime_error("Failed to create event mutex");
		}

		void** parms = static_cast<void**>(SDL_malloc(sizeof(void*) * 5));
		if (parms == nullptr) {
			Kotonoha_OrsClean(&eventsFromScript);
			SDL_DestroyMutex(eventMutex);
			eventMutex = nullptr;
			throw std::runtime_error("Failed to allocate EventManager params");
		}

		parms[0] = gameplay;
		parms[1] = gameCtx;
		parms[2] = this;
		parms[3] = new std::vector<std::tuple<std::string, int>>();
		parms[4] = new std::string();

		if (parms[3] == nullptr || parms[4] == nullptr) {
			DestroyEventManagerParams(parms);
			Kotonoha_OrsClean(&eventsFromScript);
			SDL_DestroyMutex(eventMutex);
			eventMutex = nullptr;
			throw std::runtime_error("Failed to allocate EventManager state");
		}

		eventManagerParams = parms;

		SDL_LockMutex(gameCtx->taskLock);
		auto* tasks =
			static_cast<std::vector<std::tuple<SDL_ThreadFunction, void*>>*>(
				gameCtx->processPoolTasks);

		EventManager(parms);
		tasks->emplace_back(EventManager, parms);
		SDL_UnlockMutex(gameCtx->taskLock);
	}

	bool Event::ProcessNow(struct Kotonoha_Game* gameCtx) {
		if (gameCtx == nullptr ||
			gameCtx->taskLock == nullptr ||
			eventManagerParams == nullptr) {
			return false;
		}

		// Serialize with the 50 ms worker loop so the same EventManager task
		// cannot be processed concurrently by the UI and worker threads.
		SDL_LockMutex(gameCtx->taskLock);
		const int result = EventManager(eventManagerParams);
		SDL_UnlockMutex(gameCtx->taskLock);

		SDL_Log("[KTN-0001-R3] EventManager primed synchronously");
		return result >= 0;
	}

	bool Event::CheckEnd(void* gameplay) {
		auto* gp = static_cast<Gameplay*>(gameplay);
		if (gp == nullptr || gp->tm == nullptr) {
			return true;
		}

		return Kotonoha_timeGet(gp->tm) > lastTime;
	}

	Event::~Event() {
		eventManagerParams = nullptr;
		Kotonoha_OrsClean(&eventsFromScript);
		if (eventMutex != nullptr) {
			SDL_DestroyMutex(eventMutex);
			eventMutex = nullptr;
		}
	}

} // namespace Kotonoha