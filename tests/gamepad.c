//========================================================================
// Gamepad mapping parsing and state transform tests
// Copyright (c) Camilla Löwy <elmindreda@glfw.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================
//
// This test exercises the SDL_GameControllerDB mapping parser and the
// gamepad state transform headlessly on the null platform using simulated
// joysticks injected through the shared internal input API.
//
//========================================================================

#include "../src/internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int error_count;
static int last_error;
static int failures;

static void error_callback(int code, const char* description)
{
    error_count++;
    last_error = code;
    (void) description;
}

#define CHECK(cond) \
    do \
    { \
        if (!(cond)) \
        { \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

#define CHECK_FLOAT(actual, expected) \
    do \
    { \
        const float a = (actual); \
        const float e = (expected); \
        if (fabsf(a - e) > 1e-6f) \
        { \
            fprintf(stderr, "FAIL %s:%d: %s was %.9g, expected %.9g\n", \
                    __FILE__, __LINE__, #actual, (double) a, (double) e); \
            failures++; \
        } \
    } while (0)

// Allocates a simulated joystick and marks it connected through the same
// internal path used by the platform backends
//
static _GLFWjoystick* alloc_virtual(const char* guid,
                                    int axes, int buttons, int hats)
{
    _GLFWjoystick* js = _glfwAllocJoystick("Virtual Pad", guid,
                                           axes, buttons, hats);
    if (!js)
        return NULL;

    _glfwInputJoystick(js, GLFW_CONNECTED);
    return js;
}

static void free_virtual(_GLFWjoystick* js)
{
    _glfwInputJoystick(js, GLFW_DISCONNECTED);
    _glfwFreeJoystick(js);
}

// Returns the mapping stored for a GUID, or NULL when the line was rejected
//
static _GLFWmapping* find_mapping(const char* guid)
{
    int i;

    for (i = 0;  i < _glfw.mappingCount;  i++)
    {
        if (strcmp(_glfw.mappings[i].guid, guid) == 0)
            return _glfw.mappings + i;
    }

    return NULL;
}

// The independently derived transform, fixed for this test:
// out = clamp(input * scale + offset, -1, 1)
//
static float expected_axis(float input, int scale, int offset)
{
    float value = input * (float) scale + (float) offset;
    if (value < -1.f)
        value = -1.f;
    if (value > 1.f)
        value = 1.f;
    return value;
}

// Exercises a mapping covering every supported field, all three input types
// and every axis modifier combination
//
static void test_complete_mapping(void)
{
    static const char* const guid = "0102030405060708090a0b0c0d0e0f00";
    const char* line =
        "0102030405060708090a0b0c0d0e0f00,"
        "Virtual Complete Pad,"
        "platform:Linux,"
        "a:b0,b:b1,x:b2,y:b3,"
        "back:b4,start:b5,guide:b6,"
        "leftshoulder:b7,rightshoulder:b8,"
        "leftstick:b9,rightstick:b10,"
        "dpup:h1.1,dpright:h1.2,dpdown:h1.4,dpleft:h1.8,"
        "leftx:a0,lefty:a1~,rightx:+a2,righty:-a3~,"
        "lefttrigger:+a4,righttrigger:-a5";
    _GLFWmapping* m;
    _GLFWmapelement* e;

    error_count = 0;
    glfwUpdateGamepadMappings(line);
    CHECK(error_count == 0);

    m = find_mapping(guid);
    CHECK(m != NULL);
    if (!m)
        return;

    CHECK(strcmp(m->name, "Virtual Complete Pad") == 0);

    e = &m->buttons[GLFW_GAMEPAD_BUTTON_A];
    CHECK(e->type == _GLFW_JOYSTICK_BUTTON);
    CHECK(e->index == 0);

    e = &m->buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB];
    CHECK(e->type == _GLFW_JOYSTICK_BUTTON);
    CHECK(e->index == 10);

    e = &m->buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP];
    CHECK(e->type == _GLFW_JOYSTICK_HATBIT);
    CHECK(e->index == ((1 << 4) | GLFW_HAT_UP));
    e = &m->buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT];
    CHECK(e->index == ((1 << 4) | GLFW_HAT_RIGHT));
    e = &m->buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN];
    CHECK(e->index == ((1 << 4) | GLFW_HAT_DOWN));
    e = &m->buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT];
    CHECK(e->index == ((1 << 4) | GLFW_HAT_LEFT));

    e = &m->axes[GLFW_GAMEPAD_AXIS_LEFT_X];
    CHECK(e->type == _GLFW_JOYSTICK_AXIS);
    CHECK(e->index == 0);
    CHECK(e->axisScale == 1);
    CHECK(e->axisOffset == 0);

    e = &m->axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
    CHECK(e->index == 1);
    CHECK(e->axisScale == -1);
    CHECK(e->axisOffset == 0);

    e = &m->axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
    CHECK(e->index == 2);
    CHECK(e->axisScale == 2);
    CHECK(e->axisOffset == -1);

    e = &m->axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
    CHECK(e->index == 3);
    CHECK(e->axisScale == -2);
    CHECK(e->axisOffset == -1);

    e = &m->axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
    CHECK(e->index == 4);
    CHECK(e->axisScale == 2);
    CHECK(e->axisOffset == -1);

    e = &m->axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
    CHECK(e->index == 5);
    CHECK(e->axisScale == 2);
    CHECK(e->axisOffset == 1);
}

