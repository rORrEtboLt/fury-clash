/*
 * platform_ios.m — iOS platform implementation.
 *
 * Compiled only when FC_PLATFORM_IOS is defined (set by CMake iOS build).
 * Provides: safe area insets, screen DPI, tablet detection, Core Haptics.
 */

#import <UIKit/UIKit.h>
#import <CoreHaptics/CoreHaptics.h>
#import <TargetConditionals.h>
#include "platform.h"

/* ── Haptic engine (lazily initialised) ─────────────────────────────────── */
static CHHapticEngine *g_haptic = nil;

static void haptic_init(void)
{
    if (@available(iOS 13.0, *)) {
        if (CHHapticEngine.capabilitiesForHardware.supportsHaptics) {
            NSError *err = nil;
            g_haptic = [[CHHapticEngine alloc] initAndReturnError:&err];
            if (!err) {
                [g_haptic startAndReturnError:nil];
                /* Auto-restart engine if it stops (e.g. audio session interruption) */
                g_haptic.stoppedHandler = ^(CHHapticEngineStoppedReason reason) {
                    [g_haptic startAndReturnError:nil];
                };
            }
        }
    }
}

static void play_haptic(float intensity, float sharpness)
{
    if (@available(iOS 13.0, *)) {
        if (!g_haptic) return;
        CHHapticEventParameter *pi = [[CHHapticEventParameter alloc]
            initWithParameterID:CHHapticEventParameterIDHapticIntensity
                          value:intensity];
        CHHapticEventParameter *ps = [[CHHapticEventParameter alloc]
            initWithParameterID:CHHapticEventParameterIDHapticSharpness
                          value:sharpness];
        CHHapticEvent *ev = [[CHHapticEvent alloc]
            initWithEventType:CHHapticEventTypeHapticTransient
                   parameters:@[pi, ps]
                 relativeTime:0];
        NSError *err = nil;
        CHHapticPattern *pat = [[CHHapticPattern alloc]
            initWithEvents:@[ev] parameters:@[] error:&err];
        if (!err) {
            id<CHHapticPatternPlayer> player =
                [g_haptic createPlayerWithPattern:pat error:&err];
            if (!err) [player startAtTime:0 error:nil];
        }
    }
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void platform_init(PlatformInfo *info)
{
    memset(info, 0, sizeof(*info));
    info->platform = "ios";

    /* Screen size and pixel density */
    UIScreen *screen   = [UIScreen mainScreen];
    CGFloat   scale    = screen.scale;
    CGSize    pts      = screen.bounds.size;

    info->screen_w   = (float)(pts.width  * scale);
    info->screen_h   = (float)(pts.height * scale);
    info->screen_dpi = 96.0f * (float)scale;   /* approximate PPI proxy */

    /* Tablet detection */
    info->is_tablet  = (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) ? 1 : 0;

    /* Safe area — must be read from the key window */
    UIWindow *window = nil;
    if (@available(iOS 13.0, *)) {
        for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
            if ([scene isKindOfClass:[UIWindowScene class]]) {
                UIWindowScene *ws = (UIWindowScene *)scene;
                window = ws.keyWindow ?: ws.windows.firstObject;
                if (window) break;
            }
        }
    } else {
        window = UIApplication.sharedApplication.keyWindow;
    }

    if (window) {
        UIEdgeInsets sa          = window.safeAreaInsets;
        info->safe_area_top      = (float)(sa.top    * scale);
        info->safe_area_bottom   = (float)(sa.bottom * scale);
        info->safe_area_left     = (float)(sa.left   * scale);
        info->safe_area_right    = (float)(sa.right  * scale);
        /* Notch / Dynamic Island: safe-area top > ~20pt */
        info->has_notch          = (sa.top > 20.0f) ? 1 : 0;
    }

    /* Init haptics */
    haptic_init();
}

void platform_open_url(const char *url)
{
    NSURL *nsurl = [NSURL URLWithString:
        [NSString stringWithUTF8String:(url ? url : "")]];
    if (!nsurl) return;
    if (@available(iOS 10.0, *)) {
        [UIApplication.sharedApplication openURL:nsurl
                                         options:@{}
                               completionHandler:nil];
    }
}

void platform_haptic_light(void)  { play_haptic(0.4f, 0.4f); }
void platform_haptic_heavy(void)  { play_haptic(1.0f, 0.8f); }
