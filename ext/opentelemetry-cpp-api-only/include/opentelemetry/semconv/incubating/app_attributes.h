/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DO NOT EDIT, this is an Auto-generated file from:
 * buildscripts/semantic-convention/templates/registry/semantic_attributes-h.j2
 */

#pragma once

#include "opentelemetry/common/macros.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv
{
namespace app
{

/**
  A unique identifier representing the installation of an application on a specific device
  <p>
  Its value SHOULD persist across launches of the same application installation, including through
  application upgrades. It SHOULD change if the application is uninstalled or if all applications of
  the vendor are uninstalled. Additionally, users might be able to reset this value (e.g. by
  clearing application data). If an app is installed multiple times on the same device (e.g. in
  different accounts on Android), each @code app.installation.id @endcode SHOULD have a different
  value. If multiple OpenTelemetry SDKs are used within the same application, they SHOULD use the
  same value for @code app.installation.id @endcode. Hardware IDs (e.g. serial number, IMEI, MAC
  address) MUST NOT be used as the @code app.installation.id @endcode. <p> For iOS, this value
  SHOULD be equal to the <a
  href="https://developer.apple.com/documentation/uikit/uidevice/identifierforvendor">vendor
  identifier</a>. <p> For Android, examples of @code app.installation.id @endcode implementations
  include: <ul> <li><a
  href="https://firebase.google.com/docs/projects/manage-installations">Firebase Installation
  ID</a>.</li> <li>A globally unique UUID which is persisted across sessions in your
  application.</li> <li><a href="https://developer.android.com/identity/app-set-id">App set
  ID</a>.</li> <li><a
  href="https://developer.android.com/reference/android/provider/Settings.Secure#ANDROID_ID">@code
  Settings.getString(Settings.Secure.ANDROID_ID) @endcode</a>.</li>
  </ul>
  <p>
  More information about Android identifier best practices can be found <a
  href="https://developer.android.com/training/articles/user-data-ids">here</a>.
 */
static constexpr const char *kAppInstallationId = "app.installation.id";

/**
  The x (horizontal) coordinate of a screen coordinate, in screen pixels.
 */
static constexpr const char *kAppScreenCoordinateX = "app.screen.coordinate.x";

/**
  The y (vertical) component of a screen coordinate, in screen pixels.
 */
static constexpr const char *kAppScreenCoordinateY = "app.screen.coordinate.y";

/**
  An identifier that uniquely differentiates this widget from other widgets in the same application.
  <p>
  A widget is an application component, typically an on-screen visual GUI element.
 */
static constexpr const char *kAppWidgetId = "app.widget.id";

/**
  The name of an application widget.
  <p>
  A widget is an application component, typically an on-screen visual GUI element.
 */
static constexpr const char *kAppWidgetName = "app.widget.name";

}  // namespace app
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
