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

static void testAudioOnlyMediaStreamPlayback(WebViewTest* test, gconstpointer)
{
    auto* settings = webkit_web_view_get_settings(test->webView());
    webkit_settings_set_enable_media_stream(settings, TRUE);
    webkit_settings_set_enable_webaudio(settings, TRUE);
    webkit_settings_set_media_playback_requires_user_gesture(settings, FALSE);
    test->loadHtml("<html><body></body></html>", nullptr);
    test->waitUntilLoadFinished();

    GUniqueOutPtr<GError> error;
    auto* result = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished(
        // A WebAudio destination provides a live audio-only MediaStream without touching any audio device.
        "const context = new AudioContext();"
        "await context.resume();"
        "const destination = context.createMediaStreamDestination();"
        "const gain = context.createGain();"
        "gain.gain.value = 0;"
        "const oscillator = context.createOscillator();"
        "oscillator.connect(gain).connect(destination);"
        "oscillator.start();"
        "const audio = document.createElement('audio');"
        "document.body.append(audio);"
        // The resource selection task queues loadstart right before it calls loadResource(), which is
        // what creates the GStreamer MediaStream pipeline and requests PLAYING. Awaiting the event
        // therefore guarantees that the backend load has run, unlike a synchronous delay here, which
        // would only postpone the queued load. Whether the element has already published readiness
        // for that backend is exactly what differs between engines.
        "const loadStarted = new Promise(resolve => audio.addEventListener('loadstart', () => resolve(true), { once: true }));"
        "audio.srcObject = destination.stream;"
        "try {"
        "    const loaded = await Promise.race([loadStarted, new Promise(resolve => setTimeout(() => resolve(false), 5000))]);"
        "    if (!loaded)"
        "        return false;"
        "    const playing = new Promise(resolve => audio.addEventListener('playing', () => resolve(true), { once: true }));"
        "    const playPromise = audio.play();"
        // Keep the page main thread busy: this lets backend work proceed while main-thread state
        // notifications and the media-session admission continuation stay queued. It does not
        // guarantee that the pipeline reaches PLAYING, nor any particular dispatch order.
        "    const deadline = performance.now() + 1000;"
        "    while (performance.now() < deadline) { }"
        // Behavioral check on the element's exposed timeline only. MediaStream currentTime() is
        // synthetic (monotonic time since play(), or the cached paused time), so a positive value
        // here does not show that the backend started or rendered anything.
        "    const timelineAdvanced = audio.currentTime > 0;"
        "    const settled = await Promise.race([playPromise.then(() => true, () => false), new Promise(resolve => setTimeout(() => resolve(false), 5000))]);"
        "    const notified = await Promise.race([playing, new Promise(resolve => setTimeout(() => resolve(false), 5000))]);"
        "    return settled && notified && timelineAdvanced && !audio.paused && audio.readyState >= HTMLMediaElement.HAVE_FUTURE_DATA;"
        "} finally {"
        "    oscillator.stop();"
        "    destination.stream.getTracks().forEach(track => track.stop());"
        "    audio.srcObject = null;"
        "    audio.remove();"
        "    await context.close();"
        "}", nullptr, nullptr, &error.outPtr());
    g_assert_no_error(error.get());
    g_assert_nonnull(result);
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(result));
    g_assert_true(jsc_value_to_boolean(result));
}

static void testAudioOnlyMediaStreamStartupPause(WebViewTest* test, gconstpointer)
{
    auto* settings = webkit_web_view_get_settings(test->webView());
    webkit_settings_set_enable_media_stream(settings, TRUE);
    webkit_settings_set_enable_webaudio(settings, TRUE);
    webkit_settings_set_media_playback_requires_user_gesture(settings, FALSE);
    test->loadHtml("<html><body></body></html>", nullptr);
    test->waitUntilLoadFinished();

    GUniqueOutPtr<GError> error;
    auto* result = test->runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished(
        "const context = new AudioContext();"
        "await context.resume();"
        "const destination = context.createMediaStreamDestination();"
        "const gain = context.createGain();"
        "gain.gain.value = 0;"
        "const oscillator = context.createOscillator();"
        "oscillator.connect(gain).connect(destination);"
        "oscillator.start();"
        "const audio = document.createElement('audio');"
        "document.body.append(audio);"
        // Await loadstart so MediaPlayerPrivateGStreamer::load(MediaStreamPrivate&) has created the
        // GStreamer MediaStream pipeline and requested PLAYING, then pause during that startup phase,
        // while the element has not acknowledged playback by playing.
        "const loadStarted = new Promise(resolve => audio.addEventListener('loadstart', () => resolve(true), { once: true }));"
        "audio.srcObject = destination.stream;"
        "try {"
        "    const loaded = await Promise.race([loadStarted, new Promise(resolve => setTimeout(() => resolve(false), 5000))]);"
        "    if (!loaded)"
        "        return false;"
        "    const playPromise = audio.play();"
        // Keep the page main thread busy: this lets backend work proceed while main-thread state
        // notifications and the media-session admission continuation stay queued, so the explicit
        // pause below races an in-flight startup. It does not guarantee any particular pipeline state
        // or dispatch order.
        "    const deadline = performance.now() + 1000;"
        "    while (performance.now() < deadline) { }"
        // Behavioral check on the element's exposed timeline only. MediaStream currentTime() is
        // synthetic (monotonic time since play(), or the cached paused time), so the value read here
        // does not show that the backend started or rendered anything.
        "    const timelineAdvanced = audio.currentTime > 0;"
        "    audio.pause();"
        "    const settled = await Promise.race([playPromise.then(() => true, () => true), new Promise(resolve => setTimeout(() => resolve(false), 5000))]);"
        "    await new Promise(resolve => setTimeout(resolve, 500));"
        "    const position = audio.currentTime;"
        "    await new Promise(resolve => setTimeout(resolve, 300));"
        // The element's exposed timeline must stay frozen while it is paused. This is the paused-time
        // invariant of the media element, not evidence that a sink stopped.
        "    return settled && timelineAdvanced && audio.paused && Math.abs(audio.currentTime - position) < 0.1;"
        "} finally {"
        "    oscillator.stop();"
        "    destination.stream.getTracks().forEach(track => track.stop());"
        "    audio.srcObject = null;"
        "    audio.remove();"
        "    await context.close();"
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
    WebViewTest::add("WebKitMediaStream", "audio-only-playback", testAudioOnlyMediaStreamPlayback);
    WebViewTest::add("WebKitMediaStream", "audio-only-startup-pause", testAudioOnlyMediaStreamStartupPause);
}

void afterAll()
{
}