// Every malformed token must be rejected with an error instead of being
// silently consumed and dropped.  The public update call itself always
// succeeds; rejection is observable through the error callback and the
// absence of a stored mapping.
//
static void test_invalid_mappings(void)
{
    static const char* const bad[] =
    {
        // GUID contains a non-hexadecimal character
        "0002030405060708090a0b0c0d0e0f0g,Bad GUID Pad,a:b0",
        // No comma after a full-length GUID
        "f202030405060708090a0b0c0d0e0f00",
        // Unknown field name
        "1102030405060708090a0b0c0d0e0f00,Bad Field Pad,bogus:b0",
        // Unknown input type
        "2102030405060708090a0b0c0d0e0f00,Bad Type Pad,a:x0",
        // Hat missing the required dot separator
        "3102030405060708090a0b0c0d0e0f00,Bad Hat Pad,dpup:h01",
        // Hat with no index
        "4102030405060708090a0b0c0d0e0f00,Bad Hat Pad,dpup:h.1",
        // Hat mask that is not one of the SDL directional bits
        "5102030405060708090a0b0c0d0e0f00,Bad Hat Pad,dpup:h0.3",
        "5202030405060708090a0b0c0d0e0f00,Bad Hat Pad,dpup:h0.0",
        "5302030405060708090a0b0c0d0e0f00,Bad Hat Pad,dpup:h0.16",
        // Hat index out of the packed four-bit range
        "6102030405060708090a0b0c0d0e0f00,Bad Hat Pad,dpup:h16.1",
        // Axis and button index without a number
        "7102030405060708090a0b0c0d0e0f00,Bad Index Pad,a:a",
        "8102030405060708090a0b0c0d0e0f00,Bad Index Pad,a:b",
        // Index out of the packed eight-bit range
        "9102030405060708090a0b0c0d0e0f00,Bad Index Pad,a:a256",
        "a102030405060708090a0b0c0d0e0f00,Bad Index Pad,a:b256",
        // Garbage after an otherwise valid value
        "b102030405060708090a0b0c0d0e0f00,Trailing Pad,a:a0x",
        "c102030405060708090a0b0c0d0e0f00,Trailing Pad,a:b0z",
        "d102030405060708090a0b0c0d0e0f00,Trailing Pad,dpup:h0.1x",
        // More than one inversion modifier
        "e102030405060708090a0b0c0d0e0f00,Double Tilde Pad,a:a0~~",
        // Modifier in front of a non-axis input
        "f102030405060708090a0b0c0d0e0f00,Button Mod Pad,a:+b0",
        // Empty token in the middle of the mapping
        "f002030405060708090a0b0c0d0e0f00,Empty Token Pad,a:b0,,b:b1",
    };
    static const char* const good[] =
    {
        // Trailing comma and trailing empty token are tolerated
        "0302030405060708090a0b0c0d0e0f00,Trailing Comma Pad,a:b0,",
        // Half-axis modifier, including when combined with inversion
        "0402030405060708090a0b0c0d0e0f00,Half Axis Pad,a:+a0",
        "0502030405060708090a0b0c0d0e0f00,Half Axis Pad,a:-a1~",
        // All four valid hat masks
        "0602030405060708090a0b0c0d0e0f00,Hat Pad,dpup:h0.1",
        "0702030405060708090a0b0c0d0e0f00,Hat Pad,dpright:h0.2",
        "0802030405060708090a0b0c0d0e0f00,Hat Pad,dpdown:h0.4",
        "0902030405060708090a0b0c0d0e0f00,Hat Pad,dpleft:h0.8",
        // Highest packable indices
        "0a02030405060708090a0b0c0d0e0f00,Edge Pad,a:b255",
        "0b02030405060708090a0b0c0d0e0f00,Edge Pad,dpup:h15.8",
    };

    unsigned int i;

    for (i = 0;  i < sizeof(bad) / sizeof(bad[0]);  i++)
    {
        char guid[33];
        memcpy(guid, bad[i], 32);
        guid[32] = '\0';

        error_count = 0;
        last_error = GLFW_NO_ERROR;
        glfwUpdateGamepadMappings(bad[i]);
        if (find_mapping(guid))
        {
            fprintf(stderr, "FAIL: invalid mapping was accepted: %s\n", bad[i]);
            failures++;
        }
        CHECK(error_count > 0);
        CHECK(last_error == GLFW_INVALID_VALUE);
    }

    for (i = 0;  i < sizeof(good) / sizeof(good[0]);  i++)
    {
        char guid[33];
        memcpy(guid, good[i], 32);
        guid[32] = '\0';

        error_count = 0;
        glfwUpdateGamepadMappings(good[i]);
        if (!find_mapping(guid))
        {
            fprintf(stderr, "FAIL: valid mapping was rejected: %s\n", good[i]);
            failures++;
        }
        CHECK(error_count == 0);
    }
}

