#include "input.hh"

#include "EventLoop.hh"
#include "Help.hh"
#include "Lyrics.hh"
#include "PlayerView.hh"
#include "Status.hh"
#include "Widget.hh"

class App {
    enum class DrawFlags : unsigned {
        None = 0x0,
        Content = 0x1,
        Status = 0x2,
        All = 0x3
    };
    Config& config_;
    const Keymap& keymap_;
    unsigned pageSize_{0};
    std::array<Terminal::Plane, 2> planes_;
    Player player_;
    Widget<PlayerView> playview_;
    Widget<Help> help_;
    Widget<Lyrics> lyrics_;
    Widget<Status> status_;
    IWidget* activeContent_;

    void resize() noexcept {
        auto size = term().size();
        auto statusSize = config_.options.showProgress ? 2u : 1u;
        auto contentSize = size.rows - statusSize;
        pageSize_ = contentSize - 2;
        planes_[0].resize({0, 0, size.cols, contentSize});
        planes_[1].resize({0, contentSize, size.cols, statusSize});
    }

  public:
    explicit App(Sender<Msg> sender, Config& config, const Keymap& keymap,
        int argc, char* argv[]) :
        config_(config),
        keymap_(keymap),
        planes_({term().createPlane({0, 0, 0, 0}),
            term().createPlane({0, 0, 0, 0})}),
        player_(std::move(sender), config_.options, argc, argv),
        playview_(config_),
        help_(keymap),
        status_(config_, player_.state(), player_.streamParams()),
        activeContent_(&playview_) {
        resize();
        render(DrawFlags::All);
    }

    DrawFlags handleAction(Action action) {
        auto setActive = [this](auto& widget) { activeContent_ = &widget; };
        auto result = DrawFlags::All;
        switch (action) {
            case Action::Play: {
                auto queue = playview_->enter();
                if (queue) {
                    player_.emit(Command::Play, std::move(queue));
                    playview_->markPlaying(player_.currentId());
                } else {
                    player_.clearQueue();
                }
            } break;

            case Action::ToggleLists:
                playview_->toggleLists();
                break;

            case Action::Down:
                activeContent_->down(1);
                break;

            case Action::Up:
                activeContent_->up(1);
                break;

            case Action::PgDown:
                activeContent_->down(pageSize_);
                break;

            case Action::PgUp:
                activeContent_->up(pageSize_);
                break;

            case Action::Home:
                activeContent_->home(true);
                break;

            case Action::End:
                activeContent_->end();
                break;

            case Action::Stop:
                player_.emit(Command::Stop);
                playview_->markPlaying(player_.currentId());
                break;

            case Action::Next:
                player_.emit(Command::Next);
                playview_->markPlaying(player_.currentId());
                lyrics_->setSong(player_.currentSong());
                break;

            case Action::Prev:
                player_.emit(Command::Prev);
                playview_->markPlaying(player_.currentId());
                break;

            case Action::Pause:
                player_.emit(Command::Pause);
                result = DrawFlags::Status;
                break;

            case Action::ToggleProgress:
                config_.options.showProgress = !config_.options.showProgress;
                resize();
                break;

            case Action::ToggleShuffle:
                config_.options.shuffle = !config_.options.shuffle;
                player_.updateShuffleQueue();
                result = DrawFlags::Status;
                break;

            case Action::ToggleRepeat:
                config_.options.repeat = !config_.options.repeat;
                result = DrawFlags::Status;
                break;

            case Action::ToggleNext:
                config_.options.next = !config_.options.next;
                result = DrawFlags::Status;
                break;

            case Action::AddToPlaylist:
                playview_->addToPlaylist();
                break;

            case Action::Delete:
                playview_->delSelected();
                break;

            case Action::Clear:
                playview_->clear();
                break;

            case Action::ToggleLyrics:
                if (activeContent_ == &lyrics_) {
                    setActive(playview_);
                    lyrics_->activate(false);
                } else {
                    setActive(lyrics_);
                    lyrics_->activate(true);
                }
                break;

            case Action::ResetView:
                setActive(playview_);
                break;

            case Action::Rew:
                player_.rew();
                result = DrawFlags::Status;
                break;

            case Action::FF:
                player_.ff();
                result = DrawFlags::Status;
                break;

            case Action::ToggleHelp:
                if (activeContent_ == &help_) {
                    setActive(playview_);
                } else {
                    setActive(help_);
                }
                break;

            case Action::Quit:
            case Action::Count:
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
                        lyrics_->setSong(player_.currentSong());
                    }
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
        if (hasFlag(DrawFlags::Status)) {
            status_.render(planes_[1]);
            term() << planes_[1];
        }
        term().render();
    }
};

int main(int argc, char* argv[]) {
    std::setlocale(LC_ALL, "");  // NOLINT:concurrency-mt-unsafe
    auto [sender, receiver] = channel<Msg>();
    auto config = Config();
    loadTheme(config.themePath.c_str());

    auto keymap = Keymap(config.keymapPath);
    auto eventLoop = EventLoop(sender, keymap, config.socketPath.c_str());
    auto app = App(sender, config, keymap, argc, argv);

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
            break;
        }
    }
    return 0;
}
