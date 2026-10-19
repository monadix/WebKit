/*
 * Copyright (C) 2026 monadix
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "WebViewTest.h"
#include <wtf/glib/GLibExtras.h>

static void testHiddenMutedMediaStream(WebViewTest* test, gconstpointer)
{
    webkit_settings_set_enable_media_stream(webkit_web_view_get_settings(test->webView()), TRUE);
    test->showInWindow();
    test->loadHtml("<html><body></body></html>", nullptr);
    test->waitUntilLoadFinished();

    GUniqueOutPtr<GError> error;
    auto* result = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished(
        "const source = document.createElement('canvas');"
        "source.width = source.height = 16;"
        "const context = source.getContext('2d');"
        "context.fillStyle = '#00ff00';"
        "context.fillRect(0, 0, 16, 16);"
        "const video = document.createElement('video');"
        "video.muted = true;"
        "video.style.display = 'none';"
        "document.body.append(video);"
        "await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));"
        "const stream = source.captureStream();"
        "const timer = setInterval(() => context.fillRect(0, 0, 16, 16), 33);"
        "try {"
        "    const frame = new Promise(resolve => video.onloadeddata = () => resolve(true));"
        "    video.srcObject = stream;"
        "    video.play().catch(() => {});"
        "    const receivedFrame = await Promise.race([frame, new Promise(resolve => setTimeout(() => resolve(false), 5000))]);"
        "    if (!receivedFrame)"
        "        return false;"
        "    const output = document.createElement('canvas');"
        "    output.width = output.height = 16;"
        "    const outputContext = output.getContext('2d');"
        "    outputContext.drawImage(video, 0, 0);"
        "    const pixel = outputContext.getImageData(8, 8, 1, 1).data;"
        "    return pixel[0] < 10 && pixel[1] > 240 && pixel[2] < 10 && pixel[3] === 255;"
        "} finally {"
        "    clearInterval(timer);"
        "    stream.getTracks().forEach(track => track.stop());"
        "    video.remove();"
        "}", nullptr, nullptr, &error.outPtr());
    g_assert_no_error(error.get());
    g_assert_nonnull(result);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(result));
    g_assert_true(jsc_value_to_boolean(result));
}

void beforeAll()
{
    // The GLib test runner normally disables the suspension behavior under test.
    g_setenv("WEBKIT_GST_ALLOW_PLAYBACK_OF_INVISIBLE_VIDEOS", "0", TRUE);
    WebViewTest::add("WebKitMediaStream", "hidden-muted-playback", testHiddenMutedMediaStream);
}

void afterAll()
{
}