// Injects extreme axis values, including the asymmetric hardware extremes,
// and verifies the fixed transform against values derived independently
//
static void test_axis_extremes(void)
{
    static const char* const guid = "0c02030405060708090a0b0c0d0e0f00";
    const char* line =
        "0c02030405060708090a0b0c0d0e0f00,Virtual Axis Pad,"
        "a:b0,"
        "leftx:a0,lefty:a1~,rightx:+a2,righty:-a3~,"
        "lefttrigger:+a4,righttrigger:-a5";
    static const float inputs[] =
    {
        -1.f, -0.5f, 0.f, 0.5f, 1.f,
        // Asymmetric hardware extremes normalized by the platform backends
        -32768.f / 32768.f,
        32767.f  / 32767.5f
    };
    static const struct
    {
        int gamepad_axis;
        int source_axis;
        int scale;
        int offset;
    } axes[] =
    {
        { GLFW_GAMEPAD_AXIS_LEFT_X,         0,  1,  0 },
        { GLFW_GAMEPAD_AXIS_LEFT_Y,         1, -1,  0 },
        { GLFW_GAMEPAD_AXIS_RIGHT_X,        2,  2, -1 },
        { GLFW_GAMEPAD_AXIS_RIGHT_Y,        3, -2, -1 },
        { GLFW_GAMEPAD_AXIS_LEFT_TRIGGER,   4,  2, -1 },
        { GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER,  5,  2,  1 },
    };

    _GLFWjoystick* js;
    GLFWgamepadstate state;
    unsigned int i, j;

    error_count = 0;
    glfwUpdateGamepadMappings(line);
    CHECK(error_count == 0);

    js = alloc_virtual(guid, 6, 16, 0);
    CHECK(js != NULL);
    if (!js)
        return;

    CHECK(glfwJoystickIsGamepad(GLFW_JOYSTICK_1));

    for (i = 0;  i < sizeof(inputs) / sizeof(inputs[0]);  i++)
    {
        for (j = 0;  j < sizeof(axes) / sizeof(axes[0]);  j++)
        {
            memset(js->axes, 0, sizeof(float) * 6);
            js->axes[axes[j].source_axis] = inputs[i];

            if (!glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
            {
                fprintf(stderr, "FAIL: glfwGetGamepadState returned false\n");
                failures++;
                continue;
            }

            CHECK_FLOAT(state.axes[axes[j].gamepad_axis],
                        expected_axis(inputs[i],
                                      axes[j].scale,
                                      axes[j].offset));

            // The public contract guarantees the closed -1 to 1 range
            CHECK(state.axes[axes[j].gamepad_axis] >= -1.f);
            CHECK(state.axes[axes[j].gamepad_axis] <= 1.f);
        }
    }

    // An inverted full-range axis must keep its direction at both extremes
    memset(js->axes, 0, sizeof(float) * 6);
    js->axes[1] = -1.f;
    CHECK(glfwGetGamepadState(GLFW_JOYSTICK_1, &state));
    CHECK_FLOAT(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], 1.f);
    js->axes[1] = 1.f;
    CHECK(glfwGetGamepadState(GLFW_JOYSTICK_1, &state));
    CHECK_FLOAT(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], -1.f);

    free_virtual(js);
}

