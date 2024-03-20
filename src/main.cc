#include "input.hh"

#include "EventLoop.hh"
#include "Help.hh"
#include "Lyrics.hh"
#include "PlayerView.hh"
#include "Status.hh"
#include "Widget.hh"
#include "Spectralizer.hh"
#include "Config.hh"

class App {
    enum class DrawFlags : unsigned {
        None = 0x0,
        Content = 0x1,
        Status = 0x2,
        Spectre = 0x4,
        All = 0x7
    };
    const Keymap& keymap_;
    unsigned pageSize_{0};
    std::array<Terminal::Plane, 3> planes_;
    Player player_;
    Widget<PlayerView> playview_;
    Widget<Help> help_;
    Widget<Lyrics> lyrics_;
    Widget<Spectralizer> spectre_;
    Widget<Status> status_;
    IWidget* activeContent_;

    void resize() noexcept {
        auto size = Terminal::size();
        auto statusSize = config().options.showProgress ? 2U : 1U;
        auto contentSize = size.rows - statusSize;
#ifdef ENABLE_SPECTRALIZER
        // NOLINTBEGIN(readability-magic-numbers)
        auto spectrSize = config().options.spectralizer && contentSize > 15
                              ? std::max(5U, contentSize / 3)
                              : 0;
        // NOLINTEND(readability-magic-numbers)
        contentSize -= spectrSize;
#else
        constexpr auto spectrSize = 0u;
#endif
        pageSize_ = contentSize - 2;
        planes_[0].resize({0, 0, size.cols, contentSize});
        planes_[1].resize({0, contentSize, size.cols, spectrSize});
        planes_[2].resize({0, contentSize + spectrSize, size.cols, statusSize});
    }

  public:
    App(Sender<Msg> sender, const Keymap& keymap, int argc,
        char* argv[]) noexcept :
        keymap_(keymap),
        planes_(
            {term().createPlane({0, 0, 0, 0}), term().createPlane({0, 0, 0, 0}),
                term().createPlane({0, 0, 0, 0})}),
        player_(sender, argc, argv),
        help_(keymap),
        lyrics_(std::move(sender), config().lyricsProvider, config().lyricsPath),
        spectre_(player_.state(),
            [this](unsigned count) { player_.setBinCount(count); }),
        status_(player_.state(), player_.streamParams()),
        activeContent_(&playview_) {
        resize();
        render(DrawFlags::All);
    }

    void updateLyricsSong(const Entry* entry) {
        if (entry != nullptr) {
            lyrics_->setSong(entry->title);
        } else {
            lyrics_->setSong(L"");
        }
    }

    DrawFlags handleAction(Action action) {  // NOLINT(misc-no-recursion)
        auto setActive = [this](auto& widget) { activeContent_ = &widget; };
        auto modVolume = [this](double perc) {
            auto vol = player_.streamParams().volume;
            player_.setVolume(std::clamp(vol * perc + vol, 0., 1.));
        };

        auto result = DrawFlags::Status;
        switch (action) {
            case Action::Play: {
                auto queue = playview_->enter();
                if (queue) {
                    player_.emit(Command::Play, std::move(queue));
                    playview_->markPlaying(player_.currentId());
                    updateLyricsSong(player_.currentEntry());
                } else {
                    player_.clearQueue();
                }
                result = DrawFlags::All;
            } break;

            case Action::ToggleLists:
                playview_->toggleLists();
                result = DrawFlags::All;
                break;

            case Action::Down:
                activeContent_->down(1);
                result = DrawFlags::All;
                break;

            case Action::Up:
                activeContent_->up(1);
                result = DrawFlags::All;
                break;

            case Action::PgDown:
                activeContent_->down(pageSize_);
                result = DrawFlags::All;
                break;

            case Action::PgUp:
                activeContent_->up(pageSize_);
                result = DrawFlags::All;
                break;

            case Action::Home:
                activeContent_->home(true);
                result = DrawFlags::All;
                break;

            case Action::End:
                activeContent_->end();
                result = DrawFlags::All;
                break;

            case Action::Stop:
                player_.emit(Command::Stop);
                playview_->markPlaying(player_.currentId());
                result = DrawFlags::All;
                break;

            case Action::Next:
                if (std::get_if<Player::Stopped>(&player_.state()) != nullptr) {
                    return handleAction(Action::Play);
                }
                player_.emit(Command::Next);
                playview_->markPlaying(player_.currentId());
                updateLyricsSong(player_.currentEntry());
                result = DrawFlags::All;
                break;

            case Action::Prev:
                player_.emit(Command::Prev);
                playview_->markPlaying(player_.currentId());
                result = DrawFlags::All;
                break;

            case Action::Pause:
                player_.emit(Command::Pause);
                break;

            case Action::ToggleProgress:
                config().options.showProgress = !config().options.showProgress;
                resize();
                result = DrawFlags::All;
                break;

            case Action::ToggleShuffle:
                config().options.shuffle = !config().options.shuffle;
                player_.updateShuffleQueue();
                break;

            case Action::ToggleRepeat:
                config().options.repeat = !config().options.repeat;
                break;

            case Action::ToggleNext:
                config().options.next = !config().options.next;
                break;

            case Action::AddToPlaylist:
                playview_->addToPlaylist();
                result = DrawFlags::All;
                break;

            case Action::Delete:
                playview_->delSelected();
                result = DrawFlags::All;
                break;

            case Action::Clear:
                playview_->clear();
                result = DrawFlags::All;
                break;

            case Action::ToggleLyrics:
                if (activeContent_ == &lyrics_) {
                    setActive(playview_);
                    lyrics_->activate(false);
                } else {
                    if (player_.currentEntry() == nullptr) {
                        auto& playList = playview_->operator[](
                            static_cast<int>(playview_->playlistActive()));
                        if (auto sel = playList.selectedIndex()) {
                            lyrics_->setSong(playList[*sel].title);
                        }
                    }
                    lyrics_->activate(true);
                    setActive(lyrics_);
                }
                result = DrawFlags::All;
                break;

            case Action::ResetView:
                setActive(playview_);
                result = DrawFlags::All;
                break;

            case Action::Rew:
                player_.rew();
                break;

            case Action::FF:
                player_.ff();
                break;

            case Action::ToggleHelp:
                if (activeContent_ == &help_) {
                    setActive(playview_);
                } else {
                    setActive(help_);
                }
                result = DrawFlags::All;
                break;

            case Action::ToggleSpectralizer:
                config().options.spectralizer = !config().options.spectralizer;
                resize();
                result = DrawFlags::All;
                break;

                // NOLINTBEGIN(readability-magic-numbers)
            case Action::VolUp1:
                modVolume(0.01);
                break;

            case Action::VolDn1:
                modVolume(-0.01);
                break;

            case Action::VolUp5:
                modVolume(0.05);
                break;

            case Action::VolDn5:
                modVolume(-0.05);
                break;

            case Action::VolSet10:
                player_.setVolume(0.1);
                break;

            case Action::VolSet20:
                player_.setVolume(0.2);
                break;

            case Action::VolSet30:
                player_.setVolume(0.3);
                break;

            case Action::VolSet40:
                player_.setVolume(0.4);
                break;

            case Action::VolSet50:
                player_.setVolume(0.5);
                break;

            case Action::VolSet60:
                player_.setVolume(0.6);
                break;

            case Action::VolSet70:
                player_.setVolume(0.7);
                break;

            case Action::VolSet80:
                player_.setVolume(0.8);
                break;

            case Action::VolSet90:
                player_.setVolume(0.9);
                break;

            case Action::VolSet100:
                player_.setVolume(1.0);
                break;
                // NOLINTEND(readability-magic-numbers)

            case Action::Quit:
            case Action::Count:
                result = DrawFlags::None;
                break;
        }
        return result;
    }

