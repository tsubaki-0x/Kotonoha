#include <Kotonoha/routing/SchoolDaysFrontendTransition.hpp>
#include <Kotonoha/routing/SchoolDaysExitTransition.hpp>

#include <cstdlib>
#include <iostream>

namespace {

using Transition = Kotonoha::SchoolDaysFrontendTransition;

[[noreturn]] void Fail(const char* message) {
    std::cerr << "SchoolDaysFrontendTransitionTest: " << message << '\n';
    std::exit(2);
}

void Check(bool value, const char* message) {
    if (!value) Fail(message);
}

} // namespace

int main() {
    const auto start = Transition::SpecFor(Transition::Profile::TitleToStart);
    Check(start.outgoingMs == 1000u &&
              start.outgoingColor == Transition::Color::Black &&
              start.incomingMs == 0u,
          "TitleToStart profile drifted");

    const auto child = Transition::SpecFor(Transition::Profile::TitleToChild);
    const auto back = Transition::SpecFor(Transition::Profile::ChildToTitle);
    Check(child.outgoingMs == 500u && child.incomingMs == 500u &&
              child.outgoingColor == Transition::Color::White &&
              child.incomingColor == Transition::Color::White &&
              back.outgoingMs == 500u && back.incomingMs == 500u &&
              back.outgoingColor == Transition::Color::White &&
              back.incomingColor == Transition::Color::White,
          "white child transition profiles drifted");

    Check(Transition::OutgoingAlpha(0u, 500u) == 0u &&
              Transition::OutgoingAlpha(250u, 500u) == 127u &&
              Transition::OutgoingAlpha(500u, 500u) == 255u &&
              Transition::IncomingAlpha(0u, 500u) == 255u &&
              Transition::IncomingAlpha(250u, 500u) == 128u &&
              Transition::IncomingAlpha(500u, 500u) == 0u,
          "integer alpha projection drifted");

    Transition transition;
    Check(!transition.Active(0u) && transition.Alpha(0u) == 0u,
          "fresh transition was not idle");

    transition.Begin(Transition::Profile::TitleToStart, 100u);
    Check(transition.CurrentPhase(100u) == Transition::Phase::Outgoing &&
              transition.CurrentColor() == Transition::Color::Black &&
              transition.Alpha(600u) == 127u &&
              !transition.NeedsHandoff(1099u) &&
              transition.NeedsHandoff(1100u),
          "TitleToStart outgoing/handoff timing drifted");
    transition.CompleteHandoff(1100u);
    Check(!transition.Active(1100u) && transition.Alpha(1100u) == 0u,
          "TitleToStart zero-duration incoming did not finish immediately");

    transition.Begin(Transition::Profile::TitleToChild, 2000u);
    Check(transition.Alpha(2250u) == 127u &&
              !transition.NeedsHandoff(2499u) &&
              transition.NeedsHandoff(2500u),
          "TitleToChild outgoing timing drifted");
    transition.CompleteHandoff(2500u);
    Check(transition.CurrentPhase(2500u) == Transition::Phase::Incoming &&
              transition.CurrentColor() == Transition::Color::White &&
              transition.Alpha(2500u) == 255u &&
              transition.Alpha(2750u) == 128u &&
              !transition.Active(3000u),
          "TitleToChild incoming timing drifted");

    transition.BeginIncoming(Transition::Color::Black, 250u, 4000u);
    Check(transition.CurrentPhase(4000u) == Transition::Phase::Incoming &&
              transition.CurrentColor() == Transition::Color::Black &&
              transition.Alpha(4125u) == 128u &&
              !transition.Active(4250u),
          "explicit incoming transition drifted");

    transition.Begin(Transition::Profile::ChildToTitle, 5000u);
    Check(transition.Alpha(4999u) == 0u,
          "clock regression was not clamped to zero elapsed");
    transition.Reset();
    Check(!transition.Active(6000u) &&
              transition.CurrentPhase(6000u) == Transition::Phase::Idle,
          "Reset did not restore idle state");

    using Exit = Kotonoha::SchoolDaysExitTransition;
    Exit exit;
    exit.Begin(Exit::Target::Title, 7000u);
    Check(exit.InputLocked() && exit.RevealedFraction(7250u) == 0.5f &&
              exit.Update(7499u) == Exit::Event::None &&
              exit.Update(7500u) == Exit::Event::OpenDialog &&
              !exit.InputLocked(),
          "title confirmation must open after the 500 ms vertical wipe");
    exit.Cancel(7600u);
    Check(exit.InputLocked() && exit.Alpha(7850u) == 128u &&
              exit.Update(8100u) == Exit::Event::Closed && !exit.Active(),
          "cancel must fade SYSBASE away over 500 ms");
    exit.Begin(Exit::Target::Title, 9000u);
    Check(exit.Update(9500u) == Exit::Event::OpenDialog, "title dialog did not open");
    exit.Confirm(9600u);
    Check(exit.ConfirmMs() == 500u && exit.Alpha(9850u) == 127u &&
              exit.Update(10100u) == Exit::Event::Confirmed,
          "TOPMENU confirmation fade timing drifted");
    exit.Begin(Exit::Target::Quit, 11000u);
    Check(exit.Update(11500u) == Exit::Event::OpenDialog, "exit dialog did not open");
    exit.Confirm(11600u);
    Check(exit.ConfirmMs() == 750u && exit.Update(12349u) == Exit::Event::None &&
              exit.Update(12350u) == Exit::Event::Confirmed,
          "EXIT shutdown fade timing drifted");

    std::cout << "SCHOOL DAYS FRONTEND TRANSITION GATE PASS\n"
              << "profile_specs=PASS\n"
              << "integer_alpha=PASS\n"
              << "title_start_black_1000ms=PASS\n"
              << "title_child_white_500_500ms=PASS\n"
              << "child_title_profile=PASS\n"
              << "explicit_incoming=PASS\n"
              << "clock_regression_clamp=PASS\n";
    std::cout << "exit_title_sysbase_500ms=PASS\n"
              << "exit_cancel_500ms=PASS\n"
              << "topmenu_confirm_500ms=PASS\n"
              << "exit_confirm_750ms=PASS\n";
    return 0;
}