// Verifies hat bit to directional button semantics and that the centered
// state produces no dpad events
//
static void test_hat_dispatch(void)
{
    static const char* const guid = "0d02030405060708090a0b0c0d0e0f00";
    const char* line =
        "0d02030405060708090a0b0c0d0e0f00,Virtual Hat Pad,"
        "a:b0,"
        "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8";
    static const struct
    {
        char hat;
        int button;
    } directions[] =
    {
        { GLFW_HAT_UP,     GLFW_GAMEPAD_BUTTON_DPAD_UP },
        { GLFW_HAT_RIGHT,  GLFW_GAMEPAD_BUTTON_DPAD_RIGHT },
        { GLFW_HAT_DOWN,   GLFW_GAMEPAD_BUTTON_DPAD_DOWN },
        { GLFW_HAT_LEFT,   GLFW_GAMEPAD_BUTTON_DPAD_LEFT },
    };

    _GLFWjoystick* js;
    GLFWgamepadstate state;
    unsigned int i, j;

    error_count = 0;
    glfwUpdateGamepadMappings(line);
    CHECK(error_count == 0);

    js = alloc_virtual(guid, 1, 16, 1);
    CHECK(js != NULL);
    if (!js)
        return;

    for (i = 0;  i < sizeof(directions) / sizeof(directions[0]);  i++)
    {
        _glfwInputJoystickHat(js, 0, directions[i].hat);
        CHECK(glfwGetGamepadState(GLFW_JOYSTICK_1, &state));

        for (j = 0;  j < sizeof(directions) / sizeof(directions[0]);  j++)
            CHECK(state.buttons[directions[j].button] ==
                  (j == i ? GLFW_PRESS : GLFW_RELEASE));
    }

    // Diagonal hats set two bits and must press both mapped directions
    _glfwInputJoystickHat(js, 0, GLFW_HAT_RIGHT_UP);
    CHECK(glfwGetGamepadState(GLFW_JOYSTICK_1, &state));
    CHECK(state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS);
    CHECK(state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS);
    CHECK(state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_RELEASE);
    CHECK(state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_RELEASE);

    _glfwInputJoystickHat(js, 0, GLFW_HAT_CENTERED);
    CHECK(glfwGetGamepadState(GLFW_JOYSTICK_1, &state));
    for (i = 0;  i < sizeof(directions) / sizeof(directions[0]);  i++)
        CHECK(state.buttons[directions[i].button] == GLFW_RELEASE);

    free_virtual(js);
}

