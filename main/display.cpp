#include "states/states.h"
#include "trip.h"
#include "relays.h"
#include "bat.h"
#include "cu2.h"
#include "cu3.h"
#include "display.h"

static EventGroupHandle_t eventGroupHandle;

#if CONFIG_ION_CU2 || CONFIG_ION_CU3
static const int DISPLAY_UPDATE_BIT = BIT1;
#endif

void initDisplay() {
    eventGroupHandle = xEventGroupCreate();
}

void requestDisplayUpdate() {
#if CONFIG_ION_CU2 || CONFIG_ION_CU3
    xEventGroupSetBits(eventGroupHandle, DISPLAY_UPDATE_BIT);
#endif
}

static void displayUpdate(ion_state * state) {
#if CONFIG_ION_CU2
            showState(state->level, getLight(), state->speed, getTrip1(), getBatPercentage());
#elif CONFIG_ION_CU3
            showStateCu3(state->level, state->displayOn, getLight(), state->speed, getTrip1(), getTrip2());
#endif
}

bool handleDisplayUpdate(ion_state * state) {
#if CONFIG_ION_CU2
    if(cu2HandleDisplayUpdate()) {
        return true;
    }
#endif

#if CONFIG_ION_CU2 || CONFIG_ION_CU3
    EventBits_t bitsToCheck =  DISPLAY_UPDATE_BIT;

    EventBits_t bits = xEventGroupWaitBits(eventGroupHandle, bitsToCheck, false, false, 0);
    if((bits & DISPLAY_UPDATE_BIT) != 0) {
        xEventGroupClearBits(eventGroupHandle, DISPLAY_UPDATE_BIT);
        displayUpdate(state);
        return true;
    }

#endif
    return false;
}
