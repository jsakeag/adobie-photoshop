#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ps/viewport.h"

using namespace ps;
using Catch::Approx;

TEST_CASE("Viewport maps between document and screen", "[viewport]") {
  Viewport view;
  CHECK(view.zoom() == 1.0);
  CHECK(view.doc_to_screen({3, 4}).x == 3);

  view.zoom_about({0, 0}, 2.0);
  view.set_offset({10, 20});
  const Vec2 s = view.doc_to_screen({5, 7});
  CHECK(s.x == Approx(20));
  CHECK(s.y == Approx(34));

  const Vec2 d = view.screen_to_doc(s);
  CHECK(d.x == Approx(5));
  CHECK(d.y == Approx(7));
}

TEST_CASE("Viewport pan moves the document on screen", "[viewport]") {
  Viewport view;
  view.pan({15, -5});
  view.pan({5, 5});
  CHECK(view.offset().x == Approx(20));
  CHECK(view.offset().y == Approx(0));
  CHECK(view.screen_to_doc({20, 0}).x == Approx(0));
}

TEST_CASE("Viewport zoom keeps the anchor point fixed", "[viewport]") {
  Viewport view;
  view.set_offset({40, 30});
  const Vec2 anchor{123, 77};
  const Vec2 before = view.screen_to_doc(anchor);

  view.zoom_about(anchor, 4.0);
  CHECK(view.zoom() == 4.0);
  CHECK(view.screen_to_doc(anchor).x == Approx(before.x));
  CHECK(view.screen_to_doc(anchor).y == Approx(before.y));

  view.zoom_about(anchor, 0.25);
  CHECK(view.screen_to_doc(anchor).x == Approx(before.x));
  CHECK(view.screen_to_doc(anchor).y == Approx(before.y));
}

TEST_CASE("Viewport zoom is clamped", "[viewport]") {
  Viewport view;
  view.zoom_about({0, 0}, 1000.0);
  CHECK(view.zoom() == Viewport::kMaxZoom);
  view.zoom_about({0, 0}, 0.0001);
  CHECK(view.zoom() == Viewport::kMinZoom);
}

TEST_CASE("Viewport steps through fixed zoom ratios", "[viewport]") {
  Viewport view;
  view.zoom_in({0, 0});
  CHECK(view.zoom() == 2.0);
  view.zoom_in({0, 0});
  CHECK(view.zoom() == 3.0);
  view.zoom_out({0, 0});
  view.zoom_out({0, 0});
  CHECK(view.zoom() == 1.0);
  view.zoom_out({0, 0});
  CHECK(view.zoom() == Approx(2.0 / 3.0));

  SECTION("an in-between zoom snaps to the neighbouring step") {
    view.zoom_about({0, 0}, 2.5);
    view.zoom_in({0, 0});
    CHECK(view.zoom() == 3.0);
    view.zoom_about({0, 0}, 2.5);
    view.zoom_out({0, 0});
    CHECK(view.zoom() == 2.0);
  }

  SECTION("stepping stops at the limits") {
    for (int i = 0; i < 40; ++i) view.zoom_in({0, 0});
    CHECK(view.zoom() == Viewport::kMaxZoom);
    for (int i = 0; i < 40; ++i) view.zoom_out({0, 0});
    CHECK(view.zoom() == Viewport::kMinZoom);
  }
}

TEST_CASE("Viewport fit", "[viewport]") {
  Viewport view;

  SECTION("a small document is centred at 100%") {
    view.fit(100, 50, 400, 300);
    CHECK(view.zoom() == 1.0);
    CHECK(view.offset().x == Approx(150));
    CHECK(view.offset().y == Approx(125));
  }

  SECTION("a large document is scaled down to fit") {
    view.fit(800, 300, 400, 300);
    CHECK(view.zoom() == Approx(0.5));
    CHECK(view.offset().x == Approx(0));
    CHECK(view.offset().y == Approx(75));
    const Vec2 corner = view.doc_to_screen({800, 300});
    CHECK(corner.x == Approx(400));
    CHECK(corner.y == Approx(225));
  }
}
