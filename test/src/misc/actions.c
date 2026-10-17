// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * actions.c: key actions test
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *****************************************************************************/

#include "../../libvlc/test.h"
#include "../lib/libvlc_internal.h"

#include <vlc_common.h>
#include <vlc_actions.h>
#include <vlc_threads.h>
#include <vlc_atomic.h>

/* The key each thread presses */
static thread_local uint_fast32_t pressed;

static atomic_uint delivered;
static atomic_bool mismatch;

static int
OnAction(vlc_object_t *obj, char const *var,
         vlc_value_t old, vlc_value_t cur, void *data)
{
    (void) obj; (void) var; (void) old; (void) data;

    /* the key behind this very action, even while another thread delivers
     * another one */
    if (cur.i_int == ACTIONID_VOL_UP && vlc_actions_get_action_key() != pressed)
        atomic_store(&mismatch, true);
    atomic_fetch_add(&delivered, 1);
    return VLC_SUCCESS;
}

#define PRESS_COUNT 2000

struct presser
{
    libvlc_int_t *vlc;
    uint_fast32_t key;
};

static void *
Press(void *data)
{
    struct presser *p = data;
    pressed = p->key;
    for (int i = 0; i < PRESS_COUNT; ++i)
        var_SetInteger(p->vlc, "key-pressed", p->key);
    return NULL;
}

int
main(void)
{
    test_init();

    libvlc_instance_t *instance = libvlc_new(test_defaults_nargs,
                                             test_defaults_args);
    assert(instance != NULL);
    libvlc_int_t *vlc = instance->p_libvlc_int;

    var_AddCallback(vlc, "key-action", OnAction, NULL);

    /* a key gives its action and itself */
    pressed = KEY_UP;
    var_SetInteger(vlc, "key-pressed", KEY_UP);
    assert(atomic_load(&delivered) == 1);
    assert(!atomic_load(&mismatch));
    assert(vlc_actions_get_action_key() == 0);

    /* an action sent without a key has none */
    pressed = 0;
    var_SetInteger(vlc, "key-action", ACTIONID_VOL_UP);
    assert(atomic_load(&delivered) == 2);
    assert(!atomic_load(&mismatch));

    /* Up and Ctrl+Up both turn the volume up, from two threads at once */
    struct presser plain = { vlc, KEY_UP };
    struct presser ctrl = { vlc, KEY_UP | KEY_MODIFIER_CTRL };
    vlc_thread_t th_plain, th_ctrl;
    int ret = vlc_clone(&th_plain, Press, &plain);
    assert(ret == 0);
    ret = vlc_clone(&th_ctrl, Press, &ctrl);
    assert(ret == 0);
    vlc_join(th_plain, NULL);
    vlc_join(th_ctrl, NULL);

    assert(atomic_load(&delivered) == 2 + 2 * PRESS_COUNT);
    assert(!atomic_load(&mismatch));

    var_DelCallback(vlc, "key-action", OnAction, NULL);
    libvlc_release(instance);
    return 0;
}
