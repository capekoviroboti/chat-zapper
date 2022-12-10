/**
 * ChatZapper.ino
 *
 * Interact with a physical object over YouTube superchats via a relay.
 */
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define BAUDRATE 115200         // baudrate for serial connection
#define RELAY_PIN 4             // board pin `P4` --> relay `IN`
#define WARNING_LED_PIN 5       // board pin `P5`
#define BUTTON_PIN 21           // board pin `P21`
#define SLEEP_MS 100            // 100ms core loop time
#define FLICKER_MS 500          // 0.5s alternating
#define LED_BLINK_MS 300        // 0.3s alternating
#define LED_BLINK_CNT 6         // 6 --> 3 on, 3 off = 1800ms
#define RELAY_OPERATION_MS 4000 // 4s total operation time

/**
 * WIFI
 */
char WIFI_SSID[] = "<ssid>";
char WIFI_PASSWORD[] = "<password>";
WiFiClientSecure client;

/**
 * Define an enum for verbosity from silent to maximally verbose.
 */
typedef enum {
    SILENT,
    NORMAL,
    WHINY,
    SHREW,
} verbosity_t;

/**
 * Define an enum for relay operation.
 * STATIC = on/off only
 * FLICKER = flickering
 */
typedef enum {
    STATIC,
    FLICKER,
} relay_op_t;

/**
 * CONSTANTS & GLOBALS:
 */
static const int SERIAL_CONFIG = SERIAL_8N1;
const verbosity_t DEBUG_LEVEL = SHREW; // max verbosity
const relay_op_t RELAY_OP_TYPE = STATIC;
bool RELAY_PIN_ON = false;
int LOOP_COUNTER = 0;

/**
 * Print debug info if the required level is met by DEBUG_LEVEL.
 */
void print_debug_info(String s, verbosity_t required_verbosity) {
    if (DEBUG_LEVEL >= required_verbosity) {
        Serial.println(s);
    }
}

/**
 * wait()
 * controls timing of relay, sleeping, etc. mostly a wrapper to
 * add logging to `delay`.
 */
void wait(int wait_time) {
    print_debug_info("Waiting for " + String(wait_time) + "ms", SHREW);
    delay(wait_time);
}

/**
 * flip_relay_pin()
 * invert HIGH and LOW on the RELAY_PIN given the current
 * state of RELAY_PIN_ON.
 */
void flip_relay_pin() {
    String relay_new_state = RELAY_PIN_ON ? "OFF" : "ON";
    // note that the above logic is flipped since we haven't inverted RELAY_PIN_ON yet
    print_debug_info(
        "Flipping relay pin (" + String(RELAY_PIN) + ") to " + relay_new_state,
        NORMAL
    );
    RELAY_PIN_ON = !RELAY_PIN_ON; // invert
    digitalWrite(RELAY_PIN, RELAY_PIN_ON);
    print_debug_info("New relay state: " + String(RELAY_PIN_ON), WHINY);
}

/**
 * operate_relay()
 * operate the relay according to the relay_op_t specified.
 */
void operate_relay(relay_op_t relay_op_type) {
    if (relay_op_type == FLICKER) {
        int elapsed_time_ms = 0;
        // Flicker the relay...
        while (elapsed_time_ms < RELAY_OPERATION_MS) {
            flip_relay_pin();
            elapsed_time_ms = elapsed_time_ms + FLICKER_MS;
            wait(FLICKER_MS);
        }
        // If we ended `ON`
        if (RELAY_PIN_ON) {
            flip_relay_pin();
        }
    } else {
        // STATIC
        flip_relay_pin();
        wait(RELAY_OPERATION_MS);
        flip_relay_pin();
    }
}

/**
 * Blink the warning light LED
 * num_blinks times for blink_time_ms milliseconds
 * each blink.
 */
void blink_warning_light(int num_blinks, int blink_time_ms) {
    print_debug_info(
        "Blinking warning led (" + String(WARNING_LED_PIN) + ") " + String(num_blinks) +
        " times for " + String(blink_time_ms) + "ms each",
        NORMAL
    );
    for (int i = 0; i <= num_blinks; i++) {
        digitalWrite(WARNING_LED_PIN, i % 2); // Alternate LOW and HIGH
        wait(blink_time_ms);
    }
}

/**
 * Check the state of the button.
 */
bool check_button_state(void) {
    return digitalRead(BUTTON_PIN) == HIGH;
}

bool check_youtube_api(void) {
    return false;
}

/**
 * setup()
 * does setup
 */
void setup() {
    /** SERIAL **/
    Serial.begin(BAUDRATE, SERIAL_CONFIG); // init serial
    while (!Serial) ; // wait for serial to come online
    /** RELAY **/
    print_debug_info(
        "Enabling RELAY_PIN " + String(RELAY_PIN),
        NORMAL
    );
    pinMode(RELAY_PIN, OUTPUT); // init relay pin (digital)
    /** LED **/
    print_debug_info(
        "Enabling WARNING_LED_PIN " + String(WARNING_LED_PIN),
        NORMAL
    );
    pinMode(WARNING_LED_PIN, OUTPUT); // init LED pin (digital)
    /** BUTTON **/
    print_debug_info(
        "Enabling BUTTON_PIN " + String(BUTTON_PIN),
        NORMAL
    );
    pinMode(BUTTON_PIN, INPUT_PULLUP); // init button pin
    /** WIFI **/
    WiFi.mode(WIFI_STA); // set wifi to station mode
    WiFi.disconnect();   // make sure we are in a clean state
    wait(100);           // wait 100ms to let things settle
    print_debug_info(
        "Enabling Wifi " + String(WIFI_SSID),
        NORMAL
    );
    print_debug_info(
        "Using Wifi password: " + String(WIFI_PASSWORD),
        SHREW
    );
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        print_debug_info("Waiting for wifi connection...", SHREW);
        wait(5000);
    }
    print_debug_info(
        "Wifi " + String(WIFI_SSID) + " connected successfully with IP " + String(WiFi.localIP()),
        NORMAL
    );
    client.setInsecure();
    print_debug_info(
        "setup() complete",
        NORMAL
    );
}

/**
 * loop()
 * main fn
 */
void loop() {
    wait(SLEEP_MS);
    if (check_button_state()) {
        // if the button is depressed operate the relay
        // after flashing the warning light
        blink_warning_light(LED_BLINK_CNT, LED_BLINK_MS);
        operate_relay(FLICKER);
    }
    if (LOOP_COUNTER % 50 == 0) {
        // use this to make sure we only do API lookups every 5s or so:
        if (check_youtube_api()) {
            // Operate the warning LED to give the use a warning
            blink_warning_light(LED_BLINK_CNT, LED_BLINK_MS);
            operate_relay(FLICKER);
        }
        LOOP_COUNTER = 0; // reset the counter so we never overflow
    }
    LOOP_COUNTER++;
}
