/*
 * Copyright (C) 2026 monadix
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