// Mappings without a platform token and mappings carrying any platform token
// are both accepted by the null platform without errors
//
static void test_platform_handling(void)
{
    static const char* const no_platform = "0e02030405060708090a0b0c0d0e0f00";
    static const char* const with_platform = "0f02030405060708090a0b0c0d0e0f00";

    error_count = 0;
    glfwUpdateGamepadMappings("0e02030405060708090a0b0c0d0e0f00,"
                              "No Platform Pad,a:b0");
    CHECK(error_count == 0);
    CHECK(find_mapping(no_platform) != NULL);

    error_count = 0;
    glfwUpdateGamepadMappings("0f02030405060708090a0b0c0d0e0f00,"
                              "Platform Pad,platform:Linux,a:b0");
    CHECK(error_count == 0);
    CHECK(find_mapping(with_platform) != NULL);
}

// The raw joystick path must keep working independently of gamepad mappings
//
static void test_bare_joystick(void)
{
    static const char* const guid = "1002030405060708090a0b0c0d0e0f00";
    _GLFWjoystick* js;
    int count = 0;
    const float* axes;
    const unsigned char* buttons;
    const unsigned char* hats;

    js = alloc_virtual(guid, 3, 4, 1);
    CHECK(js != NULL);
    if (!js)
        return;

    CHECK(!glfwJoystickIsGamepad(GLFW_JOYSTICK_1));

    _glfwInputJoystickAxis(js, 0, -1.f);
    _glfwInputJoystickAxis(js, 1, 0.25f);
    _glfwInputJoystickAxis(js, 2, 1.f);
    _glfwInputJoystickButton(js, 1, GLFW_PRESS);
    _glfwInputJoystickHat(js, 0, GLFW_HAT_LEFT_DOWN);

    axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count);
    CHECK(axes != NULL);
    CHECK(count == 3);
    CHECK_FLOAT(axes[0], -1.f);
    CHECK_FLOAT(axes[1], 0.25f);
    CHECK_FLOAT(axes[2], 1.f);

    buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &count);
    CHECK(buttons != NULL);
    // Hat buttons are included by default, giving four slots per hat
    CHECK(count == 4 + 1 * 4);
    CHECK(buttons[0] == GLFW_RELEASE);
    CHECK(buttons[1] == GLFW_PRESS);

    hats = glfwGetJoystickHats(GLFW_JOYSTICK_1, &count);
    CHECK(hats != NULL);
    CHECK(count == 1);
    CHECK(hats[0] == GLFW_HAT_LEFT_DOWN);

    free_virtual(js);
}

// A mapping whose indices exceed the simulated hardware must not be selected
//
static void test_out_of_range_hardware(void)
{
    static const char* const guid = "1202030405060708090a0b0c0d0e0f00";
    _GLFWjoystick* js;

    error_count = 0;
    glfwUpdateGamepadMappings("1202030405060708090a0b0c0d0e0f00,"
                              "Out Of Range Pad,a:b9");
    CHECK(error_count == 0);

    js = alloc_virtual(guid, 2, 2, 1);
    CHECK(js != NULL);
    if (!js)
        return;

    CHECK(!glfwJoystickIsGamepad(GLFW_JOYSTICK_1));

    free_virtual(js);
}

int main(void)
{
    glfwSetErrorCallback(error_callback);

    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
    if (!glfwInit())
    {
        fprintf(stderr, "FAIL: glfwInit failed\n");
        return EXIT_FAILURE;
    }

    // Every built-in SDL_GameControllerDB mapping has to parse cleanly
    CHECK(error_count == 0);

    test_complete_mapping();
    test_invalid_mappings();
    test_axis_extremes();
    test_hat_dispatch();
    test_platform_handling();
    test_bare_joystick();
    test_out_of_range_hardware();

    glfwTerminate();

    if (failures)
    {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    printf("All gamepad mapping tests passed\n");
    return EXIT_SUCCESS;
}
