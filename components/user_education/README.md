# User Education component library

This library contains the code that will allow you to implement
**In-Product-Help (IPH)** and **Tutorials** in any framework, as well as display
the **"New" Badge** on menus and labels.

## Upstream dependencies

Familiarity with these libraries are strongly recommended; feel free to reach
out to their respective OWNERS if you have any questions.

  * [UI Interaction](/ui/base/interaction/README.md)
    * [ElementTracker](/ui/base/interaction/element_tracker.h) - supplies anchor
      points for help bubbles
    * [InteractionSequence](/ui/base/interaction/interaction_sequence.h) -
      describes the situations in which a Tutorial advances to the next step
  * [Feature Engagement](/components/feature_engagement/README.md) - used to
    evaluate triggering conditions for IPH and New Badge.

## Directory structure

 * [common](./common) - contains platform- and framework-agnostic APIs for
   working with `HelpBubble`s, **IPH**, and **Tutorials**.
 * [test](./test) - contains common code for testing user education primitives
 * [views](./views) - contains code required to display a `HelpBubble` in a
   Views-based UI, as well as **"New" Badge** primitives.
 * [webui](./webui/README.md) - contains code required to display a `HelpBubble`
   on a WebUI surface.

# Programming API

## Help bubbles

The core presentation element for both IPH and Tutorials is the
[HelpBubble](./common/help_bubble.h). A `HelpBubble` is a blue bubble that
appears anchored to an element in your application's UI and which contains
information about that element. For example, a `HelpBubble` might appear
underneath the profile button the first time the user starts Chrome after
adding a second profile, showing the user how they can switch between profiles.

Different UI frameworks have different `HelpBubble` implementations; for
example, [HelpBubbleViews](./views/help_bubble_factory_views.h). Each type of
`HelpBubble` is created by a different
[HelpBubbleFactory](./common/help_bubble_factory.h), which is registered at
startup in the global
[HelpBubbleFactoryRegistry](./common/help_bubble_factory_registry.h). So for
example, Chrome registers separate factories for Views and WebUI, and on Mac
a third factory that can attach a Views-based `HelpBubble` to a Mac native menu.

To actually show the bubble, the `HelpBubbleFactoryRegistry` needs two things:
  * The `TrackedElement` the bubble will be anchored to
  * The [HelpBubbleParams](./common/help_bubble_params.h) describing the bubble

You will notice that this is an extremely bare-bones system. ***You are not
expected to call `HelpBubbleFactoryRegistry` directly!*** Rather, the IPH and
Tutorial systems use this API to show help bubbles.

## In-Product Help (IPH)

In-Product Help is the simpler of the two ways to display help bubbles, and can
even be the entry point for a Tutorial.

IPH are:
 * **Spontaneous** - they are shown to the user when a set of conditions are
   met; the user does not choose to see them.
 * **Rate-limited** - the user will only ever see a specific IPH a certain
   number of times, and will only see a certain total number of different IPH
   per session.
 * **Simple** - only a small number of templates approved by UX are available,
   for different kinds of User Education journeys.

Your application will provide a
[FeaturePromoController](./common/feature_promo_controller.h) with a
[FeaturePromoRegistry](./common/feature_promo_registry.h). In order to add a new
IPH, you will need to:
 1. Add the `base::Feature` corresponding to the IPH.
 2. Register the
    [FeaturePromoSpecification](./common/feature_promo_specification.h)
    describing your IPH journey (see below).
 3. Configure the Feature Engagement backend for your IPH journey
    ([see documentation](/components/feature_engagement/README.md)).
 4. Put hooks in your code:
    * Call `FeaturePromoController::MaybeShowPromo()` at the point in the code
      when the promo should trigger, adding feature-specific logic for when the
      promo is appropriate.
    * Add additional calls to `feature_engagement::Tracker::NotifyEvent()` for
      events that should affect whether the IPH should display.
      * These should also be referenced in the Feature Engagement configuration.
      * This should include the user actually engaging with the feature being
        promo'd.
      * You can retrieve the tracker via
        `FeaturePromoControllerCommon::feature_engagement_tracker()`.
    * Optionally: add calls to `FeaturePromoController::CloseBubble()` or
      `FeaturePromoController::CloseBubbleAndContinuePromo()` to
      programmatically end the promo when the user engages your feature.
 5. Enable the feature via a trade study or Finch.

