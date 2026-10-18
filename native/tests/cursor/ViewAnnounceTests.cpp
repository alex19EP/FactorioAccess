// When the view's zoom and an entered view are said (src/viewannounce.h): a zoom once it holds
// still, the first one after a load only recorded, a view with the zoom it settles on.

#include "TestHarness.hpp"
#include "viewannounce.h"

using fa::viewannounce::Announcer;
using fa::viewannounce::kHeldFrames;
using fa::viewannounce::kSettleFrames;
using fa::viewannounce::kViewFrames;
using fa::viewannounce::Say;

namespace
{

// Shows `zoom` for `frames` frames; how many of them said what.
struct Heard
{
    int Zoom = 0;
    int View = 0;
};

Heard Show(Announcer& announcer, double zoom, int frames)
{
    Heard heard;
    for (int i = 0; i < frames; ++i)
    {
        const Say say = announcer.frame(zoom);
        if (say == Say::Zoom)
            ++heard.Zoom;
        if (say == Say::View)
            ++heard.View;
    }
    return heard;
}

// A game loaded at zoom 1, whose first zoom is recorded unheard.
Announcer Loaded()
{
    Announcer announcer;
    Show(announcer, 1.0, kSettleFrames + 2);
    return announcer;
}

} // namespace

TEST(ViewAnnounce_FirstZoomIsOnlyRecorded)
{
    Announcer announcer;
    const Heard heard = Show(announcer, 1.0, kSettleFrames + 20);
    CHECK_EQ(0, heard.Zoom);
    CHECK_EQ(0, heard.View);
}

TEST(ViewAnnounce_ZoomEasedInAsAMapStartsIsNotSaid)
{
    Announcer announcer;
    for (double zoom = 2.0; zoom > 1.5; zoom -= 0.05)
        CHECK_EQ(0, Show(announcer, zoom, kHeldFrames + 2).Zoom);
    CHECK_EQ(0, Show(announcer, 1.5, kSettleFrames + 2).Zoom);
    CHECK_EQ(1, Show(announcer, 1.0, 20).Zoom);
}

TEST(ViewAnnounce_ZoomIsSaidOnceItHolds)
{
    Announcer announcer = Loaded();
    CHECK_EQ(0, Show(announcer, 0.5, kHeldFrames).Zoom);
    CHECK_EQ(1, Show(announcer, 0.5, 20).Zoom);
}

TEST(ViewAnnounce_StepsOfABurstAreSaidAsOne)
{
    Announcer announcer = Loaded();
    CHECK_EQ(0, Show(announcer, 0.9, 1).Zoom);
    CHECK_EQ(0, Show(announcer, 0.8, 1).Zoom);
    CHECK_EQ(0, Show(announcer, 0.7, 1).Zoom);
    CHECK_EQ(1, Show(announcer, 0.7, 20).Zoom);
}

TEST(ViewAnnounce_ZoomBackToTheSpokenOneIsNotSaid)
{
    Announcer announcer = Loaded();
    Show(announcer, 0.9, 1);
    CHECK_EQ(0, Show(announcer, 1.0, 20).Zoom);
}

TEST(ViewAnnounce_ViewIsSaidWithTheZoomItSettlesOn)
{
    Announcer announcer = Loaded();
    announcer.entered();
    CHECK(announcer.viewPending());
    // Remote view opens on the full map, then is zoomed to where the jump left it.
    Show(announcer, 0.1, kViewFrames - 3);
    const Heard heard = Show(announcer, 0.5, 20);
    CHECK_EQ(1, heard.View);
    CHECK_EQ(0, heard.Zoom);
    CHECK_FALSE(announcer.viewPending());
}

TEST(ViewAnnounce_ViewWithTheSameZoomWaitsItsFrames)
{
    Announcer announcer = Loaded();
    announcer.entered();
    CHECK_EQ(0, Show(announcer, 1.0, kViewFrames - 1).View);
    CHECK_EQ(1, Show(announcer, 1.0, 20).View);
}

TEST(ViewAnnounce_ViewRightAfterALoadIsSaid)
{
    Announcer announcer;
    announcer.entered();
    CHECK_EQ(1, Show(announcer, 1.0, 20).View);
}