    void handleEvent(const Msg& msg) {
        std::visit(
            [this](auto&& value) {
                using Type = std::decay_t<decltype(value)>;
                auto drawFlags = DrawFlags::All;
                if constexpr (std::is_same<Type, input::Key>()) {
                    if (value == input::Resize) {
                        resize();
                    } else if (auto action = keymap_.map(value)) {
                        drawFlags = handleAction(*action);
                    }
                } else if constexpr (std::is_same<Type, unsigned>()) {
                    if (value != Player::EndOfSong) {
                        status_->setProgress(value);
                        drawFlags = DrawFlags::Status;
                    } else {
                        status_->setProgress(0);
                        player_.emit(Command::Next);
                        playview_->markPlaying(player_.currentId());
                        updateLyricsSong(player_.currentEntry());
                    }
#ifdef ENABLE_SPECTRALIZER
                } else if constexpr (std::is_same<Type, std::vector<float>>()) {
                    spectre_->applyBins(std::forward<decltype(value)>(value));
                    drawFlags = DrawFlags::Spectre;
#endif
                } else if constexpr (std::is_same<Type, Action>()) {
                    drawFlags = handleAction(value);
                }
                render(drawFlags);
            },
            msg);
    }

    void render(DrawFlags flags) noexcept {
        auto hasFlag = [&flags](DrawFlags value) {
            auto queryValue =
                static_cast<std::underlying_type_t<DrawFlags>>(value);
            return (static_cast<std::underlying_type_t<DrawFlags>>(flags) &
                       queryValue) == queryValue;
        };
        if (hasFlag(DrawFlags::Content)) {
            activeContent_->render(planes_[0]);
            term() << planes_[0];
        }
#ifdef ENABLE_SPECTRALIZER
        if (hasFlag(DrawFlags::Spectre) && config().options.spectralizer) {
            spectre_.render(planes_[1]);
            term() << planes_[1];
        }
#endif
        if (hasFlag(DrawFlags::Status)) {
            status_.render(planes_[2]);
            term() << planes_[2];
        }
        Terminal::render();
    }
};

int main(int argc, char* argv[]) try {
    auto [sender, receiver] = channel<Msg>();
    auto& conf = config();
    Terminal::loadTheme(conf.themePath.c_str());
    auto keymap = Keymap(conf.keymapPath);
    auto eventLoop = EventLoop(sender, keymap, conf.socketPath.c_str());
    auto app = App(sender, keymap, argc, argv);

    auto doQuit = [&keymap](const Msg& msg) {
        return std::visit(
            [&keymap](auto&& value) {
                using Type = std::decay_t<decltype(value)>;
                if constexpr (std::is_same<Type, input::Key>()) {
                    if (auto action = keymap.map(value)) {
                        return *action == Action::Quit;
                    }
                } else if constexpr (std::is_same<Type, Action>()) {
                    return value == Action::Quit;
                }
                return false;
            },
            msg);
    };

    while (true) {
        auto msg = receiver.recv();
        app.handleEvent(msg);
        if (doQuit(msg)) {
            while (receiver.tryRecv()) {
            }
            break;
        }
    }
    return 0;
} catch (std::exception& error) {
}