### Registering your IPH

You will want to create a `FeaturePromoSpecification` and register it with
`FeaturePromoRegistry::RegisterFeature()`. There should be a common function
your application uses to register IPH journeys during startup; in Chrome it's
`MaybeRegisterChromeFeaturePromos()`.

There are several factory methods on FeaturePromoSpecification for different
types of IPH:
  * **CreateForToastPromo** - creates a small, short-lived promo with no buttons
    that disappears after a short time.
    * These are designed to point out a specific UI element; you will not expect
      the user to interact with the bubble.
    * Because of this a screen reader message and accelerator to access the
      relevant feature are required; this will be used to make sure that screen
      reader users can find the thing the bubble is talking about.
  * **CreateForSnoozePromo** - creates a promo with "got it" and "remind me
    later" buttons and if the user picks the latter, snoozes the promo so it can
    be displayed again later.
  * **CreateForTutorialPromo** - similar to `CreateForSnoozePromo()` except that
    the "got it" button is replaced by a "learn more" button that launches a
    Tutorial.
  * **CreateForLegacyPromo (DEPRECATED)** - creates a toast-like promo with no
    buttons, but which does not require accessible text and has no or a long
    timeout. *For backwards compatibility with older promos; do not use.*

You may also call the following methods to add additional features to a bubble:
  * **SetBubbleTitleText()** - adds an optional title to the bubble; this will
    be in a larger font above the body text.
  * **SetBubbleIcon()** - adds an optional icon to the bubble; this will be
    displayed to the left (right in RTL) of the title/body.
  * **SetBubbleArrow()** - sets the position of the arrow relative to the
    bubble; this in turn changes the bubble's default orientation relative to
    its anchor.

These are advanced features
  * **SetInAnyContext()** - allows the system to search for the anchor element
    in any context rather than only the window in which the IPH is triggered.
  * **SetAnchorElementFilter()** - allows the system to narrow down the anchor
    from a collection of candidates, if there is more than one element maching
    the anchor's `ElementIdentifier`.

## Tutorials

Tutorials are the more complicated, in-depth way to display a series of help
bubbles. Often an IPH is an entry point to a Tutorial but Tutorials can also be
launched from e.g. a "What's New" or "Tips" page.

Tutorials are:
  * **Intentional** - the user must always opt-in to seeing a Tutorial.
  * **Repeatable** - the user may view a Tutorial any number of times, and may
    view any number of Tutorials.
  * **In-Depth** - a Tutorial can breadcrumb the user around the UI, requesting
    the user engage in any number of behaviors, and will respond to those
    actions.

Your application will provide a
[TutorialService](./common/tutorial_service.h) with a
[TutorialRegistry](./common/tutorial_registry.h). In order to add a new
Tutorial, you will need to:
  1. Declare a [TutorialIdentifier](./common/tutorial_identifier.h) in an
     accessible location.
  2. Register the `TutorialIdentifier` and
     [TutorialDescription](./common/tutorial_description.h); details will be
     provided below.
  3. Create an entry point for the Tutorial:
     * Directly call `TutorialService::StartTutorial()`, or...
     * Register an IPH using the `CreateForTutorialPromo()` factory method.

### Defining and registering your Tutorial

A [TutorialDescription](./common/tutorial_description.h) is the template from
which a [Tutorial](./common/tutorial.h) is built. A `Tutorial` is a stateful,
executable object that "runs" the Tutorial itself; since they can't be reused,
one needs to be created every time the Tutorial is started.

There are only a few fields in a `TutorialDescription`:
  * **steps** - Contains a sequence of user actions, UI changes, and the help
    bubbles that will be shown in each step.
  * **histograms** - Must be populated if you want UMA histograms regarding user
    engagement with your Tutorial.
      * The preferred syntax is:
        ```
        const char kMyTutorialHistogramPrefix[] = "MyTutorial";
        
        tutorial_description.histograms =
            user_education::MakeTutorialHistograms<kMyTutorialHistogramPrefix>(
                tutorial_description.steps.size());
        ```
      * The `kMyTutorialHistogramPrefix` needs to be declared as a local
        `const char[]` and have a globally-unique value. This will appear in UMA
        histogram entries associated with your tutorial. If this value is
        duplicated the histogram behavior will be undefined.
      * Note that this cannot be done automatically by the TutorialRegistry as
        the UMA histograms won't work without the static declarations
        implemented by the `TutorialHistogramsImpl<>` template class (via C++
        template specialization magic).
  * **can_be_restarted** - If set to `true` the Tutorial will provide an option
    to restart on the last step, in case the user wants to see the Tutorial
    again.
    * Setting this to `false` (the default) will not prevent the user from
      triggering the Tutorial again via other means.

`TutorialDescription::Step` is a bit more complex. Steps may
either be created all at once with the omnibus constructor, or created with the
default constructor and then have each field set individually. The fields of the
struct are as follows:
  * Help bubble parameters:
    * **body_text_id** - Localized string ID. The result is placed into
      `HelpBubbleParams::body_text`. If not set, this Tutorial step is a "hidden
      step" and will have no bubble.
    * **title_text_id** - Localized string ID. The result is placed into
      `HelpBubbleParams::title_text`.
    * **element_id** - Specifies the UI element the step refers to. If this is
      not a hidden step, the bubble will anchor to this element. Mandatory
      unless `element_name` is set.
    * **arrow** - Specifies how the `HelpBubble` for this step will anchor to
      the target element.
  * Interaction sequence parameters; see 
    [InteractionSequence](/ui/base/interaction/interaction_sequence.h) for
    details:
    * **step_type** - Specifies the triggering condition of this step.
    * **event_type** - If `step_type` is `kCustomEvent`, specifies the custom
      event the step will transition on. Ignored otherwise.
    * **name_elements_callback** - Allows either the current element or some
      other element to be "named" for use later in the Tutorial. This allows a
      Tutorial to remember elements that may otherwise be ambiguous or not have
      an `ElementIdentifier` before the Tutorial runs.
    * **element_name** - Specifies that the step will target an element named
      via `name_elements_callback` in a previous step, rather than using
      `element_id`. The element must have been named and still be visible.
    * **transition_only_on_event** - When `step_type` is `kShown` or `kHidden`,
      causes this step to start only when a UI element actively becomes visible
      or loses visibility. Corresponds to
      `InteractionSequence::StepBuilder::SetTransitionOnlyOnEvent()`.
    * **must_remain_visible** - Overrides the default "must remain visible"
      state of the underlying `InteractionSequence::Step`. Should only be set if
      the Tutorial won't work properly otherwise.

Notes:
  * `TutorialDescription::Step` is copyable and a step can be added to the
    `steps` member of multiple related `TutorialDescription`s.
  * We are aware that the programming interface for `Step` is a little clunky;
    at some future point they will be moved to a builder pattern like
    `FeaturePromoSpecification`.
  * If you're not sure how to construct your Tutorial, reach out to one of the
    OWNERS of this library.

Once you have defined your Tutorial; call `AddTutorial()` on the
[TutorialRegistry](./common/tutorial_registry.h) provided by your application
and pass both your `TutorialIdentifier` and your `TutorialDescription`.

## "New" Badge

For implementation on adding a "New" Badge to Chrome, Googlers can refer to the
following document:
[New Badge How-To and Best Practices](https://goto.google.com/new-badge-how-to).

# Adding User Education to your application

There are a number of virtual methods that must be implemented before you can
use these User Education libraries in a new application, mostly centered around
localization, accelerators, and global input focus.

Fortunately for Chromium developers, the browser already has the necessary
support built in for Views, WebUI, and Mac-native context menus. You may refer
to the following locations for an example that could be extended to other
platforms such as ChromeOS:
  * [UserEducationService](
    /chrome/browser/ui/user_education/user_education_service.h) - sets up the
    various registries and `TutorialService`.
  * [BrowserView](/chrome/browser/ui/views/frame/browser_view.cc#831) - sets up
    the `FeaturePromoController`.
  * [browser_user_education_service](
    /chrome/browser/ui/views/user_education/browser_user_education_service.h) -
    registers Chrome-specific IPH and Tutorials.
  * Concrete implementations of abstract User Education base classes can be
    found in [c/b/ui/user_education](/chrome/browser/ui/user_education/) and
    [c/b/ui/views/user_education](/chrome/browser/ui/views/user_education/).
